#include "config.hpp"
#include "json.hpp"
#include "util.hpp"

static Config g_cfg;

Config& cfg() { return g_cfg; }

void cfgLoad() {
    std::string s;
    json::Value v;
    if (util::readFile(CONFIG_PATH, s) && json::parse(s, v)) {
        g_cfg.lang = v["language"].str(g_cfg.lang);
        g_cfg.theme = v["theme"].str(g_cfg.theme);
        std::string repo = v["update_repo"].str();
        if (!repo.empty()) g_cfg.updateRepo = repo;
        if (v["auto_update"].type == json::Value::Bool) g_cfg.autoUpdate = v["auto_update"].b;
    } else {
        cfgSave();
    }
}

void cfgSave() {
    std::string s = "{\n";
    s += "  \"language\": \"" + json::escape(g_cfg.lang) + "\",\n";
    s += "  \"theme\": \"" + json::escape(g_cfg.theme) + "\",\n";
    s += "  \"update_repo\": \"" + json::escape(g_cfg.updateRepo) + "\",\n";
    s += std::string("  \"auto_update\": ") + (g_cfg.autoUpdate ? "true" : "false") + "\n";
    s += "}\n";
    util::writeFile(CONFIG_PATH, s);
}
