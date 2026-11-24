#pragma once
#include <string>

void i18nSetLang(const std::string& lang); // "es" o "en"
const char* tr(const char* key);
