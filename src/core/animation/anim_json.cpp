#include "anim_json.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace icg {
namespace anim {
namespace json {
namespace {

const std::string& EmptyString() {
    static const std::string kEmpty;
    return kEmpty;
}

const JsonArray& EmptyArray() {
    static const JsonArray kEmpty;
    return kEmpty;
}

const JsonObject& EmptyObject() {
    static const JsonObject kEmpty;
    return kEmpty;
}

// --- writer --

void WriteNumber(double value, std::string& out) {
    // Whole values print without a decimal point, so stage sizes and frame
    // indices stay as "1920" rather than "1920.0".
    if (value == std::floor(value) && std::fabs(value) < 9.0e15) {
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%lld",
                      static_cast<long long>(value));
        out += buf;
        return;
    }
    char buf[40];
    std::snprintf(buf, sizeof(buf), "%.17g", value);
    // Trim to the shortest representation that still round-trips.
    for (int precision = 6; precision < 17; ++precision) {
        char candidate[40];
        std::snprintf(candidate, sizeof(candidate), "%.*g", precision, value);
        if (std::strtod(candidate, nullptr) == value) {
            out += candidate;
            return;
        }
    }
    out += buf;
}

void WriteIndent(std::string& out, int depth) {
    for (int i = 0; i < depth; ++i) {
        out += "  ";
    }
}

// True when every element is a scalar, i.e. the array can be written on one
// line. Coordinate arrays like `[100, 120]` are far more readable inline, and
// a vector path would otherwise burn six lines per Bezier control point.
bool AllScalar(const JsonArray& items) {
    for (const JsonValue& item : items) {
        switch (item.GetType()) {
            case JsonValue::Type::Array:
            case JsonValue::Type::Object:
                return false;
            default:
                break;
        }
    }
    return true;
}

void WriteValue(const JsonValue& value, std::string& out, int depth);

void WriteArray(const JsonArray& items, std::string& out, int depth) {
    if (items.empty()) {
        out += "[]";
        return;
    }
    if (AllScalar(items)) {
        out += "[";
        for (size_t i = 0; i < items.size(); ++i) {
            if (i != 0) {
                out += ", ";
            }
            WriteValue(items[i], out, depth);
        }
        out += "]";
        return;
    }
    out += "[\n";
    for (size_t i = 0; i < items.size(); ++i) {
        WriteIndent(out, depth + 1);
        WriteValue(items[i], out, depth + 1);
        if (i + 1 < items.size()) {
            out += ",";
        }
        out += "\n";
    }
    WriteIndent(out, depth);
    out += "]";
}

void WriteObject(const JsonObject& members, std::string& out, int depth) {
    if (members.empty()) {
        out += "{}";
        return;
    }
    out += "{\n";
    for (size_t i = 0; i < members.size(); ++i) {
        WriteIndent(out, depth + 1);
        out += EscapeString(members[i].first);
        out += ": ";
        WriteValue(members[i].second, out, depth + 1);
        if (i + 1 < members.size()) {
            out += ",";
        }
        out += "\n";
    }
    WriteIndent(out, depth);
    out += "}";
}

void WriteValue(const JsonValue& value, std::string& out, int depth) {
    switch (value.GetType()) {
        case JsonValue::Type::Null:
            out += "null";
            break;
        case JsonValue::Type::Bool:
            out += value.AsBool() ? "true" : "false";
            break;
        case JsonValue::Type::Number:
            WriteNumber(value.AsNumber(), out);
            break;
        case JsonValue::Type::String:
            out += EscapeString(value.AsString());
            break;
        case JsonValue::Type::Array:
            WriteArray(value.AsArray(), out, depth);
            break;
        case JsonValue::Type::Object:
            WriteObject(value.AsObject(), out, depth);
            break;
    }
}

// --- parser --

class Parser {
public:
    Parser(const std::string& text) : text_(text) {}

    bool Run(JsonValue& out, std::string& error) {
        SkipWhitespace();
        if (!ParseValue(out, 0)) {
            error = error_;
            return false;
        }
        SkipWhitespace();
        if (pos_ != text_.size()) {
            error = Fail("trailing content after top-level value");
            return false;
        }
        return true;
    }

private:
    static constexpr int kMaxDepth = 64;

    const std::string& text_;
    size_t pos_ = 0;
    std::string error_;

    std::string Fail(const char* what) {
        if (error_.empty()) {
            error_ = std::string(what) + " at offset " +
                     std::to_string(pos_);
        }
        return error_;
    }

    void SkipWhitespace() {
        while (pos_ < text_.size()) {
            const char c = text_[pos_];
            if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
                ++pos_;
            } else {
                break;
            }
        }
    }

    bool AtEnd() const { return pos_ >= text_.size(); }
    char Peek() const { return pos_ < text_.size() ? text_[pos_] : '\0'; }

    bool Literal(const char* word) {
        const size_t len = std::char_traits<char>::length(word);
        if (text_.compare(pos_, len, word) != 0) {
            return false;
        }
        pos_ += len;
        return true;
    }

    bool ParseValue(JsonValue& out, int depth) {
        if (depth > kMaxDepth) {
            Fail("nesting too deep");
            return false;
        }
        SkipWhitespace();
        if (AtEnd()) {
            Fail("unexpected end of input");
            return false;
        }
        switch (Peek()) {
            case '{':
                return ParseObject(out, depth);
            case '[':
                return ParseArray(out, depth);
            case '"': {
                std::string s;
                if (!ParseString(s)) {
                    return false;
                }
                out = JsonValue::String(std::move(s));
                return true;
            }
            case 't':
                if (Literal("true")) {
                    out = JsonValue::Bool(true);
                    return true;
                }
                Fail("invalid literal");
                return false;
            case 'f':
                if (Literal("false")) {
                    out = JsonValue::Bool(false);
                    return true;
                }
                Fail("invalid literal");
                return false;
            case 'n':
                if (Literal("null")) {
                    out = JsonValue::Null();
                    return true;
                }
                Fail("invalid literal");
                return false;
            default:
                return ParseNumber(out);
        }
    }

    bool ParseNumber(JsonValue& out) {
        const size_t start = pos_;
        if (Peek() == '-' || Peek() == '+') {
            ++pos_;
        }
        while (!AtEnd()) {
            const char c = Peek();
            const bool numeric = (c >= '0' && c <= '9') || c == '.' || c == 'e' ||
                                 c == 'E' || c == '-' || c == '+';
            if (!numeric) {
                break;
            }
            ++pos_;
        }
        if (pos_ == start) {
            Fail("expected a value");
            return false;
        }
        const std::string token = text_.substr(start, pos_ - start);
        char* end = nullptr;
        const double value = std::strtod(token.c_str(), &end);
        if (end == nullptr || *end != '\0') {
            pos_ = start;
            Fail("malformed number");
            return false;
        }
        out = JsonValue::Number(value);
        return true;
    }

    bool ParseString(std::string& out) {
        if (Peek() != '"') {
            return false;
        }
        ++pos_;
        out.clear();
        while (true) {
            if (AtEnd()) {
                Fail("unterminated string");
                return false;
            }
            const char c = text_[pos_++];
            if (c == '"') {
                return true;
            }
            if (c != '\\') {
                out.push_back(c);
                continue;
            }
            if (AtEnd()) {
                Fail("unterminated escape");
                return false;
            }
            const char esc = text_[pos_++];
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
                    unsigned int code = 0;
                    if (!ParseHex4(code)) {
                        return false;
                    }
                    AppendUtf8(code, out);
                    break;
                }
                default:
                    Fail("unknown escape");
                    return false;
            }
        }
    }

    bool ParseHex4(unsigned int& out) {
        if (pos_ + 4 > text_.size()) {
            Fail("truncated \\u escape");
            return false;
        }
        out = 0;
        for (int i = 0; i < 4; ++i) {
            const char c = text_[pos_++];
            out <<= 4;
            if (c >= '0' && c <= '9') {
                out |= static_cast<unsigned int>(c - '0');
            } else if (c >= 'a' && c <= 'f') {
                out |= static_cast<unsigned int>(c - 'a' + 10);
            } else if (c >= 'A' && c <= 'F') {
                out |= static_cast<unsigned int>(c - 'A' + 10);
            } else {
                Fail("bad hex digit");
                return false;
            }
        }
        return true;
    }

    static void AppendUtf8(unsigned int code, std::string& out) {
        // Surrogate pairs: a high surrogate pairs with the next \u escape,
        // which the caller has already validated as hex.
        if (code < 0x80) {
            out.push_back(static_cast<char>(code));
        } else if (code < 0x800) {
            out.push_back(static_cast<char>(0xC0 | (code >> 6)));
            out.push_back(static_cast<char>(0x80 | (code & 0x3F)));
        } else {
            out.push_back(static_cast<char>(0xE0 | (code >> 12)));
            out.push_back(static_cast<char>(0x80 | ((code >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (code & 0x3F)));
        }
    }

    bool ParseArray(JsonValue& out, int depth) {
        ++pos_; // '['
        JsonArray items;
        SkipWhitespace();
        if (Peek() == ']') {
            ++pos_;
            out = JsonValue::Array(std::move(items));
            return true;
        }
        while (true) {
            JsonValue item;
            if (!ParseValue(item, depth + 1)) {
                return false;
            }
            items.push_back(std::move(item));
            SkipWhitespace();
            if (Peek() == ',') {
                ++pos_;
                continue;
            }
            if (Peek() == ']') {
                ++pos_;
                out = JsonValue::Array(std::move(items));
                return true;
            }
            Fail("expected ',' or ']'");
            return false;
        }
    }

    bool ParseObject(JsonValue& out, int depth) {
        ++pos_; // '{'
        JsonObject members;
        SkipWhitespace();
        if (Peek() == '}') {
            ++pos_;
            out = JsonValue::Object(std::move(members));
            return true;
        }
        while (true) {
            SkipWhitespace();
            std::string key;
            if (!ParseString(key)) {
                Fail("expected a member name");
                return false;
            }
            SkipWhitespace();
            if (Peek() != ':') {
                Fail("expected ':'");
                return false;
            }
            ++pos_;
            JsonValue value;
            if (!ParseValue(value, depth + 1)) {
                return false;
            }
            // Duplicate key: last one wins, matching most JSON readers.
            members.emplace_back(std::move(key), std::move(value));
            SkipWhitespace();
            if (Peek() == ',') {
                ++pos_;
                continue;
            }
            if (Peek() == '}') {
                ++pos_;
                out = JsonValue::Object(std::move(members));
                return true;
            }
            Fail("expected ',' or '}'");
            return false;
        }
    }
};

} // namespace

// ------------------------------------------------------------- JsonValue --

JsonValue JsonValue::Null() {
    return JsonValue();
}

JsonValue JsonValue::Bool(bool value) {
    JsonValue out;
    out.type_ = Type::Bool;
    out.bool_ = value;
    return out;
}

JsonValue JsonValue::Number(double value) {
    JsonValue out;
    out.type_ = Type::Number;
    out.number_ = value;
    return out;
}

JsonValue JsonValue::Int(long long value) {
    return Number(static_cast<double>(value));
}

JsonValue JsonValue::String(std::string value) {
    JsonValue out;
    out.type_ = Type::String;
    out.string_ = std::move(value);
    return out;
}

JsonValue JsonValue::Array(JsonArray value) {
    JsonValue out;
    out.type_ = Type::Array;
    out.array_ = std::move(value);
    return out;
}

JsonValue JsonValue::Object(JsonObject value) {
    JsonValue out;
    out.type_ = Type::Object;
    out.object_ = std::move(value);
    return out;
}

bool JsonValue::AsBool(bool fallback) const {
    return type_ == Type::Bool ? bool_ : fallback;
}

double JsonValue::AsNumber(double fallback) const {
    return type_ == Type::Number ? number_ : fallback;
}

long long JsonValue::AsInt(long long fallback) const {
    return type_ == Type::Number ? static_cast<long long>(number_) : fallback;
}

const std::string& JsonValue::AsString() const {
    return type_ == Type::String ? string_ : EmptyString();
}

std::string JsonValue::AsStringOr(const std::string& fallback) const {
    return type_ == Type::String ? string_ : fallback;
}

const JsonArray& JsonValue::AsArray() const {
    return type_ == Type::Array ? array_ : EmptyArray();
}

JsonArray& JsonValue::AsArrayMutable() {
    if (type_ != Type::Array) {
        type_ = Type::Array;
        array_.clear();
    }
    return array_;
}

const JsonObject& JsonValue::AsObject() const {
    return type_ == Type::Object ? object_ : EmptyObject();
}

JsonObject& JsonValue::AsObjectMutable() {
    if (type_ != Type::Object) {
        type_ = Type::Object;
        object_.clear();
    }
    return object_;
}

const JsonValue* JsonValue::Find(const std::string& key) const {
    if (type_ != Type::Object) {
        return nullptr;
    }
    for (const auto& member : object_) {
        if (member.first == key) {
            return &member.second;
        }
    }
    return nullptr;
}

JsonValue* JsonValue::FindOrAdd(const std::string& key) {
    if (type_ != Type::Object) {
        type_ = Type::Object;
        object_.clear();
    }
    for (auto& member : object_) {
        if (member.first == key) {
            return &member.second;
        }
    }
    object_.emplace_back(key, JsonValue());
    return &object_.back().second;
}

const JsonValue* JsonValue::At(size_t index) const {
    if (type_ != Type::Array || index >= array_.size()) {
        return nullptr;
    }
    return &array_[index];
}

size_t JsonValue::Size() const {
    if (type_ == Type::Array) {
        return array_.size();
    }
    if (type_ == Type::Object) {
        return object_.size();
    }
    return 0;
}

// ------------------------------------------------------------ public API --

std::string EscapeString(const std::string& text) {
    std::string out;
    out.reserve(text.size() + 2);
    out.push_back('"');
    for (char c : text) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\b': out += "\\b"; break;
            case '\f': out += "\\f"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    char buf[8];
                    std::snprintf(buf, sizeof(buf), "\\u%04x",
                                  static_cast<unsigned int>(
                                      static_cast<unsigned char>(c)));
                    out += buf;
                } else {
                    out.push_back(c);
                }
                break;
        }
    }
    out.push_back('"');
    return out;
}

bool Parse(const std::string& text, JsonValue& out, std::string& error) {
    error.clear();
    out = JsonValue();
    Parser parser(text);
    return parser.Run(out, error);
}

std::string Write(const JsonValue& value) {
    std::string out;
    WriteValue(value, out, 0);
    out.push_back('\n');
    return out;
}

} // namespace json
} // namespace anim
} // namespace icg