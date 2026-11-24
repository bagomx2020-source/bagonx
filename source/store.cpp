#include "store.hpp"
#include "config.hpp"
#include "i18n.hpp"
#include "json.hpp"
#include "net.hpp"
#include "util.hpp"
#include "zip.hpp"
#include <algorithm>
#include <map>
#include <cstdio>
#include <dirent.h>
#include <unistd.h>

namespace store {

namespace {

const char* kBase = "https://switch.cdn.fortheusers.org";
// Mismo registro que usa la Homebrew App Store, así ambas apps se entienden.
const std::string kPkgDir = "sdmc:/switch/appstore/.get/packages/";

bool isMetaFile(const std::string& n) {
    return n == "info.json" || n == "manifest.install" || n == "icon.png" || n == "screen.png";
}

} // namespace

bool fetch(std::vector<Pkg>& out, std::string& err) {
    std::string body;
    if (!net::get(std::string(kBase) + "/repo.json", body, &err)) return false;
    json::Value v;
    if (!json::parse(body, v)) { err = "repo.json no válido"; return false; }
    const json::Value& arr = v["packages"];
    if (!arr.isArray()) { err = "repo.json sin paquetes"; return false; }
    for (size_t i = 0; i < arr.size(); i++) {
        const json::Value& p = arr.at(i);
        Pkg k;
        k.name = p["name"].str();
        if (k.name.empty() || k.name.find('/') != std::string::npos || k.name.find("..") != std::string::npos) continue;
        k.title = p["title"].str(k.name);
        k.author = p["author"].str();
        k.version = p["version"].str();
        k.category = p["category"].str();
        k.desc = p["description"].str();
        k.details = p["details"].str();
        k.sizeKB = (uint64_t)p["filesize"].num();
        out.push_back(std::move(k));
    }
    std::sort(out.begin(), out.end(), [](const Pkg& a, const Pkg& b) {
        return util::lower(a.title) < util::lower(b.title);
    });
    refreshInstalled(out);
    return true;
}

void refreshInstalled(std::vector<Pkg>& pkgs) {
    std::map<std::string, std::string> installed;
    DIR* d = opendir(kPkgDir.c_str());
    if (d) {
        while (dirent* e = readdir(d)) {
            std::string name = e->d_name;
            if (name == "." || name == "..") continue;
            std::string s;
            json::Value v;
            if (util::readFile(kPkgDir + name + "/info.json", s) && json::parse(s, v))
                installed[name] = v["version"].str("?");
            else if (util::exists(kPkgDir + name + "/manifest.install"))
                installed[name] = "?";
        }
        closedir(d);
    }
    for (auto& p : pkgs) {
        auto it = installed.find(p.name);
        p.installedVer = it == installed.end() ? "" : it->second;
    }
}

bool install(const Pkg& p, TaskCtx& c, std::string& msg) {
    std::string err;
    std::string zipPath = std::string(TMP_DIR) + "/" + p.name + ".zip";
    c.reset();
    c.setStatus(util::fmt(tr("downloading"), p.title));
    if (!net::download(std::string(kBase) + "/zips/" + p.name + ".zip", zipPath, c.dlProgress(), &err)) {
        msg = c.cancel ? tr("cancelled") : err;
        return false;
    }

    // manifest.install: líneas "U: ruta" (sobrescribir) o "G: ruta" (solo si no existe)
    std::string manifest;
    zip::readEntry(zipPath, "manifest.install", manifest);
    std::map<std::string, char> modes;
    for (auto& line : util::lines(manifest))
        if (line.size() > 3 && line[1] == ':') modes[util::trim(line.substr(2))] = line[0];

    c.reset();
    c.setStatus(util::fmt(tr("extracting"), p.title));
    std::vector<std::string> written;
    bool ok = zip::extract(zipPath, [&modes](const std::string& n) -> std::string {
        std::string dest = "sdmc:/" + n;
        if (!modes.empty()) {
            auto it = modes.find(n);
            if (it == modes.end()) return "";
            if ((it->second == 'G' || it->second == 'L') && util::exists(dest)) return "";
            return dest;
        }
        return isMetaFile(n) ? "" : dest;
    }, c.exProgress(), &written, err);
    remove(zipPath.c_str());
    if (!ok) {
        msg = c.cancel ? tr("cancelled") : err;
        return false;
    }

    std::string dir = kPkgDir + p.name;
    util::mkdirs(dir);
    if (manifest.empty())
        for (auto& w : written) manifest += "U: " + w.substr(6) + "\n"; // quita "sdmc:/"
    util::writeFile(dir + "/manifest.install", manifest);
    util::writeFile(dir + "/info.json",
                    "{\"title\":\"" + json::escape(p.title) + "\",\"version\":\"" + json::escape(p.version) +
                    "\",\"author\":\"" + json::escape(p.author) + "\",\"category\":\"" + json::escape(p.category) + "\"}\n");
    msg = util::fmt(tr("app_installed_msg"), p.title);
    return true;
}

bool uninstall(const Pkg& p, std::string& msg) {
    std::string dir = kPkgDir + p.name;
    std::string manifest;
    if (!util::readFile(dir + "/manifest.install", manifest)) {
        msg = "manifest.install no encontrado";
        return false;
    }
    for (auto& line : util::lines(manifest)) {
        if (line.size() <= 3 || line[1] != ':') continue;
        std::string rel = util::trim(line.substr(2));
        if (rel.empty() || rel.find("..") != std::string::npos) continue;
        std::string file = "sdmc:/" + rel;
        remove(file.c_str());
        // Borra carpetas vacías, pero nunca las de primer nivel (sdmc:/switch, sdmc:/atmosphere…)
        std::string d = util::parent(file);
        while (d.size() > 6 && d.substr(6).find('/') != std::string::npos) {
            if (rmdir(d.c_str()) != 0) break;
            d = util::parent(d);
        }
    }
    for (const char* f : {"/info.json", "/manifest.install", "/icon.png", "/screen.png"})
        remove((dir + f).c_str());
    rmdir(dir.c_str());
    msg = util::fmt(tr("app_removed_msg"), p.title);
    return true;
}

} // namespace store
