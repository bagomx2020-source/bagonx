#include "gfx.hpp"
#include <switch.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <cmath>
#include <cstdio>
#include <map>
#include <unordered_map>

namespace gfx {

namespace {

SDL_Window* g_win = nullptr;
SDL_Renderer* g_ren = nullptr;
PlFontData g_fontData[2];
std::map<int, TTF_Font*> g_fonts[2];

struct Tex {
    SDL_Texture* tex;
    int w, h;
    uint64_t used;
};
std::unordered_map<std::string, Tex> g_cache;
uint64_t g_frame = 0;

TTF_Font* font(int which, int size) {
    auto& m = g_fonts[which];
    auto it = m.find(size);
    if (it != m.end()) return it->second;
    PlFontData& d = g_fontData[which];
    SDL_RWops* rw = SDL_RWFromConstMem(d.address, (int)d.size);
    TTF_Font* f = rw ? TTF_OpenFontRW(rw, 1, size) : nullptr;
    m[size] = f;
    return f;
}

void setColor(Color c) { SDL_SetRenderDrawColor(g_ren, c.r, c.g, c.b, c.a); }

std::string truncateToWidth(TTF_Font* f, std::string s, int maxW) {
    int w = 0, h = 0;
    TTF_SizeUTF8(f, s.c_str(), &w, &h);
    if (w <= maxW) return s;
    while (!s.empty()) {
        size_t n = s.size() - 1;
        while (n > 0 && ((unsigned char)s[n] & 0xC0) == 0x80) n--;
        s.erase(n);
        std::string t = s + "…";
        TTF_SizeUTF8(f, t.c_str(), &w, &h);
        if (w <= maxW) return t;
    }
    return s;
}

Tex* getTex(int which, const std::string& s, int size, Color c, int maxW, int wrap) {
    if (s.empty()) return nullptr;
    char key[64];
    snprintf(key, sizeof key, "%d|%d|%02x%02x%02x%02x|%d|%d|", which, size, c.r, c.g, c.b, c.a, maxW, wrap);
    std::string k = key + s;
    auto it = g_cache.find(k);
    if (it != g_cache.end()) { it->second.used = g_frame; return &it->second; }

    TTF_Font* f = font(which, size);
    if (!f) return nullptr;
    SDL_Color sc{c.r, c.g, c.b, c.a};
    std::string str = maxW > 0 ? truncateToWidth(f, s, maxW) : s;
    SDL_Surface* surf = wrap > 0 ? TTF_RenderUTF8_Blended_Wrapped(f, str.c_str(), sc, wrap)
                                 : TTF_RenderUTF8_Blended(f, str.c_str(), sc);
    if (!surf) return nullptr;
    SDL_Texture* t = SDL_CreateTextureFromSurface(g_ren, surf);
    Tex tx{t, surf->w, surf->h, g_frame};
    SDL_FreeSurface(surf);
    if (!t) return nullptr;
    return &g_cache.emplace(k, tx).first->second;
}

void blit(Tex* t, int x, int y, int align) {
    if (!t) return;
    int dx = align == Center ? x - t->w / 2 : align == Right ? x - t->w : x;
    SDL_Rect d{dx, y, t->w, t->h};
    SDL_RenderCopy(g_ren, t->tex, nullptr, &d);
}

} // namespace

bool init() {
    if (SDL_Init(SDL_INIT_VIDEO) < 0) return false;
    g_win = SDL_CreateWindow(APP_TITLE, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 1280, 720, 0);
    if (!g_win) return false;
    g_ren = SDL_CreateRenderer(g_win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!g_ren) return false;
    SDL_SetRenderDrawBlendMode(g_ren, SDL_BLENDMODE_BLEND);
    if (TTF_Init() < 0) return false;
    if (R_FAILED(plGetSharedFontByType(&g_fontData[0], PlSharedFontType_Standard))) return false;
    if (R_FAILED(plGetSharedFontByType(&g_fontData[1], PlSharedFontType_NintendoExt))) return false;
    return true;
}

void exit() {
    for (auto& kv : g_cache) SDL_DestroyTexture(kv.second.tex);
    g_cache.clear();
    for (auto& m : g_fonts) {
        for (auto& kv : m) if (kv.second) TTF_CloseFont(kv.second);
        m.clear();
    }
    TTF_Quit();
    if (g_ren) SDL_DestroyRenderer(g_ren);
    if (g_win) SDL_DestroyWindow(g_win);
    SDL_Quit();
}

void clear(Color c) {
    setColor(c);
    SDL_RenderClear(g_ren);
}

void present() {
    SDL_RenderPresent(g_ren);
    g_frame++;
    // Libera textos que no se han dibujado en ~3 segundos
    if (g_frame % 60 == 0) {
        for (auto it = g_cache.begin(); it != g_cache.end();) {
            if (it->second.used + 180 < g_frame) {
                SDL_DestroyTexture(it->second.tex);
                it = g_cache.erase(it);
            } else ++it;
        }
    }
}

void rect(int x, int y, int w, int h, Color c) {
    setColor(c);
    SDL_Rect r{x, y, w, h};
    SDL_RenderFillRect(g_ren, &r);
}

void roundRect(int x, int y, int w, int h, int r, Color c) {
    if (r * 2 > h) r = h / 2;
    if (r * 2 > w) r = w / 2;
    setColor(c);
    SDL_Rect mid{x, y + r, w, h - 2 * r};
    SDL_RenderFillRect(g_ren, &mid);
    for (int i = 0; i < r; i++) {
        float dy = r - i - 0.5f;
        int dx = (int)(r - std::sqrt((float)r * r - dy * dy) + 0.5f);
        SDL_Rect top{x + dx, y + i, w - 2 * dx, 1};
        SDL_Rect bot{x + dx, y + h - 1 - i, w - 2 * dx, 1};
        SDL_RenderFillRect(g_ren, &top);
        SDL_RenderFillRect(g_ren, &bot);
    }
}

void clip(int x, int y, int w, int h) {
    SDL_Rect r{x, y, w, h};
    SDL_RenderSetClipRect(g_ren, &r);
}

void noClip() { SDL_RenderSetClipRect(g_ren, nullptr); }

int text(const std::string& s, int x, int y, int size, Color c, int align, int maxW) {
    Tex* t = getTex(0, s, size, c, maxW, 0);
    blit(t, x, y, align);
    return t ? t->w : 0;
}

int measure(const std::string& s, int size) {
    TTF_Font* f = font(0, size);
    int w = 0, h = 0;
    if (f && !s.empty()) TTF_SizeUTF8(f, s.c_str(), &w, &h);
    return w;
}

int glyph(const char* utf8, int x, int y, int size, Color c) {
    Tex* t = getTex(1, utf8, size, c, 0, 0);
    blit(t, x, y, Left);
    return t ? t->w : 0;
}

int glyphWidth(const char* utf8, int size) {
    TTF_Font* f = font(1, size);
    int w = 0, h = 0;
    if (f) TTF_SizeUTF8(f, utf8, &w, &h);
    return w;
}

int wrappedHeight(const std::string& s, int size, Color c, int wrapW) {
    Tex* t = getTex(0, s, size, c, 0, wrapW);
    return t ? t->h : 0;
}

int textWrapped(const std::string& s, int x, int y, int size, Color c, int wrapW) {
    Tex* t = getTex(0, s, size, c, 0, wrapW);
    blit(t, x, y, Left);
    return t ? t->h : 0;
}

} // namespace gfx
