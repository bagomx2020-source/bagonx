#pragma once
#include <string>
#include <vector>
#include <functional>
#include <cstdint>

namespace zip {

// Devuelve la ruta de destino para una entrada del zip, o "" para saltarla.
using Mapper = std::function<std::string(const std::string& name)>;
// Devuelve false para cancelar.
using Progress = std::function<bool(uint64_t done, uint64_t total)>;

bool extract(const std::string& zipPath, Mapper mapper, Progress progress,
             std::vector<std::string>* written, std::string& err);
bool readEntry(const std::string& zipPath, const std::string& name, std::string& out);

} // namespace zip
