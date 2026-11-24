#include "json.hpp"
#include <cstdlib>
#include <cstring>
#include <cstdio>

namespace json {

static const Value kNull;

const Value& Value::operator[](const std::string& key) const {
    if (type != Object) return kNull;
    for (size_t i = 0; i < keys.size(); i++)
        if (keys[i] == key) return vals[i];
    return kNull;
}

const Value& Value::at(size_t i) const {
    if ((type != Array && type != Object) || i >= vals.size()) return kNull;
    return vals[i];
}

std::string Value::str(const std::string& def) const {
    if (type == String) return s;
    if (type == Number) {
        char buf[64];
        if (n == (long long)n) snprintf(buf, sizeof buf, "%lld", (long long)n);
        else snprintf(buf, sizeof buf, "%g", n);
        return buf;
    }
    if (type == Bool) return b ? "true" : "false";
    return def;
}

double Value::num(double def) const {
    if (type == Number) return n;
    if (type == String && !s.empty()) return strtod(s.c_str(), nullptr);
    return def;
}

namespace {

void putUtf8(std::string& out, unsigned cp) {
    if (cp < 0x80) out += (char)cp;
    else if (cp < 0x800) { out += (char)(0xC0 | (cp >> 6)); out += (char)(0x80 | (cp & 0x3F)); }
    else if (cp < 0x10000) {
        out += (char)(0xE0 | (cp >> 12));
        out += (char)(0x80 | ((cp >> 6) & 0x3F));
        out += (char)(0x80 | (cp & 0x3F));
    } else {
        out += (char)(0xF0 | (cp >> 18));
        out += (char)(0x80 | ((cp >> 12) & 0x3F));
        out += (char)(0x80 | ((cp >> 6) & 0x3F));
        out += (char)(0x80 | (cp & 0x3F));
    }
}

struct Parser {
    const char* p;
    const char* e;

    void ws() { while (p < e && (*p == ' ' || *p == '\n' || *p == '\r' || *p == '\t')) p++; }

    bool hex4(unsigned& out) {
        if (e - p < 4) return false;
        out = 0;
        for (int i = 0; i < 4; i++) {
            char c = *p++;
            out <<= 4;
            if (c >= '0' && c <= '9') out |= c - '0';
            else if (c >= 'a' && c <= 'f') out |= c - 'a' + 10;
            else if (c >= 'A' && c <= 'F') out |= c - 'A' + 10;
            else return false;
        }
        return true;
    }

    bool str(std::string& out) {
        p++; // comilla inicial
        while (p < e) {
            char c = *p++;
            if (c == '"') return true;
            if (c != '\\') { out += c; continue; }
            if (p >= e) return false;
            char x = *p++;
            switch (x) {
                case 'n': out += '\n'; break;
                case 't': out += '\t'; break;
                case 'r': out += '\r'; break;
                case 'b': out += '\b'; break;
                case 'f': out += '\f'; break;
                case 'u': {
                    unsigned cp;
                    if (!hex4(cp)) return false;
                    if (cp >= 0xD800 && cp <= 0xDBFF && e - p >= 6 && p[0] == '\\' && p[1] == 'u') {
                        p += 2;
                        unsigned lo;
                        if (!hex4(lo)) return false;
                        cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                    }
                    putUtf8(out, cp);
                    break;
                }
                default: out += x; break;
            }
        }
        return false;
    }

    bool val(Value& v, int depth) {
        if (depth > 128) return false;
        ws();
        if (p >= e) return false;
        char c = *p;
        if (c == '{') {
            v.type = Value::Object;
            p++;
            ws();
            if (p < e && *p == '}') { p++; return true; }
            while (true) {
                ws();
                if (p >= e || *p != '"') return false;
                std::string k;
                if (!str(k)) return false;
                ws();
                if (p >= e || *p != ':') return false;
                p++;
                Value x;
                if (!val(x, depth + 1)) return false;
                v.keys.push_back(std::move(k));
                v.vals.push_back(std::move(x));
                ws();
                if (p < e && *p == ',') { p++; continue; }
                if (p < e && *p == '}') { p++; return true; }
                return false;
            }
        }
        if (c == '[') {
            v.type = Value::Array;
            p++;
            ws();
            if (p < e && *p == ']') { p++; return true; }
            while (true) {
                Value x;
                if (!val(x, depth + 1)) return false;
                v.vals.push_back(std::move(x));
                ws();
                if (p < e && *p == ',') { p++; continue; }
                if (p < e && *p == ']') { p++; return true; }
                return false;
            }
        }
        if (c == '"') { v.type = Value::String; return str(v.s); }
        if (e - p >= 4 && !strncmp(p, "true", 4)) { v.type = Value::Bool; v.b = true; p += 4; return true; }
        if (e - p >= 5 && !strncmp(p, "false", 5)) { v.type = Value::Bool; v.b = false; p += 5; return true; }
        if (e - p >= 4 && !strncmp(p, "null", 4)) { v.type = Value::Null; p += 4; return true; }
        if (c == '-' || (c >= '0' && c <= '9')) {
            char* end = nullptr;
            v.type = Value::Number;
            v.n = strtod(p, &end);
            if (!end || end == p) return false;
            p = end;
            return true;
        }
        return false;
    }
};

} // namespace

bool parse(const std::string& text, Value& out) {
    out = Value();
    Parser ps{text.c_str(), text.c_str() + text.size()};
    return ps.val(out, 0);
}

std::string escape(const std::string& s) {
    std::string o;
    o.reserve(s.size() + 8);
    for (unsigned char c : s) {
        switch (c) {
            case '"': o += "\\\""; break;
            case '\\': o += "\\\\"; break;
            case '\n': o += "\\n"; break;
            case '\r': o += "\\r"; break;
            case '\t': o += "\\t"; break;
            default:
                if (c < 0x20) { char b[8]; snprintf(b, sizeof b, "\\u%04x", c); o += b; }
                else o += (char)c;
        }
    }
    return o;
}

} // namespace json
