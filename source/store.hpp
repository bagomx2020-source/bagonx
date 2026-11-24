#pragma once
// Tienda de apps homebrew usando el catálogo público de la Homebrew App Store.
#include <string>
#include <vector>
#include <cstdint>
#include "task.hpp"

namespace store {

struct Pkg {
    std::string name, title, author, version, category, desc, details;
    uint64_t sizeKB = 0;
    std::string installedVer; // vacío si no está instalada
};

bool fetch(std::vector<Pkg>& out, std::string& err);
void refreshInstalled(std::vector<Pkg>& pkgs);
bool install(const Pkg& p, TaskCtx& c, std::string& msg);
bool uninstall(const Pkg& p, std::string& msg);

} // namespace store
