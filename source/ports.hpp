#pragma once
// Ports de juegos: catálogo ports.json del repositorio de BagoNX. Cada port se
// descarga de la última release de su propio GitHub.
#include <string>
#include <vector>
#include "task.hpp"

namespace ports {

struct Port {
    std::string id, title, system, author, repo, asset, type, dest, check, desc;
    std::string dataDir;  // dónde van tus archivos del juego (vacío si no hacen falta)
    std::string manifest; // lista "sha256 tamaño ruta" que trae el port, para comprobar tu copia
    bool needsData = false;
    std::string installedVer; // vacío si no está instalado; "?" si está pero sin registro
};

bool fetch(const std::string& repo, std::vector<Port>& out, std::string& err);
void refreshInstalled(std::vector<Port>& list);
bool install(const Port& p, TaskCtx& c, std::string& msg);

} // namespace ports
