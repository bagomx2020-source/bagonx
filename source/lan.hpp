#pragma once
// Mis juegos (red local): copia los archivos de tus juegos desde tu PC, donde
// corre pc/bagonx_pc.py, sin subirlos a internet.
#include <cstdint>
#include <string>
#include <vector>
#include "task.hpp"

namespace lan {

struct File {
    std::string path, sha256;
    uint64_t size = 0;
};

struct Game {
    std::string name;
    uint64_t size = 0;
    std::vector<File> files;
};

// Resultado de comparar un juego con la lista de archivos que espera el port.
struct Check {
    bool hasManifest = false;
    int ok = 0, different = 0, missing = 0, extra = 0;
};

// "192.168.1.5" -> "http://192.168.1.5:8686"
std::string baseUrl(const std::string& addr);
bool list(const std::string& addr, const std::string& pin, std::vector<Game>& out, std::string& err);
Check checkManifest(const Game& g, const std::string& manifestPath);
bool copyGame(const std::string& addr, const std::string& pin, const Game& g, const std::string& destDir,
              TaskCtx& c, std::string& msg);

} // namespace lan
