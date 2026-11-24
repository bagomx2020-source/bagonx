#pragma once
#include <string>
#include <vector>
#include <cstdint>

namespace util {

bool exists(const std::string& path);
bool mkdirs(const std::string& dir);              // crea la carpeta y sus padres
std::string parent(const std::string& path);      // "sdmc:/a/b.txt" -> "sdmc:/a"
bool readFile(const std::string& path, std::string& out);
bool writeFile(const std::string& path, const std::string& data);
bool copyFile(const std::string& from, const std::string& to);

bool startsWith(const std::string& s, const std::string& p);
bool endsWith(const std::string& s, const std::string& p);
std::string trim(const std::string& s);
std::string lower(std::string s);
std::vector<std::string> lines(const std::string& s);

// Compara "v1.2.3" con "1.10": <0, 0, >0
int versionCompare(const std::string& a, const std::string& b);
std::string humanSize(uint64_t bytes);
// Sustituye el primer "{}" de tpl por arg
std::string fmt(const char* tpl, const std::string& arg);
// Corta en un límite de bytes sin romper caracteres UTF-8
std::string utf8Cut(const std::string& s, size_t maxBytes);

} // namespace util
