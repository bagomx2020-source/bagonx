#include "lan.hpp"
#include "i18n.hpp"
#include "json.hpp"
#include "net.hpp"
#include "util.hpp"
#include <cctype>
#include <cstdio>
#include <map>
#include <sys/stat.h>
#include <sys/statvfs.h>

namespace lan {

namespace {

std::string urlEncode(const std::string& s) {
    static const char* hex = "0123456789ABCDEF";
    std::string o;
    for (unsigned char ch : s) {
        if (isalnum(ch) || ch == '-' || ch == '_' || ch == '.' || ch == '~' || ch == '/') {
            o += (char)ch;
        } else {
            o += '%';
            o += hex[ch >> 4];
            o += hex[ch & 15];
        }
    }
    return o;
}

bool safeRel(const std::string& p) {
    if (p.empty() || p[0] == '/' || p.find('\\') != std::string::npos || p.find(':') != std::string::npos) return false;
    size_t start = 0;
    while (start <= p.size()) {
        size_t end = p.find('/', start);
        if (end == std::string::npos) end = p.size();
        std::string part = p.substr(start, end - start);
        if (part.empty() || part == "." || part == "..") return false;
        start = end + 1;
    }
    return true;
}

uint64_t fileSize(const std::string& path) {
    struct stat st;
    return stat(path.c_str(), &st) == 0 ? (uint64_t)st.st_size : UINT64_MAX;
}

} // namespace

std::string baseUrl(const std::string& addr) {
    std::string a = util::trim(addr);
    if (util::startsWith(a, "http://")) a = a.substr(7);
    while (!a.empty() && a.back() == '/') a.pop_back();
    if (a.find(':') == std::string::npos) a += ":8686";
    return "http://" + a;
}

bool list(const std::string& addr, const std::string& pin, std::vector<Game>& out, std::string& err) {
    if (util::trim(addr).empty()) { err = tr("lan_no_addr"); return false; }
    std::string body;
    if (!net::get(baseUrl(addr) + "/bagonx/list?k=" + urlEncode(pin), body, &err)) {
        if (err.find("403") != std::string::npos) err = tr("lan_bad_pin");
        else err = util::fmt(tr("lan_unreachable"), baseUrl(addr)) + "\n\n" + err;
        return false;
    }
    json::Value v;
    if (!json::parse(body, v)) { err = "JSON no válido"; return false; }
    const json::Value& games = v["games"];
    for (size_t i = 0; i < games.size(); i++) {
        const json::Value& g = games.at(i);
        Game game;
        game.name = g["name"].str();
        if (game.name.empty() || !safeRel(game.name) || game.name.find('/') != std::string::npos) continue;
        const json::Value& files = g["files"];
        for (size_t j = 0; j < files.size(); j++) {
            File f;
            f.path = files.at(j)["path"].str();
            f.size = (uint64_t)files.at(j)["size"].num();
            f.sha256 = util::lower(files.at(j)["sha256"].str());
            if (!safeRel(f.path)) continue;
            game.size += f.size;
            game.files.push_back(std::move(f));
        }
        out.push_back(std::move(game));
    }
    return true;
}

Check checkManifest(const Game& g, const std::string& manifestPath) {
    Check r;
    std::string text;
    if (manifestPath.empty() || !util::readFile(manifestPath, text)) return r;
    r.hasManifest = true;
    // La microSD (FAT32/exFAT) no distingue mayúsculas: comparamos en minúsculas.
    std::map<std::string, const File*> have;
    for (auto& f : g.files) have[util::lower(f.path)] = &f;
    size_t used = 0;
    for (auto& line : util::lines(text)) {
        std::string l = util::trim(line);
        if (l.empty() || l[0] == '#') continue;
        size_t a = l.find_first_of(" \t");
        if (a == std::string::npos) continue;
        size_t b = l.find_first_not_of(" \t", a);
        size_t c = b == std::string::npos ? std::string::npos : l.find_first_of(" \t", b);
        size_t d = c == std::string::npos ? std::string::npos : l.find_first_not_of(" \t", c);
        if (d == std::string::npos) continue;
        std::string sha = util::lower(l.substr(0, a));
        uint64_t size = strtoull(l.substr(b, c - b).c_str(), nullptr, 10);
        std::string path = util::lower(l.substr(d));
        auto it = have.find(path);
        if (it == have.end()) { r.missing++; continue; }
        used++;
        if (it->second->size == size && it->second->sha256 == sha) r.ok++;
        else r.different++;
    }
    r.extra = (int)(g.files.size() - used);
    return r;
}

bool copyGame(const std::string& addr, const std::string& pin, const Game& g, const std::string& destDir,
              TaskCtx& c, std::string& msg) {
    std::string root = destDir;
    if (!util::endsWith(root, "/")) root += "/";
    util::mkdirs(root);

    struct statvfs st;
    if (statvfs("sdmc:/", &st) == 0) {
        uint64_t freeB = (uint64_t)st.f_bavail * st.f_frsize;
        if (freeB < g.size + 64ULL * 1024 * 1024) {
            msg = util::fmt(tr("lan_no_space"), util::humanSize(g.size));
            return false;
        }
    }

    std::string base = baseUrl(addr) + "/bagonx/file/" + urlEncode(g.name) + "/";
    std::string key = "?k=" + urlEncode(pin);
    uint64_t doneBefore = 0;
    int copied = 0, skipped = 0;
    for (size_t i = 0; i < g.files.size(); i++) {
        if (c.cancel) { msg = tr("cancelled"); return false; }
        const File& f = g.files[i];
        std::string dest = root + f.path;
        c.setStatus(util::fmt(tr("lan_copying"), std::to_string(i + 1) + "/" + std::to_string(g.files.size())) +
                    "  " + util::utf8Cut(f.path, 60));
        // Si ya está con el mismo tamaño, no se vuelve a copiar (permite reanudar).
        if (fileSize(dest) == f.size) {
            skipped++;
        } else {
            std::string err;
            uint64_t total = g.size;
            bool ok = net::download(base + urlEncode(f.path) + key, dest,
                                    [&c, doneBefore, total](double now, double) {
                                        c.done = doneBefore + (uint64_t)now;
                                        c.total = total;
                                        return !c.cancel.load();
                                    }, &err);
            if (!ok) { msg = c.cancel ? tr("cancelled") : f.path + "\n" + err; return false; }
            if (fileSize(dest) != f.size) { msg = util::fmt(tr("lan_size_bad"), f.path); return false; }
            copied++;
        }
        doneBefore += f.size;
        c.done = doneBefore;
        c.total = g.size;
    }
    msg = util::fmt(tr("lan_done"), g.name) + "\n" + util::fmt(tr("lan_done_count"), std::to_string(copied)) +
          (skipped ? "  ·  " + util::fmt(tr("lan_done_skipped"), std::to_string(skipped)) : "") + "\n" + root;
    return true;
}

} // namespace lan
