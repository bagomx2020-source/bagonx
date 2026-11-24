#pragma once
// Parser JSON mínimo (solo lectura) + escape para escribir.
#include <string>
#include <vector>

namespace json {

struct Value {
    enum Type { Null, Bool, Number, String, Array, Object };
    Type type = Null;
    bool b = false;
    double n = 0;
    std::string s;
    std::vector<std::string> keys; // solo Object
    std::vector<Value> vals;       // Array u Object

    const Value& operator[](const std::string& key) const;
    const Value& at(size_t i) const;
    size_t size() const { return (type == Array || type == Object) ? vals.size() : 0; }
    std::string str(const std::string& def = "") const;
    double num(double def = 0) const;
    bool isArray() const { return type == Array; }
    bool isObject() const { return type == Object; }
};

bool parse(const std::string& text, Value& out);
std::string escape(const std::string& s);

} // namespace json
