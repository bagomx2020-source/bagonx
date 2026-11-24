#include "cfw.hpp"
#include "config.hpp"
#include "i18n.hpp"
#include "net.hpp"
#include "util.hpp"
#include "zip.hpp"
#include <cstdio>

namespace cfw {

namespace {

bool findAsset(const net::Release& r, const std::function<bool(const std::string&)>& pred,
               std::string& name, std::string& url) {
    for (auto& a : r.assets)
        if (pred(a.first)) { name = a.first; url = a.second; return true; }
    return false;
}

bool fail(TaskCtx& c, std::string& msg, const std::string& err) {
    msg = c.cancel ? tr("cancelled") : err;
    return false;
}

} // namespace

bool installAtmosphere(TaskCtx& c, std::string& msg) {
    c.reset();
    c.setStatus(tr("querying"));
    net::Release r;
    std::string err;
    if (!net::githubLatest("Atmosphere-NX/Atmosphere", r, err)) return fail(c, msg, err);

    std::string zipName, zipUrl, fuseeName, fuseeUrl;
    if (!findAsset(r, [](const std::string& n) {
            return util::startsWith(n, "atmosphere-") && util::endsWith(n, ".zip");
        }, zipName, zipUrl)) {
        msg = util::fmt(tr("no_asset"), r.tag);
        return false;
    }
    findAsset(r, [](const std::string& n) { return n == "fusee.bin"; }, fuseeName, fuseeUrl);

    std::string zipPath = std::string(TMP_DIR) + "/atmosphere.zip";
    c.reset();
    c.setStatus(util::fmt(tr("downloading"), zipName));
    if (!net::download(zipUrl, zipPath, c.dlProgress(), &err)) return fail(c, msg, err);

    c.reset();
    c.setStatus(util::fmt(tr("extracting"), "Atmosphère " + r.tag));
    bool ok = zip::extract(zipPath, [](const std::string& n) { return "sdmc:/" + n; },
                           c.exProgress(), nullptr, err);
    remove(zipPath.c_str());
    if (!ok) return fail(c, msg, err);

    if (!fuseeUrl.empty()) {
        c.reset();
        c.setStatus(util::fmt(tr("downloading"), "fusee.bin"));
        if (!net::download(fuseeUrl, "sdmc:/bootloader/payloads/fusee.bin", c.dlProgress(), &err))
            return fail(c, msg, err);
    }
    msg = util::fmt(tr("ams_done"), r.tag);
    return true;
}

bool installHekate(TaskCtx& c, std::string& msg) {
    c.reset();
    c.setStatus(tr("querying"));
    net::Release r;
    std::string err;
    if (!net::githubLatest("CTCaer/hekate", r, err)) return fail(c, msg, err);

    std::string zipName, zipUrl;
    if (!findAsset(r, [](const std::string& n) {
            return util::startsWith(n, "hekate_ctcaer_") && util::endsWith(n, ".zip");
        }, zipName, zipUrl)) {
        msg = util::fmt(tr("no_asset"), r.tag);
        return false;
    }

    std::string zipPath = std::string(TMP_DIR) + "/hekate.zip";
    c.reset();
    c.setStatus(util::fmt(tr("downloading"), zipName));
    if (!net::download(zipUrl, zipPath, c.dlProgress(), &err)) return fail(c, msg, err);

    c.reset();
    c.setStatus(util::fmt(tr("extracting"), "Hekate " + r.tag));
    std::string payloadName;
    bool ok = zip::extract(zipPath, [&payloadName](const std::string& n) -> std::string {
        std::string dest = "sdmc:/" + n;
        // Nunca pisar la configuración del usuario
        if ((n == "bootloader/hekate_ipl.ini" || n == "bootloader/nyx.ini") && util::exists(dest)) return "";
        if (n.find('/') == std::string::npos && util::startsWith(n, "hekate_ctcaer_") && util::endsWith(n, ".bin"))
            payloadName = n;
        return dest;
    }, c.exProgress(), nullptr, err);
    remove(zipPath.c_str());
    if (!ok) return fail(c, msg, err);

    if (!payloadName.empty()) {
        std::string src = "sdmc:/" + payloadName;
        util::copyFile(src, "sdmc:/atmosphere/reboot_payload.bin");
        util::copyFile(src, "sdmc:/bootloader/update.bin");
    }
    msg = util::fmt(tr("hekate_done"), r.tag);
    return true;
}

bool installBoth(TaskCtx& c, std::string& msg) {
    std::string a, h;
    if (!installAtmosphere(c, a)) { msg = a; return false; }
    bool ok = installHekate(c, h);
    msg = a + "\n" + h;
    return ok;
}

} // namespace cfw
