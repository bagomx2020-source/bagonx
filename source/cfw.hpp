#pragma once
#include <string>
#include "task.hpp"

namespace cfw {

bool installAtmosphere(TaskCtx& c, std::string& msg);
bool installHekate(TaskCtx& c, std::string& msg);
bool installBoth(TaskCtx& c, std::string& msg);

} // namespace cfw
