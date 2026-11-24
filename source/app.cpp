#include "app.hpp"
#include <switch.h>
#include <SDL2/SDL.h>
#include <pthread.h>
#include <sys/statvfs.h>
#include <algorithm>
#include <cstdio>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "cfw.hpp"
#include "config.hpp"
#include "gfx.hpp"
#include "i18n.hpp"
#include "store.hpp"
#include "task.hpp"
#include "updater.hpp"
#include "util.hpp"

namespace {

// ---------------------------------------------------------------- tema
struct Theme {
    Color bg, side, item, itemSel, text, sub, accent, onAccent, ok, warn, overlay;
};
const Theme kDark{{20, 22, 28, 255},   {29, 32, 40, 255},  {37, 41, 51, 255},  {46, 51, 64, 255},
                  {236, 238, 243, 255}, {146, 152, 166, 255}, {0, 198, 170, 255}, {12, 20, 22, 255},
                  {86, 212, 124, 255},  {255, 178, 64, 255},  {0, 0, 0, 175}};
const Theme kLight{{238, 240, 244, 255}, {224, 227, 234, 255}, {255, 255, 255, 255}, {243, 250, 249, 255},
                   {22, 24, 30, 255},    {96, 102, 116, 255},  {0, 150, 130, 255},   {255, 255, 255, 255},
                   {28, 150, 70, 255},   {196, 116, 0, 255},   {0, 0, 0, 120}};
const Theme& T() { return cfg().theme == "light" ? kLight : kDark; }

// ---------------------------------------------------------------- entrada
enum : uint32_t {
    K_UP = 1, K_DOWN = 2, K_LEFT = 4, K_RIGHT = 8, K_A = 16, K_B = 32,
    K_X = 64, K_Y = 128, K_L = 256, K_R = 512, K_PLUS = 1024
};

PadState g_pad;

uint32_t readInput() {
    padUpdate(&g_pad);
    u64 down = padGetButtonsDown(&g_pad);
    u64 held = padGetButtons(&g_pad);
    // Repetición al mantener pulsada una dirección
    static u64 lastDir = 0, nextRepeat = 0;
    const u64 dirMask = HidNpadButton_AnyUp | HidNpadButton_AnyDown | HidNpadButton_AnyLeft | HidNpadButton_AnyRight;
    u64 now = armTicksToNs(armGetSystemTick()) / 1000000ULL;
    u64 heldDir = held & dirMask;
    if (down & dirMask) { lastDir = heldDir; nextRepeat = now + 380; }
    else if (heldDir && heldDir == lastDir && now >= nextRepeat) { down |= heldDir; nextRepeat = now + 70; }
    else if (!heldDir) lastDir = 0;

    uint32_t k = 0;
    if (down & HidNpadButton_AnyUp) k |= K_UP;
    if (down & HidNpadButton_AnyDown) k |= K_DOWN;
    if (down & HidNpadButton_AnyLeft) k |= K_LEFT;
    if (down & HidNpadButton_AnyRight) k |= K_RIGHT;
    if (down & HidNpadButton_A) k |= K_A;
    if (down & HidNpadButton_B) k |= K_B;
    if (down & HidNpadButton_X) k |= K_X;
    if (down & HidNpadButton_Y) k |= K_Y;
    if (down & (HidNpadButton_L | HidNpadButton_ZL)) k |= K_L;
    if (down & (HidNpadButton_R | HidNpadButton_ZR)) k |= K_R;
    if (down & HidNpadButton_Plus) k |= K_PLUS;
    return k;
}

// ---------------------------------------------------------------- estado
struct Item {
    std::string title, sub, value;
    Color vcol{0, 0, 0, 0};
    bool header = false;
    std::function<void()> act;
};

struct Dialog {
    bool open = false;
    std::string title, body;
    std::vector<std::string> buttons;
    std::vector<std::function<void()>> actions;
    int sel = 0;
};

struct Task {
    TaskCtx ctx;
    pthread_t thread;
    std::atomic<bool> finished{false};
    bool ok = false;
    std::string title, msg;
    std::function<bool(TaskCtx&, std::string&)> fn;
    std::function<void(bool, const std::string&)> after;
};

struct SysInfo {
    std::string fw = "?", ams, sdFree = "?";
    bool applet = false;
};

enum { TAB_CFW, TAB_APPS, TAB_SETTINGS, TAB_ABOUT, TAB_COUNT };
enum { STORE_IDLE, STORE_LOADING, STORE_OK, STORE_ERROR };

const char* kCats[] = {"", "game", "emu", "tool", "advanced", "theme", "aesthetics", "concept", "misc", "legacy"};
const int kCatCount = sizeof(kCats) / sizeof(kCats[0]);

bool g_running = true;
std::string g_nroPath;
int g_tab = TAB_CFW;
bool g_focusContent = false;
int g_sel[TAB_COUNT] = {0};
int g_scroll[TAB_COUNT] = {0};
std::vector<Item> g_items;
bool g_dirty = true;
Dialog g_dlg;
std::unique_ptr<Task> g_task;
SysInfo g_sys;

std::vector<store::Pkg> g_pkgs;
int g_storeState = STORE_IDLE;
std::string g_storeErr;
int g_cat = 0;
std::string g_search;

// ---------------------------------------------------------------- utilidades de UI
void showDialog(const std::string& title, const std::string& body, std::vector<std::string> buttons,
                std::vector<std::function<void()>> actions) {
    g_dlg.open = true;
    g_dlg.title = title;
    g_dlg.body = body;
    g_dlg.buttons = std::move(buttons);
    g_dlg.actions = std::move(actions);
    g_dlg.actions.resize(g_dlg.buttons.size());
    g_dlg.sel = 0;
}

void showMessage(const std::string& title, const std::string& body) {
    showDialog(title, body, {tr("ok")}, {nullptr});
}

void* taskEntry(void* arg) {
    Task* t = static_cast<Task*>(arg);
    t->ok = t->fn(t->ctx, t->msg);
    t->finished = true;
    return nullptr;
}

void startTask(const std::string& title, std::function<bool(TaskCtx&, std::string&)> fn,
               std::function<void(bool, const std::string&)> after) {
    if (g_task) return;
    auto t = std::make_unique<Task>();
    t->title = title;
    t->fn = std::move(fn);
    t->after = std::move(after);
    t->ctx.setStatus(tr("working"));
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setstacksize(&attr, 1024 * 1024);
    int rc = pthread_create(&t->thread, &attr, taskEntry, t.get());
    pthread_attr_destroy(&attr);
    if (rc != 0) {
        showMessage(tr("error"), "pthread_create");
        return;
    }
    appletSetMediaPlaybackState(true); // evita que la consola se suspenda durante la descarga
    g_task = std::move(t);
}

void pollTask() {
    if (!g_task || !g_task->finished) return;
    pthread_join(g_task->thread, nullptr);
    appletSetMediaPlaybackState(false);
    std::unique_ptr<Task> t = std::move(g_task);
    if (t->after) t->after(t->ok, t->msg);
    else if (!t->ok || !t->msg.empty()) showMessage(t->ok ? tr("done") : tr("error"), t->msg);
}

void refreshSys() {
    SetSysFirmwareVersion fw;
    if (R_SUCCEEDED(setsysGetFirmwareVersion(&fw))) g_sys.fw = fw.display_version;
    u64 v = 0;
    if (R_SUCCEEDED(splGetConfig((SplConfigItem)65000, &v))) {
        char buf[32];
        snprintf(buf, sizeof buf, "%u.%u.%u", (unsigned)((v >> 56) & 0xFF), (unsigned)((v >> 48) & 0xFF),
                 (unsigned)((v >> 40) & 0xFF));
        g_sys.ams = buf;
    } else {
        g_sys.ams.clear();
    }
    struct statvfs st;
    if (statvfs("sdmc:/", &st) == 0) g_sys.sdFree = util::humanSize((uint64_t)st.f_bavail * st.f_frsize);
    AppletType at = appletGetAppletType();
    g_sys.applet = !(at == AppletType_Application || at == AppletType_SystemApplication);
}

// ---------------------------------------------------------------- construcción de listas
void addHeader(const std::string& t) {
    Item it;
    it.title = t;
    it.header = true;
    g_items.push_back(std::move(it));
}

void addInfo(const std::string& title, const std::string& value, Color vcol = {0, 0, 0, 0}) {
    Item it;
    it.title = title;
    it.value = value;
    it.vcol = vcol;
    g_items.push_back(std::move(it));
}

void addAction(const std::string& title, const std::string& sub, std::function<void()> act,
               const std::string& value = "", Color vcol = {0, 0, 0, 0}) {
    Item it;
    it.title = title;
    it.sub = sub;
    it.act = std::move(act);
    it.value = value;
    it.vcol = vcol;
    g_items.push_back(std::move(it));
}

void confirmInstall(const std::string& name, std::function<bool(TaskCtx&, std::string&)> fn) {
    showDialog(tr("confirm"), util::fmt(tr("confirm_install"), name), {tr("install"), tr("cancel")},
               {[name, fn] {
                    startTask(name, fn, [](bool ok, const std::string& m) {
                        refreshSys();
                        g_dirty = true;
                        showMessage(ok ? tr("done") : tr("error"), m);
                    });
                },
                nullptr});
}

void buildCfw() {
    const Theme& t = T();
    addHeader(tr("status"));
    addInfo(tr("firmware"), g_sys.fw);
    addInfo(tr("ams_installed"), g_sys.ams.empty() ? tr("not_detected") : g_sys.ams,
            g_sys.ams.empty() ? t.warn : t.ok);
    addHeader(tr("downloads"));
    addAction(tr("dl_ams"), tr("dl_ams_sub"), [] { confirmInstall("Atmosphère", cfw::installAtmosphere); });
    addAction(tr("dl_hekate"), tr("dl_hekate_sub"), [] { confirmInstall("Hekate", cfw::installHekate); });
    addAction(tr("dl_both"), tr("dl_both_sub"), [] { confirmInstall("Atmosphère + Hekate", cfw::installBoth); });
    addHeader(tr("system"));
    addAction(tr("reboot"), tr("reboot_sub"), [] {
        showDialog(tr("confirm"), tr("confirm_reboot"), {tr("yes"), tr("no")},
                   {[] { spsmShutdown(true); }, nullptr});
    });
}

void loadStore() {
    if (g_task) return;
    g_storeState = STORE_LOADING;
    auto result = std::make_shared<std::vector<store::Pkg>>();
    startTask(tr("loading_catalog"),
              [result](TaskCtx& c, std::string& m) {
                  c.setStatus(tr("loading_catalog"));
                  return store::fetch(*result, m);
              },
              [result](bool ok, const std::string& m) {
                  if (ok) { g_pkgs = std::move(*result); g_storeState = STORE_OK; }
                  else { g_storeErr = m; g_storeState = STORE_ERROR; }
                  g_dirty = true;
              });
}

void installPkg(size_t idx) {
    store::Pkg p = g_pkgs[idx];
    startTask(p.title, [p](TaskCtx& c, std::string& m) { return store::install(p, c, m); },
              [](bool ok, const std::string& m) {
                  store::refreshInstalled(g_pkgs);
                  refreshSys();
                  g_dirty = true;
                  showMessage(ok ? tr("done") : tr("error"), m);
              });
}

void uninstallPkg(size_t idx) {
    std::string m;
    bool ok = store::uninstall(g_pkgs[idx], m);
    store::refreshInstalled(g_pkgs);
    refreshSys();
    g_dirty = true;
    showMessage(ok ? tr("done") : tr("error"), m);
}

void openPkg(size_t idx) {
    const store::Pkg& p = g_pkgs[idx];
    std::string body = p.desc;
    if (!p.details.empty() && p.details != p.desc) body += "\n\n" + p.details;
    if (body.size() > 650) body = util::utf8Cut(body, 650) + "…";
    body += "\n\n" + std::string(tr("version")) + ": " + p.version + "    " + tr("size") + ": " +
            util::humanSize(p.sizeKB * 1024);
    bool installed = !p.installedVer.empty();
    if (installed) body += "    " + std::string(tr("installed")) + ": " + p.installedVer;

    std::vector<std::string> b;
    std::vector<std::function<void()>> a;
    b.push_back(!installed ? tr("install") : (p.installedVer == p.version ? tr("reinstall") : tr("update")));
    a.push_back([idx] { installPkg(idx); });
    if (installed) {
        b.push_back(tr("uninstall"));
        a.push_back([idx] { uninstallPkg(idx); });
    }
    b.push_back(tr("cancel"));
    a.push_back(nullptr);
    showDialog(p.title + "  ·  " + p.author, body, b, a);
}

std::string catLabel(int i) {
    if (i == 0) return tr("all");
    std::string key = std::string("cat_") + kCats[i];
    return tr(key.c_str());
}

void buildApps() {
    const Theme& t = T();
    if (g_storeState == STORE_IDLE) loadStore();
    if (g_storeState == STORE_ERROR) {
        addHeader(tr("catalog_error"));
        addInfo(g_storeErr, "");
        addAction(tr("retry"), "", [] { loadStore(); });
        return;
    }
    if (g_storeState != STORE_OK) return;
    std::string q = util::lower(g_search);
    for (size_t i = 0; i < g_pkgs.size(); i++) {
        const store::Pkg& p = g_pkgs[i];
        if (g_cat > 0 && p.category != kCats[g_cat]) continue;
        if (!q.empty() && util::lower(p.title).find(q) == std::string::npos &&
            util::lower(p.name).find(q) == std::string::npos && util::lower(p.author).find(q) == std::string::npos)
            continue;
        std::string sub = "v" + p.version + "  ·  " + p.author;
        if (p.installedVer.empty())
            addAction(p.title, sub, [i] { openPkg(i); }, util::humanSize(p.sizeKB * 1024));
        else if (p.installedVer == p.version)
            addAction(p.title, sub, [i] { openPkg(i); }, tr("installed"), t.ok);
        else
            addAction(p.title, sub, [i] { openPkg(i); }, tr("update"), t.warn);
    }
    if (g_items.empty()) addInfo(tr("no_results"), "");
}

void checkUpdate() {
    std::string repo = cfg().updateRepo;
    if (repo.empty()) { showMessage(tr("updates"), tr("repo_not_set")); return; }
    auto tag = std::make_shared<std::string>();
    auto url = std::make_shared<std::string>();
    startTask(tr("check_update"),
              [repo, tag, url](TaskCtx& c, std::string& m) { return updater::check(repo, *tag, *url, c, m); },
              [tag, url](bool ok, const std::string& m) {
                  if (!ok) { showMessage(tr("error"), m); return; }
                  if (util::versionCompare(*tag, APP_VERSION) <= 0) {
                      showMessage(tr("updates"), util::fmt(tr("up_to_date"), APP_VERSION));
                      return;
                  }
                  if (url->empty()) { showMessage(tr("error"), util::fmt(tr("no_asset"), *tag)); return; }
                  showDialog(tr("updates"), util::fmt(tr("new_version"), *tag), {tr("yes"), tr("no")},
                             {[url] {
                                  startTask(APP_TITLE,
                                            [url](TaskCtx& c, std::string& m) {
                                                return updater::apply(*url, g_nroPath, c, m);
                                            },
                                            nullptr);
                              },
                              nullptr});
              });
}

void buildSettings() {
    addAction(tr("language"), "", [] {
        cfg().lang = cfg().lang == "es" ? "en" : "es";
        i18nSetLang(cfg().lang);
        cfgSave();
        g_dirty = true;
    }, cfg().lang == "es" ? "Español" : "English");
    addAction(tr("theme"), "", [] {
        cfg().theme = cfg().theme == "dark" ? "light" : "dark";
        cfgSave();
        g_dirty = true;
    }, cfg().theme == "dark" ? tr("theme_dark") : tr("theme_light"));
    addHeader(tr("updates"));
    addAction(tr("check_update"), util::fmt(tr("repo"), cfg().updateRepo.empty() ? "—" : cfg().updateRepo),
              [] { checkUpdate(); });
    addHeader(tr("help"));
    addInfo(tr("config_file"), "/config/bagonx/config.json");
}

void buildAbout() {
    const Theme& t = T();
    addInfo(APP_TITLE, "v" APP_VERSION, t.accent);
    addInfo(tr("mode"), g_sys.applet ? tr("mode_applet") : tr("mode_app"), g_sys.applet ? t.warn : t.ok);
    addInfo(tr("sd_free"), g_sys.sdFree);
    addInfo(tr("firmware"), g_sys.fw);
    addInfo(tr("ams_installed"), g_sys.ams.empty() ? tr("not_detected") : g_sys.ams);
    addHeader(tr("credits"));
    addAction(tr("credits"), tr("credits_sub"), nullptr);
}

int nextSelectable(int from, int dir) {
    for (int i = from; i >= 0 && i < (int)g_items.size(); i += dir)
        if (!g_items[i].header) return i;
    return -1;
}

void rebuild() {
    g_dirty = false;
    g_items.clear();
    switch (g_tab) {
        case TAB_CFW: buildCfw(); break;
        case TAB_APPS: buildApps(); break;
        case TAB_SETTINGS: buildSettings(); break;
        case TAB_ABOUT: buildAbout(); break;
    }
    int& s = g_sel[g_tab];
    if (s >= (int)g_items.size()) s = (int)g_items.size() - 1;
    if (s < 0) s = 0;
    int n = nextSelectable(s, +1);
    if (n < 0) n = nextSelectable(s, -1);
    s = n < 0 ? 0 : n;
    if (n < 0) g_focusContent = false;
}

void setTab(int t) {
    g_tab = t;
    g_dirty = true;
}

// ---------------------------------------------------------------- entrada por pantalla
void searchApps() {
    SwkbdConfig kbd;
    if (R_FAILED(swkbdCreate(&kbd, 0))) return;
    swkbdConfigMakePresetDefault(&kbd);
    swkbdConfigSetGuideText(&kbd, tr("search"));
    swkbdConfigSetInitialText(&kbd, g_search.c_str());
    char out[128] = {0};
    Result rc = swkbdShow(&kbd, out, sizeof out);
    swkbdClose(&kbd);
    if (R_SUCCEEDED(rc)) {
        g_search = util::trim(out);
        g_sel[TAB_APPS] = 0;
        g_scroll[TAB_APPS] = 0;
        g_dirty = true;
    }
}

void handleDialog(uint32_t k) {
    int n = (int)g_dlg.buttons.size();
    if (k & K_LEFT) g_dlg.sel = (g_dlg.sel + n - 1) % n;
    if (k & K_RIGHT) g_dlg.sel = (g_dlg.sel + 1) % n;
    if (k & K_B) { g_dlg.open = false; return; }
    if (k & K_A) {
        auto act = g_dlg.actions[g_dlg.sel];
        g_dlg.open = false;
        if (act) act();
    }
}

void handleMain(uint32_t k) {
    if (k & K_PLUS) { g_running = false; return; }
    if (k & K_L) setTab((g_tab + TAB_COUNT - 1) % TAB_COUNT);
    if (k & K_R) setTab((g_tab + 1) % TAB_COUNT);

    if (g_tab == TAB_APPS && g_storeState == STORE_OK) {
        if (k & K_X) {
            g_cat = (g_cat + 1) % kCatCount;
            g_sel[TAB_APPS] = 0;
            g_scroll[TAB_APPS] = 0;
            g_dirty = true;
        }
        if (k & K_Y) searchApps();
    }

    if (!g_focusContent) {
        if (k & K_UP) setTab((g_tab + TAB_COUNT - 1) % TAB_COUNT);
        if (k & K_DOWN) setTab((g_tab + 1) % TAB_COUNT);
        if ((k & (K_A | K_RIGHT)) && nextSelectable(0, +1) >= 0) g_focusContent = true;
        return;
    }
    int& s = g_sel[g_tab];
    if (k & K_UP) { int n = nextSelectable(s - 1, -1); if (n >= 0) s = n; }
    if (k & K_DOWN) { int n = nextSelectable(s + 1, +1); if (n >= 0) s = n; }
    if (k & (K_B | K_LEFT)) { g_focusContent = false; return; }
    if ((k & K_A) && s >= 0 && s < (int)g_items.size() && g_items[s].act) {
        auto act = g_items[s].act; // copia: la acción puede reconstruir la lista
        act();
    }
}

// ---------------------------------------------------------------- dibujo
struct Hint { const char* glyph; std::string label; };

void drawHints(const std::vector<Hint>& hints) {
    const Theme& t = T();
    int x = 1250, y = 676;
    for (auto it = hints.rbegin(); it != hints.rend(); ++it) {
        x -= gfx::measure(it->label, 20);
        gfx::text(it->label, x, y, 20, t.text);
        x -= 6 + gfx::glyphWidth(it->glyph, 24);
        gfx::glyph(it->glyph, x, y - 2, 24, t.text);
        x -= 34;
    }
}

void drawSidebar() {
    const Theme& t = T();
    gfx::rect(0, 0, 300, 720, t.side);
    gfx::text(APP_TITLE, 34, 34, 38, t.accent);
    gfx::text("v" APP_VERSION, 36, 84, 18, t.sub);
    const char* tabs[TAB_COUNT] = {tr("tab_cfw"), tr("tab_apps"), tr("tab_settings"), tr("tab_about")};
    for (int i = 0; i < TAB_COUNT; i++) {
        int y = 140 + i * 62;
        bool sel = i == g_tab;
        if (sel) {
            if (!g_focusContent) gfx::roundRect(13, y - 3, 274, 58, 12, t.accent);
            gfx::roundRect(16, y, 268, 52, 10, t.item);
            gfx::roundRect(26, y + 14, 5, 24, 2, t.accent);
        }
        gfx::text(tabs[i], 46, y + 11, 24, sel ? t.text : t.sub);
    }
    gfx::text("BagoTech", 36, 610, 16, t.sub);
}

void drawList() {
    const Theme& t = T();
    const int x = 330, y0 = 124, w = 920, h = 524;
    std::vector<int> top(g_items.size());
    int cy = 0;
    for (size_t i = 0; i < g_items.size(); i++) {
        top[i] = cy;
        cy += g_items[i].header ? 46 : 78;
    }
    int totalH = cy;
    int& sc = g_scroll[g_tab];
    int s = g_sel[g_tab];
    if (s >= 0 && s < (int)g_items.size()) {
        int st = (s > 0 && g_items[s - 1].header) ? top[s - 1] : top[s];
        int sb = top[s] + 70;
        if (st - sc < 0) sc = st;
        if (sb - sc > h) sc = sb - h;
    }
    sc = std::max(0, std::min(sc, std::max(0, totalH - h)));

    gfx::clip(x - 10, y0 - 6, w + 20, h + 12);
    for (size_t i = 0; i < g_items.size(); i++) {
        const Item& it = g_items[i];
        int iy = y0 + top[i] - sc;
        int ih = it.header ? 46 : 70;
        if (iy + ih < y0 - 6 || iy > y0 + h + 6) continue;
        if (it.header) {
            gfx::text(it.title, x + 6, iy + 14, 19, t.accent, gfx::Left, w - 12);
            continue;
        }
        bool focus = (int)i == s && g_focusContent;
        if (focus) {
            gfx::roundRect(x - 3, iy - 3, w + 6, 76, 13, t.accent);
            gfx::roundRect(x, iy, w, 70, 11, t.itemSel);
        } else {
            gfx::roundRect(x, iy, w, 70, 11, t.item);
        }
        int valW = it.value.empty() ? 0 : std::min(gfx::measure(it.value, 20), 340) + 30;
        int textW = w - 44 - valW;
        if (it.sub.empty()) {
            gfx::text(it.title, x + 22, iy + 21, 23, t.text, gfx::Left, textW);
        } else {
            gfx::text(it.title, x + 22, iy + 9, 23, t.text, gfx::Left, textW);
            gfx::text(it.sub, x + 22, iy + 40, 17, t.sub, gfx::Left, textW);
        }
        if (!it.value.empty())
            gfx::text(it.value, x + w - 22, iy + 22, 20, it.vcol.a ? it.vcol : t.sub, gfx::Right, 340);
    }
    gfx::noClip();

    if (totalH > h) {
        int bh = std::max(30, h * h / totalH);
        int by = y0 + (h - bh) * sc / std::max(1, totalH - h);
        gfx::roundRect(1262, by, 6, bh, 3, t.sub);
    }
}

void drawContent() {
    const Theme& t = T();
    const char* titles[TAB_COUNT] = {tr("tab_cfw"), tr("tab_apps"), tr("tab_settings"), tr("tab_about")};
    gfx::text(titles[g_tab], 336, 34, 34, t.text);
    if (g_tab == TAB_APPS) {
        if (g_storeState == STORE_OK) {
            std::string info = std::string(tr("category")) + ": " + catLabel(g_cat);
            if (!g_search.empty()) info += "   ·   " + std::string(tr("search")) + ": \"" + g_search + "\"";
            int count = 0;
            for (auto& it : g_items) if (it.act) count++;
            info += "   ·   " + util::fmt(tr("apps_count"), std::to_string(count));
            gfx::text(info, 338, 84, 18, t.sub, gfx::Left, 900);
        } else if (g_storeState == STORE_LOADING) {
            gfx::text(tr("loading_catalog"), 790, 360, 22, t.sub, gfx::Center);
        }
    }
    drawList();
}

void drawTask() {
    const Theme& t = T();
    Task* task = g_task.get();
    gfx::rect(0, 0, 1280, 720, t.overlay);
    const int bw = 720, bh = 236, bx = (1280 - bw) / 2, by = (720 - bh) / 2;
    gfx::roundRect(bx, by, bw, bh, 18, t.side);
    gfx::text(task->title, bx + 36, by + 28, 28, t.text, gfx::Left, bw - 72);
    gfx::text(task->ctx.status(), bx + 36, by + 80, 20, t.sub, gfx::Left, bw - 72);
    const int barX = bx + 36, barY = by + 132, barW = bw - 72, barH = 14;
    gfx::roundRect(barX, barY, barW, barH, 7, t.item);
    uint64_t done = task->ctx.done, total = task->ctx.total;
    if (total > 0) {
        int fill = (int)(barW * std::min<uint64_t>(done, total) / total);
        if (fill > 0) gfx::roundRect(barX, barY, std::max(fill, barH), barH, 7, t.accent);
        char pct[16];
        snprintf(pct, sizeof pct, "%d%%", (int)(done * 100 / total));
        gfx::text(pct, barX, barY + 28, 18, t.text);
        gfx::text(util::humanSize(done) + " / " + util::humanSize(total), barX + barW, barY + 28, 18, t.sub, gfx::Right);
    } else {
        // Barra indeterminada animada
        int seg = barW / 4;
        int pos = (int)((SDL_GetTicks() / 4) % (barW + seg)) - seg;
        int sx = std::max(0, pos), ex = std::min(barW, pos + seg);
        if (ex > sx) gfx::roundRect(barX + sx, barY, ex - sx, barH, 7, t.accent);
        if (done > 0) gfx::text(util::humanSize(done), barX, barY + 28, 18, t.sub);
    }
}

void drawDialog() {
    const Theme& t = T();
    gfx::rect(0, 0, 1280, 720, t.overlay);
    const int bw = 840, wrapW = bw - 72;
    int bodyH = std::min(gfx::wrappedHeight(g_dlg.body, 20, t.text, wrapW), 380);
    int bh = 28 + 40 + 20 + bodyH + 30 + 56 + 28;
    int bx = (1280 - bw) / 2, by = (720 - bh) / 2;
    gfx::roundRect(bx, by, bw, bh, 18, t.side);
    gfx::text(g_dlg.title, bx + 36, by + 28, 28, t.text, gfx::Left, bw - 72);
    gfx::clip(bx + 36, by + 88, wrapW, bodyH);
    gfx::textWrapped(g_dlg.body, bx + 36, by + 88, 20, t.text, wrapW);
    gfx::noClip();
    int n = (int)g_dlg.buttons.size();
    int btnW = (bw - 72 - (n - 1) * 16) / n;
    int btnY = by + bh - 28 - 56;
    for (int i = 0; i < n; i++) {
        int bxi = bx + 36 + i * (btnW + 16);
        bool sel = i == g_dlg.sel;
        gfx::roundRect(bxi, btnY, btnW, 56, 12, sel ? t.accent : t.item);
        gfx::text(g_dlg.buttons[i], bxi + btnW / 2, btnY + 14, 22, sel ? t.onAccent : t.text, gfx::Center, btnW - 20);
    }
}

void render() {
    const Theme& t = T();
    gfx::clear(t.bg);
    drawSidebar();
    drawContent();
    gfx::rect(300, 654, 980, 1, t.item);

    std::vector<Hint> hints;
    if (g_task) {
        hints = {{GLYPH_B, tr("cancel")}};
    } else if (g_dlg.open) {
        hints = {{GLYPH_A, tr("select")}, {GLYPH_B, tr("back")}};
    } else {
        if (g_tab == TAB_APPS && g_storeState == STORE_OK)
            hints = {{GLYPH_X, tr("category")}, {GLYPH_Y, tr("search")}};
        hints.push_back({GLYPH_L, ""});
        hints.push_back({GLYPH_R, tr("tabs")});
        hints.push_back({GLYPH_A, tr("select")});
        if (g_focusContent) hints.push_back({GLYPH_B, tr("back")});
        hints.push_back({GLYPH_PLUS, tr("exit")});
    }
    drawHints(hints);

    if (g_task) drawTask();
    else if (g_dlg.open) drawDialog();
}

} // namespace

void appRun(const char* argv0) {
    g_nroPath = (argv0 && util::startsWith(argv0, "sdmc:/")) ? argv0 : "sdmc:/switch/bagonx/bagonx.nro";
    padInitializeDefault(&g_pad);
    refreshSys();

    while (g_running && appletMainLoop()) {
        SDL_Event e;
        while (SDL_PollEvent(&e))
            if (e.type == SDL_QUIT) g_running = false;

        uint32_t k = readInput();
        pollTask();
        if (g_task) {
            if (k & K_B) g_task->ctx.cancel = true;
        } else if (g_dlg.open) {
            handleDialog(k);
        } else {
            handleMain(k);
        }
        if (g_dirty) rebuild();
        render();
        gfx::present();
    }

    if (g_task) {
        g_task->ctx.cancel = true;
        pthread_join(g_task->thread, nullptr);
        g_task.reset();
        appletSetMediaPlaybackState(false);
    }
}
