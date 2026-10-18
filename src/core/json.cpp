#include "core/json.hpp"

#include <charconv>
#include <cstdint>
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <limits>
#include <sstream>

namespace mtg::json {
namespace {

void append_utf8(std::string& out, std::uint32_t cp) {
    if (cp <= 0x7F) {
        out.push_back(static_cast<char>(cp));
    } else if (cp <= 0x7FF) {
        out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else if (cp <= 0xFFFF) {
        out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else if (cp <= 0x10FFFF) {
        out.push_back(static_cast<char>(0xF0 | (cp >> 18)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else {
        throw Error("invalid unicode code point");
    }
}

int hex_value(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return 10 + c - 'a';
    if (c >= 'A' && c <= 'F') return 10 + c - 'A';
    return -1;
}

std::string escape_string(std::string_view text) {
    std::ostringstream out;
    out << '"';
    for (char raw : text) {
        const unsigned char c = static_cast<unsigned char>(raw);
        switch (c) {
        case '"': out << "\\\""; break;
        case '\\': out << "\\\\"; break;
        case '\b': out << "\\b"; break;
        case '\f': out << "\\f"; break;
        case '\n': out << "\\n"; break;
        case '\r': out << "\\r"; break;
        case '\t': out << "\\t"; break;
        default:
            if (c < 0x20) {
                out << "\\u" << std::hex << std::setw(4) << std::setfill('0') << static_cast<int>(c) << std::dec;
            } else {
                out << static_cast<char>(c);
            }
        }
    }
    out << '"';
    return out.str();
}

class Parser {
public:
    explicit Parser(std::string_view input) : input_(input) {}

    Value parse_document() {
        skip_ws();
        Value value = parse_value();
        skip_ws();
        if (pos_ != input_.size()) fail("trailing characters after JSON value");
        return value;
    }

private:
    std::string_view input_;
    std::size_t pos_ = 0;

    [[noreturn]] void fail(std::string_view message) const {
        throw Error(std::string(message) + " at byte " + std::to_string(pos_));
    }

    void skip_ws() {
        while (pos_ < input_.size()) {
            const char c = input_[pos_];
            if (c == ' ' || c == '\n' || c == '\r' || c == '\t') ++pos_;
            else break;
        }
    }

    bool consume(char expected) {
        if (pos_ < input_.size() && input_[pos_] == expected) {
            ++pos_;
            return true;
        }
        return false;
    }

    void expect(char expected) {
        if (!consume(expected)) fail(std::string("expected '") + expected + "'");
    }

    Value parse_value() {
        if (pos_ >= input_.size()) fail("unexpected end of input");
        switch (input_[pos_]) {
        case 'n': return parse_null();
        case 't': return parse_true();
        case 'f': return parse_false();
        case '"': return Value(parse_string());
        case '[': return parse_array();
        case '{': return parse_object();
        default:
            if (input_[pos_] == '-' || (input_[pos_] >= '0' && input_[pos_] <= '9')) {
                return Value(parse_number());
            }
            fail("unexpected token");
        }
    }

    Value parse_null() {
        if (input_.substr(pos_, 4) != "null") fail("invalid null literal");
        pos_ += 4;
        return Value(nullptr);
    }

    Value parse_true() {
        if (input_.substr(pos_, 4) != "true") fail("invalid true literal");
        pos_ += 4;
        return Value(true);
    }

    Value parse_false() {
        if (input_.substr(pos_, 5) != "false") fail("invalid false literal");
        pos_ += 5;
        return Value(false);
    }

    std::uint32_t parse_hex4() {
        if (pos_ + 4 > input_.size()) fail("incomplete unicode escape");
        std::uint32_t value = 0;
        for (int i = 0; i < 4; ++i) {
            const int h = hex_value(input_[pos_++]);
            if (h < 0) fail("invalid hex digit in unicode escape");
            value = (value << 4) | static_cast<std::uint32_t>(h);
        }
        return value;
    }

    std::string parse_string() {
        expect('"');
        std::string out;
        while (pos_ < input_.size()) {
            const unsigned char c = static_cast<unsigned char>(input_[pos_++]);
            if (c == '"') return out;
            if (c < 0x20) fail("control character in string");
            if (c != '\\') {
                out.push_back(static_cast<char>(c));
                continue;
            }

            if (pos_ >= input_.size()) fail("incomplete escape sequence");
            const char esc = input_[pos_++];
            switch (esc) {
            case '"': out.push_back('"'); break;
            case '\\': out.push_back('\\'); break;
            case '/': out.push_back('/'); break;
            case 'b': out.push_back('\b'); break;
            case 'f': out.push_back('\f'); break;
            case 'n': out.push_back('\n'); break;
            case 'r': out.push_back('\r'); break;
            case 't': out.push_back('\t'); break;
            case 'u': {
                std::uint32_t cp = parse_hex4();
                if (cp >= 0xD800 && cp <= 0xDBFF) {
                    if (pos_ + 2 > input_.size() || input_[pos_] != '\\' || input_[pos_ + 1] != 'u') {
                        fail("high surrogate without low surrogate");
                    }
                    pos_ += 2;
                    const std::uint32_t low = parse_hex4();
                    if (low < 0xDC00 || low > 0xDFFF) fail("invalid low surrogate");
                    cp = 0x10000 + ((cp - 0xD800) << 10) + (low - 0xDC00);
                } else if (cp >= 0xDC00 && cp <= 0xDFFF) {
                    fail("unexpected low surrogate");
                }
                append_utf8(out, cp);
                break;
            }
            default: fail("invalid escape sequence");
            }
        }
        fail("unterminated string");
    }

    double parse_number() {
        const std::size_t begin = pos_;
        if (consume('-')) {}
        if (pos_ >= input_.size()) fail("incomplete number");

        if (consume('0')) {
        } else {
            if (input_[pos_] < '1' || input_[pos_] > '9') fail("invalid number");
            while (pos_ < input_.size() && input_[pos_] >= '0' && input_[pos_] <= '9') ++pos_;
        }

        if (consume('.')) {
            if (pos_ >= input_.size() || input_[pos_] < '0' || input_[pos_] > '9') fail("invalid fraction");
            while (pos_ < input_.size() && input_[pos_] >= '0' && input_[pos_] <= '9') ++pos_;
        }

        if (pos_ < input_.size() && (input_[pos_] == 'e' || input_[pos_] == 'E')) {
            ++pos_;
            if (pos_ < input_.size() && (input_[pos_] == '+' || input_[pos_] == '-')) ++pos_;
            if (pos_ >= input_.size() || input_[pos_] < '0' || input_[pos_] > '9') fail("invalid exponent");
            while (pos_ < input_.size() && input_[pos_] >= '0' && input_[pos_] <= '9') ++pos_;
        }

        const std::string owned(input_.substr(begin, pos_ - begin));
        char* end = nullptr;
        const double value = std::strtod(owned.c_str(), &end);
        if (!end || *end != '\0' || !std::isfinite(value)) fail("invalid numeric value");
        return value;
    }

    Value parse_array() {
        expect('[');
        skip_ws();
        Value::Array array;
        if (consume(']')) return Value(std::move(array));
        for (;;) {
            skip_ws();
            array.push_back(parse_value());
            skip_ws();
            if (consume(']')) return Value(std::move(array));
            expect(',');
            skip_ws();
        }
    }

    Value parse_object() {
        expect('{');
        skip_ws();
        Value::Object object;
        if (consume('}')) return Value(std::move(object));
        for (;;) {
            skip_ws();
            if (pos_ >= input_.size() || input_[pos_] != '"') fail("object key must be a string");
            std::string key = parse_string();
            skip_ws();
            expect(':');
            skip_ws();
            object.insert_or_assign(std::move(key), parse_value());
            skip_ws();
            if (consume('}')) return Value(std::move(object));
            expect(',');
            skip_ws();
        }
    }
};

} // namespace

bool Value::is_null() const { return std::holds_alternative<std::nullptr_t>(data_); }
bool Value::is_bool() const { return std::holds_alternative<bool>(data_); }
bool Value::is_number() const { return std::holds_alternative<double>(data_); }
bool Value::is_string() const { return std::holds_alternative<std::string>(data_); }
bool Value::is_array() const { return std::holds_alternative<Array>(data_); }
bool Value::is_object() const { return std::holds_alternative<Object>(data_); }

bool Value::as_bool() const {
    if (!is_bool()) throw Error("JSON value is not a boolean");
    return std::get<bool>(data_);
}

double Value::as_number() const {
    if (!is_number()) throw Error("JSON value is not a number");
    return std::get<double>(data_);
}

const std::string& Value::as_string() const {
    if (!is_string()) throw Error("JSON value is not a string");
    return std::get<std::string>(data_);
}

const Value::Array& Value::as_array() const {
    if (!is_array()) throw Error("JSON value is not an array");
    return std::get<Array>(data_);
}

const Value::Object& Value::as_object() const {
    if (!is_object()) throw Error("JSON value is not an object");
    return std::get<Object>(data_);
}

const Value* Value::find(std::string_view key) const {
    if (!is_object()) return nullptr;
    const auto& object = std::get<Object>(data_);
    const auto it = object.find(key);
    return it == object.end() ? nullptr : &it->second;
}

std::string Value::dump() const {
    if (is_null()) return "null";
    if (is_bool()) return as_bool() ? "true" : "false";
    if (is_number()) {
        std::ostringstream out;
        out << std::setprecision(17) << as_number();
        return out.str();
    }
    if (is_string()) return escape_string(as_string());
    if (is_array()) {
        std::string out = "[";
        bool first = true;
        for (const auto& value : as_array()) {
            if (!first) out += ',';
            first = false;
            out += value.dump();
        }
        out += ']';
        return out;
    }
    std::string out = "{";
    bool first = true;
    for (const auto& [key, value] : as_object()) {
        if (!first) out += ',';
        first = false;
        out += escape_string(key);
        out += ':';
        out += value.dump();
    }
    out += '}';
    return out;
}

Value parse(std::string_view text) { return Parser(text).parse_document(); }

std::string string_or(const Value& object, std::string_view key, std::string fallback) {
    const Value* value = object.find(key);
    return value && value->is_string() ? value->as_string() : std::move(fallback);
}

double number_or(const Value& object, std::string_view key, double fallback) {
    const Value* value = object.find(key);
    return value && value->is_number() ? value->as_number() : fallback;
}

int int_or(const Value& object, std::string_view key, int fallback) {
    const double value = number_or(object, key, static_cast<double>(fallback));
    if (value > static_cast<double>(std::numeric_limits<int>::max()) ||
        value < static_cast<double>(std::numeric_limits<int>::min())) return fallback;
    return static_cast<int>(value);
}

bool bool_or(const Value& object, std::string_view key, bool fallback) {
    const Value* value = object.find(key);
    return value && value->is_bool() ? value->as_bool() : fallback;
}

std::string dump_or(const Value& object, std::string_view key, std::string fallback) {
    const Value* value = object.find(key);
    return value ? value->dump() : std::move(fallback);
}

} // namespace mtg::json
