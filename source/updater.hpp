#pragma once
// Autoactualización desde las releases de GitHub del propio proyecto.
#include <string>
#include "task.hpp"

namespace updater {

bool check(const std::string& repo, std::string& tag, std::string& url, TaskCtx& c, std::string& msg);
bool apply(const std::string& url, const std::string& nroPath, TaskCtx& c, std::string& msg);

} // namespace updater
