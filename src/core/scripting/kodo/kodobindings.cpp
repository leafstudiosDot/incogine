// Kodo interpreter, part 3: global builtins + engine module bindings.
// Service calls (log/time/input/scene/save) reuse the shared C ABI in
// ../csharp/csharpinterop.h so both scripting languages behave the same.

#include "kodointerpreter.h"
#include "kodoengine.h"

#include <cctype>
#include <cstdio>
#include <iostream>
#include <random>

#include "../../assets/audio/audio.h"
#include "../../engine/math.h"
#include "../csharp/csharpinterop.h"

namespace Kodo {

namespace {

Vec3 requireVec3(Interpreter& ip, const Value& v, const std::string& what) {
    if (v.type != Value::Type::Vec3) ip.fail(what + " needs a Vector", 1);
    return v.vec;
}

double requireNum(Interpreter& ip, const Value& v, const std::string& what) {
    double out = 0.0;
    if (!v.asNumber(out)) ip.fail(what + " needs a number", 1);
    return out;
}

std::string requireStr(Interpreter& ip, const Value& v, const std::string& what) {
    if (v.type != Value::Type::String) ip.fail(what + " needs a string", 1);
    return v.str;
}

double clamp01(double c) {
    if (c < 0.0) return 0.0;
    if (c > 1.0) return 1.0;
    return c;
}

} // namespace

void Interpreter::installBuiltins() {
    globals->define("INFO", Value::Int(0));
    globals->define("WARN", Value::Int(1));
    globals->define("ERROR", Value::Int(2));
    globals->define("DEBUG", Value::Int(3));

    globals->define("log", makeNative([](Interpreter& ip, const std::vector<Value>& args,
                                         const std::map<std::string, Value>& named) {
        if (args.empty()) ip.fail("log(message) needs a message", 1);
        int level = 0;
        if (const Value* lv = internal::namedArg(named, "level")) {
            if (lv->type != Value::Type::Int) ip.fail("log level needs INFO/WARN/ERROR/DEBUG", 1);
            level = (int)lv->integer;
        } else if (args.size() > 1) {
            if (args[1].type != Value::Type::Int) ip.fail("log level needs INFO/WARN/ERROR/DEBUG", 1);
            level = (int)args[1].integer;
        }
        Incogine_Log(level, Interpreter::stringify(args[0]).c_str());
        return Value::Null();
    }));

    globals->define("logf", makeNative([](Interpreter& ip, const std::vector<Value>& args,
                                          const std::map<std::string, Value>& named) {
        if (args.empty() || args[0].type != Value::Type::String)
            ip.fail("logf(format, ...) needs a format string", 1);
        std::string out = args[0].str;
        // Named {key} substitution
        for (const auto& kv : named) {
            if (kv.first == "level" || kv.first == "timestamp") continue;
            std::string ph = "{" + kv.first + "}";
            std::string rep = Interpreter::stringify(kv.second);
            size_t p = 0;
            while ((p = out.find(ph, p)) != std::string::npos) {
                out.replace(p, ph.size(), rep);
                p += rep.size();
            }
        }
        // Positional {0}, {1}, ...
        for (size_t i = 1; i < args.size(); i++) {
            std::string ph = "{" + std::to_string(i - 1) + "}";
            std::string rep = Interpreter::stringify(args[i]);
            size_t p = 0;
            while ((p = out.find(ph, p)) != std::string::npos) {
                out.replace(p, ph.size(), rep);
                p += rep.size();
            }
        }
        int level = 0;
        if (const Value* lv = internal::namedArg(named, "level")) {
            if (lv->type != Value::Type::Int) ip.fail("log level needs INFO/WARN/ERROR/DEBUG", 1);
            level = (int)lv->integer;
        }
        if (const Value* tv = internal::namedArg(named, "timestamp")) {
            if (tv->truthy()) {
                char buf[32];
                std::snprintf(buf, sizeof(buf), "[t=%.2f] ", Incogine_Time_GetTime());
                out = buf + out;
            }
        }
        Incogine_Log(level, out.c_str());
        return Value::Null();
    }));

    globals->define("lerp", makeNative([](Interpreter& ip, const std::vector<Value>& args,
                                          const std::map<std::string, Value>&) {
        if (args.size() != 3) ip.fail("lerp(a, b, t) needs 3 arguments", 1);
        double a = requireNum(ip, args[0], "lerp(a)");
        double b = requireNum(ip, args[1], "lerp(b)");
        double t = requireNum(ip, args[2], "lerp(t)");
        return Value::Float(a + (b - a) * t);
    }));

    globals->define("cubicBezier",
                    makeNative([](Interpreter& ip, const std::vector<Value>& args,
                                   const std::map<std::string, Value>&) {
                        if (args.size() != 5) ip.fail("cubicBezier(t, x1, y1, x2, y2) needs 5 arguments", 1);
                        float t = (float)requireNum(ip, args[0], "t");
                        float x1 = (float)requireNum(ip, args[1], "x1");
                        float y1 = (float)requireNum(ip, args[2], "y1");
                        float x2 = (float)requireNum(ip, args[3], "x2");
                        float y2 = (float)requireNum(ip, args[4], "y2");
                        return Value::Float((double)::cubicBezier(t, x1, y1, x2, y2));
                    }));

    auto vecCtor = [](const std::string& what) {
        return makeNative([what](Interpreter& ip, const std::vector<Value>& args,
                                  const std::map<std::string, Value>&) {
            if (args.size() != 3) ip.fail(what + "(x, y, z) needs 3 arguments", 1);
            Vec3 v{requireNum(ip, args[0], "x"), requireNum(ip, args[1], "y"),
                   requireNum(ip, args[2], "z")};
            return Value::Vec(v);
        });
    };
    globals->define("Vector", vecCtor("Vector"));
    globals->define("Position", vecCtor("Position"));
    globals->define("Scale", vecCtor("Scale"));
    globals->define("Rotation", vecCtor("Rotation"));

    globals->define("Color", makeNative([](Interpreter& ip, const std::vector<Value>& args,
                                           const std::map<std::string, Value>&) {
        if (args.size() != 3 && args.size() != 4)
            ip.fail("Color(r, g, b, a) needs 3 or 4 arguments (0..1)", 1);
        ColorV c{clamp01(requireNum(ip, args[0], "r")), clamp01(requireNum(ip, args[1], "g")),
                 clamp01(requireNum(ip, args[2], "b")),
                 args.size() == 4 ? clamp01(requireNum(ip, args[3], "a")) : 1.0};
        return Value::Col(c);
    }));

    globals->define("Transform", makeNative([](Interpreter& ip, const std::vector<Value>& args,
                                               const std::map<std::string, Value>&) {
        TransformSpec t;
        if (!args.empty()) {
            if (args.size() != 3) ip.fail("Transform(Position, Scale, Rotation) needs 0 or 3 arguments", 1);
            t.position = requireVec3(ip, args[0], "Position");
            t.scale = requireVec3(ip, args[1], "Scale");
            t.rotation = requireVec3(ip, args[2], "Rotation");
        }
        return Value::TrSpec(t);
    }));

    globals->define("Sprite", makeNative([](Interpreter& ip, const std::vector<Value>& args,
                                            const std::map<std::string, Value>&) {
        if (args.size() != 1) ip.fail("Sprite(color|path) needs 1 argument", 1);
        SpriteSpec s;
        if (args[0].type == Value::Type::Color) {
            s.hasColor = true;
            s.color = args[0].color;
        } else if (args[0].type == Value::Type::String) {
            s.hasTexture = true;
            s.texture = args[0].str;
        } else if (args[0].type == Value::Type::SpriteSpec) {
            return args[0];
        } else {
            ip.fail("Sprite() needs a Color or an image path", 1);
        }
        return Value::SpSpec(s);
    }));

    globals->define("Audio", makeNative([](Interpreter& ip, const std::vector<Value>& args,
                                           const std::map<std::string, Value>&) {
        if (args.size() != 1) ip.fail("Audio(path) needs 1 argument", 1);
        std::string path = requireStr(ip, args[0], "Audio(path)");
        return Value::AudioRef(std::make_shared<Audio>(path.c_str()));
    }));

    globals->define("Random", Value::Mod("Random"));
    globals->define("Engine", Value::Mod("Engine"));
    globals->define("Scene", Value::Mod("Scene"));
    globals->define("Object", Value::Mod("Object"));
    globals->define("Save", Value::Mod("Save"));
    globals->define("Time", Value::Mod("Time"));
    globals->define("Input", Value::Mod("Input"));
}

Value Interpreter::getModuleMember(const std::string& module, const std::string& member, int line) {
    if (module == "Random") {
        if (member == "range") {
            return makeNative([](Interpreter& ip, const std::vector<Value>& args,
                                  const std::map<std::string, Value>&) {
                if (args.size() != 2) ip.fail("Random.range(min, max) needs 2 arguments", 1);
                double lo = requireNum(ip, args[0], "min");
                double hi = requireNum(ip, args[1], "max");
                if (hi <= lo) return Value::Int((int64_t)lo);
                std::uniform_int_distribution<int64_t> d((int64_t)lo, (int64_t)hi - 1);
                return Value::Int(d(internal::rng()));
            });
        }
        if (member == "rangeFloat") {
            return makeNative([](Interpreter& ip, const std::vector<Value>& args,
                                  const std::map<std::string, Value>&) {
                if (args.size() != 2) ip.fail("Random.rangeFloat(min, max) needs 2 arguments", 1);
                double lo = requireNum(ip, args[0], "min");
                double hi = requireNum(ip, args[1], "max");
                if (hi <= lo) return Value::Float(lo);
                std::uniform_real_distribution<double> d(lo, hi);
                return Value::Float(d(internal::rng()));
            });
        }
        if (member == "pick") {
            return makeNative([](Interpreter& ip, const std::vector<Value>& args,
                                  const std::map<std::string, Value>&) {
                if (args.size() != 1 || args[0].type != Value::Type::Array)
                    ip.fail("Random.pick(array) needs an array", 1);
                if (args[0].arr->empty()) ip.fail("Random.pick() of empty array", 1);
                std::uniform_int_distribution<size_t> d(0, args[0].arr->size() - 1);
                return (*args[0].arr)[d(internal::rng())];
            });
        }
        fail("Random has range/rangeFloat/pick", line);
    } else if (module == "Engine") {
        if (member == "setScene") {
            return makeNative([](Interpreter& ip, const std::vector<Value>& args,
                                  const std::map<std::string, Value>&) {
                if (args.size() != 1) ip.fail("Engine.setScene(name) needs 1 argument", 1);
                Incogine_Scene_SetByName(requireStr(ip, args[0], "scene name").c_str());
                return Value::Null();
            });
        }
        fail("Engine has setScene()", line);
    } else if (module == "Scene") {
        if (member == "change") {
            return makeNative([](Interpreter& ip, const std::vector<Value>& args,
                                  const std::map<std::string, Value>&) {
                if (args.size() != 1) ip.fail("Scene.change(name) needs 1 argument", 1);
                Incogine_Scene_SetByName(requireStr(ip, args[0], "scene name").c_str());
                return Value::Null();
            });
        }
        fail("Scene has change()", line);
    } else if (module == "Object") {
        if (member == "find") {
            return makeNative([this](Interpreter& ip, const std::vector<Value>& args,
                                     const std::map<std::string, Value>&) {
                if (args.size() != 1) ip.fail("Object.find(name) needs 1 argument", 1);
                return wrapObject(Object::FindByName(requireStr(ip, args[0], "name")));
            });
        }
        if (member == "findAll") {
            return makeNative([this](Interpreter& ip, const std::vector<Value>& args,
                                     const std::map<std::string, Value>&) {
                if (args.size() != 1) ip.fail("Object.findAll(name) needs 1 argument", 1);
                Value arr = Value::Array();
                for (Object* o : Object::FindAllByName(requireStr(ip, args[0], "name")))
                    arr.arr->push_back(wrapObject(o));
                return arr;
            });
        }
        if (member == "create") {
            return makeNative([this](Interpreter& ip, const std::vector<Value>& args,
                                     const std::map<std::string, Value>&) {
                std::string name = "Object";
                Vec3 pos{};
                if (args.size() > 0) {
                    if (args[0].type != Value::Type::String)
                        ip.fail("Object.create(name, position) needs a name string", 1);
                    name = args[0].str;
                }
                if (args.size() > 1) pos = requireVec3(ip, args[1], "position");
                if (args.size() > 2) ip.fail("Object.create(name, position) takes at most 2 arguments", 1);
                Object* o = new Object(name, Position(pos.x, pos.y, pos.z), Scale(1.0, 1.0, 1.0),
                                       Rotation(0.0, 0.0, 0.0));
                orphans.push_back(o);
                return wrapObject(o);
            });
        }
        fail("Object has find/findAll/create", line);
    } else if (module == "Save") {
        if (member == "set") {
            return makeNative([](Interpreter& ip, const std::vector<Value>& args,
                                  const std::map<std::string, Value>&) {
                if (args.size() != 2) ip.fail("Save.set(key, value) needs 2 arguments", 1);
                Incogine_Save_Set(requireStr(ip, args[0], "key").c_str(),
                                  Interpreter::stringify(args[1]).c_str());
                return Value::Null();
            });
        }
        if (member == "get") {
            return makeNative([](Interpreter& ip, const std::vector<Value>& args,
                                  const std::map<std::string, Value>&) {
                if (args.size() != 1) ip.fail("Save.get(key) needs 1 argument", 1);
                return Value::String(Incogine_Save_Get(requireStr(ip, args[0], "key").c_str()));
            });
        }
        if (member == "has") {
            return makeNative([](Interpreter& ip, const std::vector<Value>& args,
                                  const std::map<std::string, Value>&) {
                if (args.size() != 1) ip.fail("Save.has(key) needs 1 argument", 1);
                return Value::Bool(
                    Incogine_Save_Has(requireStr(ip, args[0], "key").c_str()) != 0);
            });
        }
        if (member == "remove") {
            return makeNative([](Interpreter& ip, const std::vector<Value>& args,
                                  const std::map<std::string, Value>&) {
                if (args.size() != 1) ip.fail("Save.remove(key) needs 1 argument", 1);
                return Value::Bool(
                    Incogine_Save_Remove(requireStr(ip, args[0], "key").c_str()) != 0);
            });
        }
        if (member == "clear") {
            return makeNative([](Interpreter&, const std::vector<Value>&,
                                  const std::map<std::string, Value>&) {
                Incogine_Save_Clear();
                return Value::Null();
            });
        }
        if (member == "save") {
            return makeNative([](Interpreter&, const std::vector<Value>&,
                                  const std::map<std::string, Value>&) {
                return Value::Bool(Incogine_Save_Save() != 0);
            });
        }
        if (member == "load") {
            return makeNative([](Interpreter&, const std::vector<Value>&,
                                  const std::map<std::string, Value>&) {
                return Value::Bool(Incogine_Save_Load() != 0);
            });
        }
        fail("Save has set/get/has/remove/clear/save/load", line);
    } else if (module == "Time") {
        if (member == "deltaTime") return Value::Float(Incogine_Time_GetDeltaTime());
        if (member == "time") return Value::Float(Incogine_Time_GetTime());
        fail("Time has deltaTime/time", line);
    } else if (module == "Input") {
        if (member == "isKeyDown") {
            return makeNative([](Interpreter& ip, const std::vector<Value>& args,
                                  const std::map<std::string, Value>&) {
                if (args.size() != 1) ip.fail("Input.isKeyDown(key) needs 1 argument", 1);
                return Value::Bool(Incogine_Input_IsKeyDown(requireStr(ip, args[0], "key").c_str()) != 0);
            });
        }
        if (member == "isKeyPressed" || member == "isKeyReleased") {
            bool pressed = member == "isKeyPressed";
            return makeNative([this, pressed](Interpreter& ip, const std::vector<Value>& args,
                                              const std::map<std::string, Value>&) {
                if (args.size() != 1) ip.fail("Input key edge check needs 1 argument", 1);
                std::string key = "key:" + requireStr(ip, args[0], "key");
                bool down =
                    Incogine_Input_IsKeyDown(requireStr(ip, args[0], "key").c_str()) != 0;
                bool prev = lastKeyState.count(key) ? lastKeyState[key] : false;
                lastKeyState[key] = down;
                return Value::Bool(pressed ? (down && !prev) : (!down && prev));
            });
        }
        if (member == "getMousePosition") {
            return makeNative([](Interpreter&, const std::vector<Value>&,
                                  const std::map<std::string, Value>&) {
                float x = 0.0f, y = 0.0f;
                Incogine_Input_GetMousePosition(&x, &y);
                return Value::Vec({(double)x, (double)y, 0.0});
            });
        }
        if (member == "isMouseButtonDown") {
            return makeNative([](Interpreter& ip, const std::vector<Value>& args,
                                  const std::map<std::string, Value>&) {
                if (args.size() != 1) ip.fail("Input.isMouseButtonDown(button) needs 1 argument", 1);
                int btn = 0;
                if (args[0].type == Value::Type::Int) {
                    btn = (int)args[0].integer;
                } else if (args[0].type == Value::Type::String) {
                    std::string b = args[0].str;
                    for (auto& c : b) c = (char)std::tolower((unsigned char)c);
                    if (b == "left") btn = 1;
                    else if (b == "middle") btn = 2;
                    else if (b == "right") btn = 3;
                    else ip.fail("mouse button is Left/Middle/Right (or 1/2/3)", 1);
                } else {
                    ip.fail("mouse button is Left/Middle/Right (or 1/2/3)", 1);
                }
                return Value::Bool(Incogine_Input_IsMouseButtonDown(btn) != 0);
            });
        }
        if (member == "isGamepadButtonDown") {
            return makeNative([](Interpreter& ip, const std::vector<Value>& args,
                                  const std::map<std::string, Value>&) {
                if (args.size() != 2) ip.fail("Input.isGamepadButtonDown(index, button) needs 2", 1);
                double di = 0.0;
                if (!args[0].asNumber(di)) ip.fail("gamepad index needs a number", 1);
                std::string btn = requireStr(ip, args[1], "button");
                return Value::Bool(
                    Incogine_Input_IsGamepadButtonDown((int)di, btn.c_str()) != 0);
            });
        }
        if (member == "getGamepadAxis") {
            return makeNative([](Interpreter& ip, const std::vector<Value>& args,
                                  const std::map<std::string, Value>&) {
                if (args.size() != 2) ip.fail("Input.getGamepadAxis(index, axis) needs 2", 1);
                double di = 0.0;
                if (!args[0].asNumber(di)) ip.fail("gamepad index needs a number", 1);
                std::string ax = requireStr(ip, args[1], "axis");
                return Value::Float(
                    (double)Incogine_Input_GetGamepadAxis((int)di, ax.c_str()));
            });
        }
        fail("Input has isKeyDown/isKeyPressed/isKeyReleased/getMousePosition/"
             "isMouseButtonDown/isGamepadButtonDown/getGamepadAxis",
             line);
    } else {
        fail("unknown module '" + module + "'", line);
    }
    return Value::Null();
}

} // namespace Kodo
