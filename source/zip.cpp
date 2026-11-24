// Extractor ZIP propio sobre zlib (métodos store y deflate).
#include "zip.hpp"
#include "util.hpp"
#include <zlib.h>
#include <cstdio>
#include <cstring>
#include <algorithm>

namespace zip {

namespace {

struct Entry {
    std::string name;
    uint16_t method = 0;
    uint32_t csize = 0, usize = 0, offset = 0;
};

uint16_t r16(const uint8_t* p) { return (uint16_t)(p[0] | (p[1] << 8)); }
uint32_t r32(const uint8_t* p) { return p[0] | (p[1] << 8) | (p[2] << 16) | ((uint32_t)p[3] << 24); }

bool listEntries(FILE* f, std::vector<Entry>& out, std::string& err) {
    if (fseek(f, 0, SEEK_END) != 0) { err = "zip: seek"; return false; }
    long size = ftell(f);
    if (size < 22) { err = "zip: archivo demasiado pequeño"; return false; }
    long tail = std::min<long>(size, 65557);
    std::vector<uint8_t> buf(tail);
    fseek(f, size - tail, SEEK_SET);
    if ((long)fread(buf.data(), 1, tail, f) != tail) { err = "zip: lectura"; return false; }
    long pos = -1;
    for (long i = tail - 22; i >= 0; i--)
        if (r32(&buf[i]) == 0x06054b50) { pos = i; break; }
    if (pos < 0) { err = "zip: no es un archivo zip válido"; return false; }

    uint16_t count = r16(&buf[pos + 10]);
    uint32_t cdSize = r32(&buf[pos + 12]);
    uint32_t cdOff = r32(&buf[pos + 16]);
    std::vector<uint8_t> cd(cdSize);
    fseek(f, cdOff, SEEK_SET);
    if (fread(cd.data(), 1, cdSize, f) != cdSize) { err = "zip: directorio central"; return false; }

    size_t p = 0;
    for (int i = 0; i < count; i++) {
        if (p + 46 > cd.size() || r32(&cd[p]) != 0x02014b50) { err = "zip: entrada corrupta"; return false; }
        Entry e;
        e.method = r16(&cd[p + 10]);
        e.csize = r32(&cd[p + 20]);
        e.usize = r32(&cd[p + 24]);
        uint16_t n = r16(&cd[p + 28]), x = r16(&cd[p + 30]), c = r16(&cd[p + 32]);
        e.offset = r32(&cd[p + 42]);
        if (p + 46 + n > cd.size()) { err = "zip: nombre corrupto"; return false; }
        e.name.assign((const char*)&cd[p + 46], n);
        p += 46 + n + x + c;
        out.push_back(std::move(e));
    }
    return true;
}

bool safeName(const std::string& n) {
    if (n.empty() || n[0] == '/' || n.find('\\') != std::string::npos) return false;
    if (n == ".." || util::startsWith(n, "../") || n.find("/../") != std::string::npos) return false;
    return true;
}

bool readData(FILE* f, const Entry& e, const std::function<bool(const uint8_t*, size_t)>& sink, std::string& err) {
    uint8_t lh[30];
    if (fseek(f, e.offset, SEEK_SET) != 0 || fread(lh, 1, 30, f) != 30 || r32(lh) != 0x04034b50) {
        err = "zip: cabecera local";
        return false;
    }
    fseek(f, (long)e.offset + 30 + r16(&lh[26]) + r16(&lh[28]), SEEK_SET);

    std::vector<uint8_t> in(64 * 1024);
    if (e.method == 0) {
        uint32_t left = e.csize;
        while (left) {
            size_t n = fread(in.data(), 1, std::min<uint32_t>(left, in.size()), f);
            if (!n) { err = "zip: lectura"; return false; }
            if (!sink(in.data(), n)) return false;
            left -= n;
        }
        return true;
    }
    if (e.method != 8) { err = "zip: método de compresión no soportado"; return false; }

    std::vector<uint8_t> out(128 * 1024);
    z_stream z;
    memset(&z, 0, sizeof z);
    if (inflateInit2(&z, -MAX_WBITS) != Z_OK) { err = "zlib"; return false; }
    uint32_t left = e.csize;
    int ret = Z_OK;
    bool ok = true;
    while (ret != Z_STREAM_END) {
        if (z.avail_in == 0) {
            if (!left) { ok = false; err = "zip: datos truncados"; break; }
            size_t n = fread(in.data(), 1, std::min<uint32_t>(left, in.size()), f);
            if (!n) { ok = false; err = "zip: lectura"; break; }
            left -= n;
            z.next_in = in.data();
            z.avail_in = (uInt)n;
        }
        z.next_out = out.data();
        z.avail_out = (uInt)out.size();
        ret = inflate(&z, Z_NO_FLUSH);
        if (ret != Z_OK && ret != Z_STREAM_END) { ok = false; err = "zip: datos corruptos"; break; }
        size_t have = out.size() - z.avail_out;
        if (have && !sink(out.data(), have)) { ok = false; break; }
    }
    inflateEnd(&z);
    return ok;
}

} // namespace

bool extract(const std::string& zipPath, Mapper mapper, Progress progress,
             std::vector<std::string>* written, std::string& err) {
    FILE* f = fopen(zipPath.c_str(), "rb");
    if (!f) { err = "No se pudo abrir " + zipPath; return false; }
    std::vector<Entry> entries;
    if (!listEntries(f, entries, err)) { fclose(f); return false; }

    uint64_t total = 0, done = 0;
    for (auto& e : entries)
        if (!util::endsWith(e.name, "/") && safeName(e.name) && !mapper(e.name).empty()) total += e.usize;

    for (auto& e : entries) {
        if (util::endsWith(e.name, "/") || !safeName(e.name)) continue;
        std::string dest = mapper(e.name);
        if (dest.empty()) continue;
        util::mkdirs(util::parent(dest));
        FILE* o = fopen(dest.c_str(), "wb");
        if (!o) { err = "No se pudo escribir " + dest; fclose(f); return false; }
        bool cancelled = false;
        bool ok = readData(f, e, [&](const uint8_t* d, size_t n) {
            if (fwrite(d, 1, n, o) != n) { err = "Error de escritura (¿microSD llena?)"; return false; }
            done += n;
            if (progress && !progress(done, total)) { cancelled = true; err = "cancel"; return false; }
            return true;
        }, err);
        fclose(o);
        if (!ok) {
            remove(dest.c_str());
            fclose(f);
            (void)cancelled;
            return false;
        }
        if (written) written->push_back(dest);
    }
    fclose(f);
    return true;
}

bool readEntry(const std::string& zipPath, const std::string& name, std::string& out) {
    FILE* f = fopen(zipPath.c_str(), "rb");
    if (!f) return false;
    std::vector<Entry> entries;
    std::string err;
    bool ok = false;
    if (listEntries(f, entries, err)) {
        for (auto& e : entries) {
            if (e.name != name) continue;
            out.clear();
            ok = readData(f, e, [&](const uint8_t* d, size_t n) { out.append((const char*)d, n); return true; }, err);
            break;
        }
    }
    fclose(f);
    return ok;
}

} // namespace zip
