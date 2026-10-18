#pragma once

#include <cstddef>
#include <map>
#include <stdexcept>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace mtg::json {

class Error : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

class Value {
public:
    using Array = std::vector<Value>;
    using Object = std::map<std::string, Value, std::less<>>;
    using Storage = std::variant<std::nullptr_t, bool, double, std::string, Array, Object>;

    Value() : data_(nullptr) {}
    explicit Value(std::nullptr_t) : data_(nullptr) {}
    explicit Value(bool value) : data_(value) {}
    explicit Value(double value) : data_(value) {}
    explicit Value(std::string value) : data_(std::move(value)) {}
    explicit Value(Array value) : data_(std::move(value)) {}
    explicit Value(Object value) : data_(std::move(value)) {}

    bool is_null() const;
    bool is_bool() const;
    bool is_number() const;
    bool is_string() const;
    bool is_array() const;
    bool is_object() const;

    bool as_bool() const;
    double as_number() const;
    const std::string& as_string() const;
    const Array& as_array() const;
    const Object& as_object() const;

    const Value* find(std::string_view key) const;
    std::string dump() const;

private:
    Storage data_;
};

Value parse(std::string_view text);

std::string string_or(const Value& object, std::string_view key, std::string fallback = {});
double number_or(const Value& object, std::string_view key, double fallback = 0.0);
int int_or(const Value& object, std::string_view key, int fallback = 0);
bool bool_or(const Value& object, std::string_view key, bool fallback = false);
std::string dump_or(const Value& object, std::string_view key, std::string fallback);

} // namespace mtg::json
