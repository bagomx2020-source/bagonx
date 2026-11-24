#include "net.hpp"
#include "json.hpp"
#include "util.hpp"
#include <curl/curl.h>
#include <cstdio>

namespace net {

static const char* kUserAgent = APP_TITLE "/" APP_VERSION " (Nintendo Switch homebrew)";

void init() { curl_global_init(CURL_GLOBAL_DEFAULT); }
void exit() { curl_global_cleanup(); }

static size_t writeString(char* ptr, size_t size, size_t n, void* user) {
    static_cast<std::string*>(user)->append(ptr, size * n);
    return size * n;
}

static size_t writeFile(char* ptr, size_t size, size_t n, void* user) {
    return fwrite(ptr, size, n, static_cast<FILE*>(user)) * size;
}

static int xferInfo(void* user, curl_off_t dlTotal, curl_off_t dlNow, curl_off_t, curl_off_t) {
    Progress* cb = static_cast<Progress*>(user);
    if (cb && *cb && !(*cb)((double)dlNow, (double)dlTotal)) return 1; // cancelar
    return 0;
}

static void setCommon(CURL* c, const std::string& url) {
    curl_easy_setopt(c, CURLOPT_URL, url.c_str());
    curl_easy_setopt(c, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(c, CURLOPT_USERAGENT, kUserAgent);
    curl_easy_setopt(c, CURLOPT_CONNECTTIMEOUT, 15L);
    curl_easy_setopt(c, CURLOPT_LOW_SPEED_LIMIT, 1L);
    curl_easy_setopt(c, CURLOPT_LOW_SPEED_TIME, 30L);
    curl_easy_setopt(c, CURLOPT_FAILONERROR, 1L);
    curl_easy_setopt(c, CURLOPT_ACCEPT_ENCODING, "");
    curl_easy_setopt(c, CURLOPT_BUFFERSIZE, 256L * 1024L);
}

static std::string describe(CURLcode rc, long http) {
    if (rc == CURLE_ABORTED_BY_CALLBACK) return "cancel";
    std::string s = curl_easy_strerror(rc);
    if (http) s += " (HTTP " + std::to_string(http) + ")";
    return s;
}

bool get(const std::string& url, std::string& out, std::string* err, bool github) {
    out.clear();
    CURL* c = curl_easy_init();
    if (!c) { if (err) *err = "curl"; return false; }
    setCommon(c, url);
    curl_slist* headers = nullptr;
    if (github) {
        headers = curl_slist_append(headers, "Accept: application/vnd.github+json");
        curl_easy_setopt(c, CURLOPT_HTTPHEADER, headers);
    }
    curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, writeString);
    curl_easy_setopt(c, CURLOPT_WRITEDATA, &out);
    CURLcode rc = curl_easy_perform(c);
    long http = 0;
    curl_easy_getinfo(c, CURLINFO_RESPONSE_CODE, &http);
    curl_easy_cleanup(c);
    if (headers) curl_slist_free_all(headers);
    if (rc != CURLE_OK) { if (err) *err = describe(rc, http); return false; }
    return true;
}

bool download(const std::string& url, const std::string& path, Progress cb, std::string* err) {
    std::string part = path + ".part";
    util::mkdirs(util::parent(path));
    FILE* f = fopen(part.c_str(), "wb");
    if (!f) { if (err) *err = "No se pudo crear " + path; return false; }
    std::vector<char> vbuf(512 * 1024);
    setvbuf(f, vbuf.data(), _IOFBF, vbuf.size());

    CURL* c = curl_easy_init();
    if (!c) { fclose(f); remove(part.c_str()); if (err) *err = "curl"; return false; }
    setCommon(c, url);
    curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, writeFile);
    curl_easy_setopt(c, CURLOPT_WRITEDATA, f);
    curl_easy_setopt(c, CURLOPT_NOPROGRESS, 0L);
    curl_easy_setopt(c, CURLOPT_XFERINFOFUNCTION, xferInfo);
    curl_easy_setopt(c, CURLOPT_XFERINFODATA, &cb);
    CURLcode rc = curl_easy_perform(c);
    long http = 0;
    curl_easy_getinfo(c, CURLINFO_RESPONSE_CODE, &http);
    curl_easy_cleanup(c);
    fclose(f);

    if (rc != CURLE_OK) {
        remove(part.c_str());
        if (err) *err = describe(rc, http);
        return false;
    }
    remove(path.c_str());
    if (rename(part.c_str(), path.c_str()) != 0) {
        remove(part.c_str());
        if (err) *err = "No se pudo renombrar " + path;
        return false;
    }
    return true;
}

bool githubLatest(const std::string& repo, Release& out, std::string& err) {
    std::string body;
    if (!get("https://api.github.com/repos/" + repo + "/releases/latest", body, &err, true)) return false;
    json::Value v;
    if (!json::parse(body, v)) { err = "Respuesta JSON no válida"; return false; }
    out.tag = v["tag_name"].str();
    if (out.tag.empty()) { err = v["message"].str("GitHub no devolvió versiones"); return false; }
    const json::Value& assets = v["assets"];
    for (size_t i = 0; i < assets.size(); i++)
        out.assets.push_back({assets.at(i)["name"].str(), assets.at(i)["browser_download_url"].str()});
    return true;
}

} // namespace net
