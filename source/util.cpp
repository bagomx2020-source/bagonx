#include "util.hpp"
#include <sys/stat.h>
#include <cstdio>
#include <cstdlib>
#include <cctype>
#include <cerrno>

namespace util {

bool exists(const std::string& path) {
    struct stat st;
    return stat(path.c_str(), &st) == 0;
}

bool mkdirs(const std::string& dir) {
    if (dir.empty()) return false;
    size_t start = dir.find(":/");
    start = (start == std::string::npos) ? 1 : start + 2;
    for (size_t i = start; i < dir.size(); i++) {
        if (dir[i] == '/') {
            std::string sub = dir.substr(0, i);
            mkdir(sub.c_str(), 0777);
        }
    }
    if (mkdir(dir.c_str(), 0777) != 0 && errno != EEXIST) return exists(dir);
    return true;
}

std::string parent(const std::string& path) {
    size_t pos = path.find_last_of('/');
    if (pos == std::string::npos) return "";
    if (pos > 0 && path[pos - 1] == ':') return path.substr(0, pos + 1); // "sdmc:/"
    return path.substr(0, pos);
}

bool readFile(const std::string& path, std::string& out) {
    FILE* f = fopen(path.c_str(), "rb");
    if (!f) return false;
    out.clear();
    char buf[16384];
    size_t n;
    while ((n = fread(buf, 1, sizeof buf, f)) > 0) out.append(buf, n);
    fclose(f);
    return true;
}

bool writeFile(const std::string& path, const std::string& data) {
    mkdirs(parent(path));
    FILE* f = fopen(path.c_str(), "wb");
    if (!f) return false;
    bool ok = fwrite(data.data(), 1, data.size(), f) == data.size();
    fclose(f);
    return ok;
}

bool copyFile(const std::string& from, const std::string& to) {
    FILE* in = fopen(from.c_str(), "rb");
    if (!in) return false;
    mkdirs(parent(to));
    FILE* out = fopen(to.c_str(), "wb");
    if (!out) { fclose(in); return false; }
    static char buf[256 * 1024];
    size_t n;
    bool ok = true;
    while ((n = fread(buf, 1, sizeof buf, in)) > 0)
        if (fwrite(buf, 1, n, out) != n) { ok = false; break; }
    fclose(in);
    fclose(out);
    return ok;
}

bool startsWith(const std::string& s, const std::string& p) {
    return s.size() >= p.size() && s.compare(0, p.size(), p) == 0;
}

bool endsWith(const std::string& s, const std::string& p) {
    return s.size() >= p.size() && s.compare(s.size() - p.size(), p.size(), p) == 0;
}

std::string trim(const std::string& s) {
    size_t a = 0, b = s.size();
    while (a < b && isspace((unsigned char)s[a])) a++;
    while (b > a && isspace((unsigned char)s[b - 1])) b--;
    return s.substr(a, b - a);
}

std::string lower(std::string s) {
    for (auto& c : s) c = (char)tolower((unsigned char)c);
    return s;
}

std::vector<std::string> lines(const std::string& s) {
    std::vector<std::string> out;
    size_t start = 0;
    while (start <= s.size()) {
        size_t end = s.find('\n', start);
        if (end == std::string::npos) end = s.size();
        std::string l = trim(s.substr(start, end - start));
        if (!l.empty()) out.push_back(l);
        start = end + 1;
    }
    return out;
}

static std::vector<long> versionParts(const std::string& v) {
    std::vector<long> parts;
    size_t i = 0;
    while (i < v.size() && !isdigit((unsigned char)v[i])) i++;
    while (i < v.size()) {
        if (!isdigit((unsigned char)v[i])) break;
        char* end = nullptr;
        parts.push_back(strtol(v.c_str() + i, &end, 10));
        i = end - v.c_str();
        if (i < v.size() && v[i] == '.') i++;
        else break;
    }
    return parts;
}

int versionCompare(const std::string& a, const std::string& b) {
    auto pa = versionParts(a), pb = versionParts(b);
    size_t n = pa.size() > pb.size() ? pa.size() : pb.size();
    for (size_t i = 0; i < n; i++) {
        long x = i < pa.size() ? pa[i] : 0;
        long y = i < pb.size() ? pb[i] : 0;
        if (x != y) return x < y ? -1 : 1;
    }
    return 0;
}

std::string humanSize(uint64_t bytes) {
    char buf[32];
    if (bytes >= 1024ULL * 1024 * 1024) snprintf(buf, sizeof buf, "%.1f GB", bytes / (1024.0 * 1024 * 1024));
    else if (bytes >= 1024 * 1024) snprintf(buf, sizeof buf, "%.1f MB", bytes / (1024.0 * 1024));
    else if (bytes >= 1024) snprintf(buf, sizeof buf, "%.0f KB", bytes / 1024.0);
    else snprintf(buf, sizeof buf, "%llu B", (unsigned long long)bytes);
    return buf;
}

std::string fmt(const char* tpl, const std::string& arg) {
    std::string s(tpl);
    size_t pos = s.find("{}");
    if (pos != std::string::npos) s.replace(pos, 2, arg);
    return s;
}

std::string utf8Cut(const std::string& s, size_t maxBytes) {
    if (s.size() <= maxBytes) return s;
    size_t n = maxBytes;
    while (n > 0 && ((unsigned char)s[n] & 0xC0) == 0x80) n--;
    return s.substr(0, n);
}

} // namespace util
