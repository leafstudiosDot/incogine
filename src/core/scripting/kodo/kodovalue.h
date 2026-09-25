// Kodo value model: dynamically-typed runtime values (see syntax-reference.md).
// Numbers are Int (int64) or Float (double) with promotion on arithmetic.

#ifndef KODOVALUE_H
#define KODOVALUE_H

#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

class Object;
class Audio;

namespace Kodo {

class Interpreter;
struct Function; // Defined in kodoast.h (AST + closure).
struct INativeFn; // Defined below (after Value is complete).
struct Value;

struct Vec3 {
    double x = 0.0, y = 0.0, z = 0.0;
};

struct ColorV { // 0..1 per channel (engine Color is 0..255; converted at the boundary)
    double r = 1.0, g = 1.0, b = 1.0, a = 1.0;
};

struct SpriteSpec {
    bool hasColor = false;
    ColorV color;
    bool hasTexture = false;
    std::string texture;
};

struct TransformSpec {
    Vec3 position;
    Vec3 scale{1.0, 1.0, 1.0};
    Vec3 rotation;
};

struct Value {
    enum class Type {
        Null, Bool, Int, Float, String, Array, Map, Function, NativeFn,
        Vec3, Color, ObjectRef, TransformRef, SpriteRef, AudioRef,
        SpriteSpec, TransformSpec, Module
    };

    Type type = Type::Null;
    bool boolean = false;
    int64_t integer = 0;
    double number = 0.0;
    std::string str;
    Vec3 vec{};
    ColorV color{};
    SpriteSpec spriteSpec{};
    TransformSpec transformSpec{};
    Object* object = nullptr; // Non-owning (ObjectRef/TransformRef/SpriteRef)
    std::shared_ptr<Audio> audio; // Owning (AudioRef)
    std::shared_ptr<std::vector<Value>> arr;
    std::shared_ptr<std::map<std::string, Value>> map;
    std::shared_ptr<Function> func;
    std::shared_ptr<INativeFn> native;
    std::string module; // Module name

    static Value Null() { return Value(); }
    static Value Bool(bool v) { Value x; x.type = Type::Bool; x.boolean = v; return x; }
    static Value Int(int64_t v) { Value x; x.type = Type::Int; x.integer = v; return x; }
    static Value Float(double v) { Value x; x.type = Type::Float; x.number = v; return x; }
    static Value String(const std::string& v) { Value x; x.type = Type::String; x.str = v; return x; }
    static Value Array() { Value x; x.type = Type::Array; x.arr = std::make_shared<std::vector<Value>>(); return x; }
    static Value Map() { Value x; x.type = Type::Map; x.map = std::make_shared<std::map<std::string, Value>>(); return x; }
    static Value Fn(std::shared_ptr<Function> f) { Value x; x.type = Type::Function; x.func = f; return x; }
    static Value Native(std::shared_ptr<INativeFn> f) { Value x; x.type = Type::NativeFn; x.native = std::move(f); return x; }
    static Value Vec(const Vec3& v) { Value x; x.type = Type::Vec3; x.vec = v; return x; }
    static Value Col(const ColorV& c) { Value x; x.type = Type::Color; x.color = c; return x; }
    static Value ObjRef(Object* o) { Value x; x.type = Type::ObjectRef; x.object = o; return x; }
    static Value TrRef(Object* o) { Value x; x.type = Type::TransformRef; x.object = o; return x; }
    static Value SpRef(Object* o) { Value x; x.type = Type::SpriteRef; x.object = o; return x; }
    static Value AudioRef(std::shared_ptr<Audio> a) { Value x; x.type = Type::AudioRef; x.audio = std::move(a); return x; }
    static Value SpSpec(const SpriteSpec& s) { Value x; x.type = Type::SpriteSpec; x.spriteSpec = s; return x; }
    static Value TrSpec(const TransformSpec& t) { Value x; x.type = Type::TransformSpec; x.transformSpec = t; return x; }
    static Value Mod(const std::string& name) { Value x; x.type = Type::Module; x.module = name; return x; }

    bool isNull() const { return type == Type::Null; }

    // Kodo truthiness: null, false, 0, 0.0 and "" are falsy; everything else truthy.
    bool truthy() const {
        switch (type) {
            case Type::Null: return false;
            case Type::Bool: return boolean;
            case Type::Int: return integer != 0;
            case Type::Float: return number != 0.0;
            case Type::String: return !str.empty();
            default: return true;
        }
    }

    // Numeric coercion (bool -> 0/1). Returns false when not numeric.
    bool asNumber(double& out) const {
        switch (type) {
            case Type::Int: out = (double)integer; return true;
            case Type::Float: out = number; return true;
            case Type::Bool: out = boolean ? 1.0 : 0.0; return true;
            default: return false;
        }
    }

    const char* typeName() const {
        switch (type) {
            case Type::Null: return "null";
            case Type::Bool: return "bool";
            case Type::Int: return "int";
            case Type::Float: return "float";
            case Type::String: return "string";
            case Type::Array: return "array";
            case Type::Map: return "object";
            case Type::Function: return "function";
            case Type::NativeFn: return "function";
            case Type::Vec3: return "Vector";
            case Type::Color: return "Color";
            case Type::ObjectRef: return "Object";
            case Type::TransformRef: return "Transform";
            case Type::SpriteRef: return "Sprite";
            case Type::AudioRef: return "Audio";
            case Type::SpriteSpec: return "Sprite";
            case Type::TransformSpec: return "Transform";
            case Type::Module: return module.c_str();
        }
        return "unknown";
    }
};

// Type-erased native callable. Defined after Value (above) so the
// virtual signature can use Value by value/reference directly.
struct INativeFn {
    virtual ~INativeFn() = default;
    virtual Value call(Interpreter&, const std::vector<Value>&, const std::map<std::string, Value>&) = 0;
};

template <typename F>
struct NativeFnImpl : INativeFn {
    F f;
    explicit NativeFnImpl(F fn) : f(std::move(fn)) {}
    Value call(Interpreter& ip, const std::vector<Value>& a, const std::map<std::string, Value>& n) override {
        return f(ip, a, n);
    }
};

// Wrap any (Interpreter&, args, named) -> Value callable as a script value.
template <typename F>
inline Value makeNative(F fn) {
    return Value::Native(std::make_shared<NativeFnImpl<F>>(std::move(fn)));
}

} // namespace Kodo

#endif
