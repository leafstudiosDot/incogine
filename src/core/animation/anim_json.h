// Incogine — minimal JSON value, parser, and writer (no third-party deps).
// Part of Incogine by leafstudiosDot (MPL-2.0). See LICENSE.
//
// Why hand-rolled: `.incoanim` has to be readable by the SDL3 runtime, which
// cannot depend on Qt, and the repo carries no JSON library. This mirrors the
// existing stdlib-only codec approach (`incoba_detail.h`, `xml_util.h`) — and
// the scope is deliberately small: it has to read back exactly what
// `anim_json.cpp`'s writer produces.
//
// Object keys keep insertion order rather than sorting, so the writer emits a
// stable, human-readable, diff-friendly layout instead of alphabetical noise.

#pragma once

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace icg {
namespace anim {
namespace json {

class JsonValue;

using JsonObject = std::vector<std::pair<std::string, JsonValue>>;
using JsonArray = std::vector<JsonValue>;

class JsonValue {
public:
    enum class Type { Null, Bool, Number, String, Array, Object };

    JsonValue() = default;
    static JsonValue Null();
    static JsonValue Bool(bool value);
    static JsonValue Number(double value);
    static JsonValue Int(long long value);
    static JsonValue String(std::string value);
    static JsonValue Array(JsonArray value);
    static JsonValue Object(JsonObject value);

    Type GetType() const { return type_; }
    bool IsNull() const { return type_ == Type::Null; }
    bool IsBool() const { return type_ == Type::Bool; }
    bool IsNumber() const { return type_ == Type::Number; }
    bool IsString() const { return type_ == Type::String; }
    bool IsArray() const { return type_ == Type::Array; }
    bool IsObject() const { return type_ == Type::Object; }

    // Typed accessors. Each returns `fallback` when the type does not match, so
    // a hand-edited file with a wrong-typed field degrades instead of failing
    // the whole load — the loader validates required fields separately.
    bool AsBool(bool fallback = false) const;
    double AsNumber(double fallback = 0.0) const;
    long long AsInt(long long fallback = 0) const;
    const std::string& AsString() const;
    std::string AsStringOr(const std::string& fallback) const;
    const JsonArray& AsArray() const;
    JsonArray& AsArrayMutable();
    const JsonObject& AsObject() const;
    JsonObject& AsObjectMutable();

    // Object lookup. Returns nullptr when absent (or when this is not an object).
    const JsonValue* Find(const std::string& key) const;
    // Object lookup that inserts a null when absent.
    JsonValue* FindOrAdd(const std::string& key);
    bool Has(const std::string& key) const { return Find(key) != nullptr; }

    // Array element access. Out-of-range returns nullptr.
    const JsonValue* At(size_t index) const;
    size_t Size() const;

private:
    Type type_ = Type::Null;
    bool bool_ = false;
    double number_ = 0.0;
    std::string string_;
    JsonArray array_;
    JsonObject object_;
};

// Parses `text`. Returns false and fills `error` (with a byte offset) on
// malformed input. Trailing whitespace is fine; trailing garbage is not.
bool Parse(const std::string& text, JsonValue& out, std::string& error);

// Serializes with 2-space indent and a trailing newline. Object keys are
// emitted in insertion order.
std::string Write(const JsonValue& value);

// Escapes a string as a JSON string literal, including the surrounding quotes.
std::string EscapeString(const std::string& text);

} // namespace json
} // namespace anim
} // namespace icg