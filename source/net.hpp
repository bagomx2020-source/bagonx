#pragma once
#include <string>
#include <vector>
#include <functional>

namespace net {

// Devuelve false para cancelar.
using Progress = std::function<bool(double now, double total)>;

struct Release {
    std::string tag;
    std::vector<std::pair<std::string, std::string>> assets; // nombre, url
};

void init();
void exit();
bool get(const std::string& url, std::string& out, std::string* err = nullptr, bool github = false);
bool download(const std::string& url, const std::string& path, Progress cb, std::string* err = nullptr);
bool githubLatest(const std::string& repo, Release& out, std::string& err);

} // namespace net
