#include "updater.hpp"
#include "i18n.hpp"
#include "net.hpp"
#include "util.hpp"
#include <cstdio>
#include <cstring>

namespace updater {

bool check(const std::string& repo, std::string& tag, std::string& url, TaskCtx& c, std::string& msg) {
    c.setStatus(tr("querying"));
    net::Release r;
    if (!net::githubLatest(repo, r, msg)) return false;
    tag = r.tag;
    for (auto& a : r.assets)
        if (util::endsWith(a.first, ".nro")) { url = a.second; break; }
    return true;
}

bool apply(const std::string& url, const std::string& nroPath, TaskCtx& c, std::string& msg) {
    std::string tmp = nroPath + ".new";
    c.reset();
    c.setStatus(util::fmt(tr("downloading"), APP_TITLE));
    std::string err;
    if (!net::download(url, tmp, c.dlProgress(), &err)) {
        msg = c.cancel ? tr("cancelled") : err;
        return false;
    }
    // Comprueba la firma "NRO0" en el offset 0x10 antes de reemplazar
    char magic[4] = {0};
    FILE* f = fopen(tmp.c_str(), "rb");
    if (f) {
        fseek(f, 0x10, SEEK_SET);
        fread(magic, 1, 4, f);
        fclose(f);
    }
    if (memcmp(magic, "NRO0", 4) != 0) {
        remove(tmp.c_str());
        msg = tr("bad_nro");
        return false;
    }
    remove(nroPath.c_str());
    if (rename(tmp.c_str(), nroPath.c_str()) != 0) {
        msg = "No se pudo reemplazar " + nroPath;
        return false;
    }
    msg = tr("update_done");
    return true;
}

} // namespace updater
