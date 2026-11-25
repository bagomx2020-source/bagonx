#include "ports.hpp"
#include "config.hpp"
#include "i18n.hpp"
#include "json.hpp"
#include "net.hpp"
#include "util.hpp"
#include "zip.hpp"
#include <cstdio>

namespace ports {

namespace {

const std::string kVerDir = std::string(CONFIG_DIR) + "/ports/";

// Comparación con comodín '*' (cualquier texto).
bool glob(const char* p, const char* s) {
    if (*p == '\0') return *s == '\0';
    if (*p == '*') return glob(p + 1, s) || (*s && glob(p, s + 1));
    return *s && *p == *s && glob(p + 1, s + 1);
}

// El catálogo viene de internet: solo se aceptan rutas dentro de la microSD.
bool safePath(const std::string& p) {
    return util::startsWith(p, "sdmc:/") && p.find("..") == std::string::npos;
}

bool fail(TaskCtx& c, std::string& msg, const std::string& err) {
    msg = c.cancel ? tr("cancelled") : err;
    return false;
}

} // namespace

bool fetch(const std::string& repo, std::vector<Port>& out, std::string& err) {
    if (repo.empty()) { err = tr("repo_not_set"); return false; }
    std::string body;
    if (!net::get("https://raw.githubusercontent.com/" + repo + "/main/ports.json", body, &err)) return false;
    json::Value v;
    if (!json::parse(body, v)) { err = "ports.json no válido"; return false; }
    const json::Value& arr = v["ports"];
    if (!arr.isArray()) { err = "ports.json sin ports"; return false; }
    for (size_t i = 0; i < arr.size(); i++) {
        const json::Value& j = arr.at(i);
        Port p;
        p.id = j["id"].str();
        p.title = j["title"].str(p.id);
        p.system = j["system"].str();
        p.author = j["author"].str();
        p.repo = j["repo"].str();
        p.asset = j["asset"].str();
        p.type = j["type"].str();
        p.dest = j["dest"].str();
        p.check = j["check"].str();
        p.desc = j["desc"].str();
        p.dataDir = j["data_dir"].str();
        p.manifest = j["manifest"].str();
        p.needsData = j["needs_data"].type == json::Value::Bool && j["needs_data"].b;
        if (p.id.empty() || p.id.find('/') != std::string::npos || p.id.find("..") != std::string::npos) continue;
        if (p.repo.empty() || p.asset.empty() || (p.type != "nro" && p.type != "zip")) continue;
        if (!safePath(p.dest) || (!p.check.empty() && !safePath(p.check))) continue;
        if (!p.dataDir.empty() && !safePath(p.dataDir)) p.dataDir.clear();
        if (!p.manifest.empty() && !safePath(p.manifest)) p.manifest.clear();
        out.push_back(std::move(p));
    }
    refreshInstalled(out);
    return true;
}

void refreshInstalled(std::vector<Port>& list) {
    for (auto& p : list) {
        std::string ver;
        if (util::readFile(kVerDir + p.id + ".txt", ver)) ver = util::trim(ver);
        bool present = p.check.empty() ? !ver.empty() : util::exists(p.check);
        p.installedVer = present ? (ver.empty() ? "?" : ver) : "";
    }
}

bool install(const Port& p, TaskCtx& c, std::string& msg) {
    c.reset();
    c.setStatus(tr("querying"));
    net::Release r;
    std::string err;
    if (!net::githubLatest(p.repo, r, err)) return fail(c, msg, err);

    std::string name, url;
    for (auto& a : r.assets)
        if (glob(p.asset.c_str(), a.first.c_str())) { name = a.first; url = a.second; break; }
    if (url.empty()) { msg = util::fmt(tr("no_asset"), r.tag); return false; }

    c.reset();
    c.setStatus(util::fmt(tr("downloading"), name));
    if (p.type == "nro") {
        if (!net::download(url, p.dest, c.dlProgress(), &err)) return fail(c, msg, err);
    } else {
        std::string zipPath = std::string(TMP_DIR) + "/port.zip";
        if (!net::download(url, zipPath, c.dlProgress(), &err)) return fail(c, msg, err);
        c.reset();
        c.setStatus(util::fmt(tr("extracting"), p.title));
        std::string root = p.dest;
        if (!util::endsWith(root, "/")) root += "/";
        bool ok = zip::extract(zipPath, [&root](const std::string& n) { return root + n; },
                               c.exProgress(), nullptr, err);
        remove(zipPath.c_str());
        if (!ok) return fail(c, msg, err);
    }

    util::mkdirs(kVerDir);
    util::writeFile(kVerDir + p.id + ".txt", r.tag);
    msg = util::fmt(tr("port_done"), p.title + " " + r.tag);
    if (p.needsData) msg += "\n\n" + p.desc;
    return true;
}

} // namespace ports
