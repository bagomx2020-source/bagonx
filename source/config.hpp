#pragma once
#include <string>

#define CONFIG_DIR "sdmc:/config/bagonx"
#define CONFIG_PATH CONFIG_DIR "/config.json"
#define TMP_DIR CONFIG_DIR "/tmp"

#ifndef DEFAULT_UPDATE_REPO
#define DEFAULT_UPDATE_REPO ""
#endif

struct Config {
    std::string lang = "es";
    std::string theme = "dark";
    std::string updateRepo = DEFAULT_UPDATE_REPO; // "usuario/repositorio" en GitHub
    bool autoUpdate = true;                       // comprobar al arrancar
};

Config& cfg();
void cfgLoad();
void cfgSave();
