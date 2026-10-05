// Kodo interpreter, part 2: expression evaluation + member/index access.

#include "kodointerpreter.h"
#include "kodoengine.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <iostream>

#include "../../assets/audio/audio.h"
#include "../csharp/csharpinterop.h"

namespace Kodo {

Value Interpreter::evalExpr(Expr* e, std::shared_ptr<Environment> env) {
    tick(e->line);
    if (auto* lit = dynamic_cast<LiteralExpr*>(e)) {
        return lit->value;
    } else if (auto* id = dynamic_cast<IdentExpr*>(e)) {
        Value v;
        if (!env->lookup(id->name, v)) fail("undefined variable '" + id->name + "'", e->line);
        return v;
    } else if (dynamic_cast<ThisExpr*>(e)) {
        Value v;
        if (!env->lookup("this", v))
            fail("'this' is only available inside start/update/onDestroy", e->line);
        return v;
    } else if (auto* a = dynamic_cast<ArrayExpr*>(e)) {
        Value arr = Value::Array();
        for (auto& item : a->items) arr.arr->push_back(evalExpr(item.get(), env));
        return arr;
    } else if (auto* m = dynamic_cast<MapExpr*>(e)) {
        Value map = Value::Map();
        for (auto& kv : m->pairs) map.map->emplace(kv.first, evalExpr(kv.second.get(), env));
        return map;
    } else if (auto* f = dynamic_cast<FunctionExpr*>(e)) {
        auto fn = std::make_shared<Function>();
        fn->name = "";
        fn->params = f->params;
        fn->body = std::move(f->body);
        fn->line = e->line;
        fn->closure = env;
        return Value::Fn(fn);
    } else if (auto* c = dynamic_cast<CallExpr*>(e)) {
        Value callee = evalExpr(c->callee.get(), env);
        std::vector<Value> args;
        std::map<std::string, Value> named;
        for (size_t i = 0; i < c->args.size(); i++) {
            Value v = evalExpr(c->args[i].get(), env);
            if (c->argNames[i].empty())
                args.push_back(v);
            else
                named[c->argNames[i]] = v;
        }
        return callValue(callee, args, named, e->line, env);
    } else if (auto* m = dynamic_cast<MemberExpr*>(e)) {
        Value obj = evalExpr(m->object.get(), env);
        return getMember(obj, m->member, e->line);
    } else if (auto* ix = dynamic_cast<IndexExpr*>(e)) {
        Value obj = evalExpr(ix->object.get(), env);
        Value idx = evalExpr(ix->index.get(), env);
        return getIndex(obj, idx, e->line);
    } else if (auto* u = dynamic_cast<UnaryExpr*>(e)) {
        Value v = evalExpr(u->operand.get(), env);
        if (u->op.type == Token::Type::Bang) return Value::Bool(!v.truthy());
        if (u->op.type == Token::Type::Minus) {
            if (v.type == Value::Type::Int) return Value::Int(-v.integer);
            if (v.type == Value::Type::Float) return Value::Float(-v.number);
            if (v.type == Value::Type::Vec3) return Value::Vec({-v.vec.x, -v.vec.y, -v.vec.z});
            fail("unary '-' needs a number", e->line);
        }
        fail("unknown unary operator", e->line);
    } else if (auto* b = dynamic_cast<BinaryExpr*>(e)) {
        Token::Type op = b->op.type;
        if (op == Token::Type::AmpAmp) {
            Value l = evalExpr(b->left.get(), env);
            if (!l.truthy()) return Value::Bool(false);
            return Value::Bool(evalExpr(b->right.get(), env).truthy());
        }
        if (op == Token::Type::PipePipe) {
            Value l = evalExpr(b->left.get(), env);
            if (l.truthy()) return Value::Bool(true);
            return Value::Bool(evalExpr(b->right.get(), env).truthy());
        }
        Value l = evalExpr(b->left.get(), env);
        Value r = evalExpr(b->right.get(), env);
        if (op == Token::Type::EqEq) return Value::Bool(equals(l, r));
        if (op == Token::Type::BangEq) return Value::Bool(!equals(l, r));
        // String concat with +
        if (op == Token::Type::Plus &&
            (l.type == Value::Type::String || r.type == Value::Type::String))
            return Value::String(stringify(l) + stringify(r));
        // Vec3 arithmetic (+, -, scalar *)
        if (l.type == Value::Type::Vec3 && r.type == Value::Type::Vec3) {
            if (op == Token::Type::Plus)
                return Value::Vec({l.vec.x + r.vec.x, l.vec.y + r.vec.y, l.vec.z + r.vec.z});
            if (op == Token::Type::Minus)
                return Value::Vec({l.vec.x - r.vec.x, l.vec.y - r.vec.y, l.vec.z - r.vec.z});
            fail("only + and - work on Vectors", e->line);
        }
        if (l.type == Value::Type::Vec3 && op == Token::Type::Star) {
            double s = numOf(r, e->line);
            return Value::Vec({l.vec.x * s, l.vec.y * s, l.vec.z * s});
        }
        if (r.type == Value::Type::Vec3 && op == Token::Type::Star) {
            double s = numOf(l, e->line);
            return Value::Vec({r.vec.x * s, r.vec.y * s, r.vec.z * s});
        }
        double x = numOf(l, e->line);
        double y = numOf(r, e->line);
        bool bothInt = l.type == Value::Type::Int && r.type == Value::Type::Int;
        switch (op) {
            case Token::Type::Plus:
                return bothInt ? Value::Int(l.integer + r.integer) : Value::Float(x + y);
            case Token::Type::Minus:
                return bothInt ? Value::Int(l.integer - r.integer) : Value::Float(x - y);
            case Token::Type::Star:
                return bothInt ? Value::Int(l.integer * r.integer) : Value::Float(x * y);
            case Token::Type::Slash:
                if (y == 0.0) fail("division by zero", e->line);
                return bothInt ? Value::Int(l.integer / r.integer) : Value::Float(x / y);
            case Token::Type::Percent:
                if (y == 0.0) fail("modulo by zero", e->line);
                return bothInt ? Value::Int(l.integer % r.integer) : Value::Float(std::fmod(x, y));
            case Token::Type::Lt: return Value::Bool(x < y);
            case Token::Type::LtEq: return Value::Bool(x <= y);
            case Token::Type::Gt: return Value::Bool(x > y);
            case Token::Type::GtEq: return Value::Bool(x >= y);
            default: fail("unknown binary operator", e->line);
        }
    } else if (auto* as = dynamic_cast<AssignExpr*>(e)) {
        Value rhs = evalExpr(as->value.get(), env);
        if (as->op.type == Token::Type::Eq) {
            assignTarget(as->target.get(), rhs, env);
            return rhs;
        }
        // Compound: read target, apply op, write back
        Value cur;
        if (auto* id = dynamic_cast<IdentExpr*>(as->target.get())) {
            if (!env->lookup(id->name, cur)) fail("undefined variable '" + id->name + "'", e->line);
        } else if (auto* m = dynamic_cast<MemberExpr*>(as->target.get())) {
            cur = getMember(evalExpr(m->object.get(), env), m->member, e->line);
        } else if (auto* ix = dynamic_cast<IndexExpr*>(as->target.get())) {
            cur = getIndex(evalExpr(ix->object.get(), env), evalExpr(ix->index.get(), env), e->line);
        } else {
            fail("invalid assignment target", e->line);
        }
        Value res;
        Token::Type op = as->op.type;
        if (op == Token::Type::PlusEq &&
            (cur.type == Value::Type::String || rhs.type == Value::Type::String)) {
            res = Value::String(stringify(cur) + stringify(rhs));
        } else {
            double x = numOf(cur, e->line);
            double y = numOf(rhs, e->line);
            bool bothInt = cur.type == Value::Type::Int && rhs.type == Value::Type::Int;
            if (op == Token::Type::PlusEq)
                res = bothInt ? Value::Int(cur.integer + rhs.integer) : Value::Float(x + y);
            else if (op == Token::Type::MinusEq)
                res = bothInt ? Value::Int(cur.integer - rhs.integer) : Value::Float(x - y);
            else if (op == Token::Type::StarEq)
                res = bothInt ? Value::Int(cur.integer * rhs.integer) : Value::Float(x * y);
            else if (op == Token::Type::SlashEq) {
                if (y == 0.0) fail("division by zero", e->line);
                res = bothInt ? Value::Int(cur.integer / rhs.integer) : Value::Float(x / y);
            } else
                fail("unknown assignment operator", e->line);
        }
        assignTarget(as->target.get(), res, env);
        return res;
    } else if (auto* p = dynamic_cast<PostfixExpr*>(e)) {
        auto* id = dynamic_cast<IdentExpr*>(p->target.get());
        if (!id) fail("++/-- needs a variable", e->line);
        Value cur;
        if (!env->lookup(id->name, cur)) fail("undefined variable '" + id->name + "'", e->line);
        double d = numOf(cur, e->line);
        double step = (p->op.type == Token::Type::PlusPlus) ? 1.0 : -1.0;
        Value next = cur.type == Value::Type::Int ? Value::Int((int64_t)(d + step)) : Value::Float(d + step);
        if (!env->assign(id->name, next)) fail("undefined variable '" + id->name + "'", e->line);
        return cur;
    } else if (auto* p = dynamic_cast<PrefixExpr*>(e)) {
        auto* id = dynamic_cast<IdentExpr*>(p->target.get());
        if (!id) fail("++/-- needs a variable", e->line);
        Value cur;
        if (!env->lookup(id->name, cur)) fail("undefined variable '" + id->name + "'", e->line);
        double d = numOf(cur, e->line);
        double step = (p->op.type == Token::Type::PlusPlus) ? 1.0 : -1.0;
        Value next = cur.type == Value::Type::Int ? Value::Int((int64_t)(d + step)) : Value::Float(d + step);
        if (!env->assign(id->name, next)) fail("undefined variable '" + id->name + "'", e->line);
        return next;
    }
    fail("unknown expression", e->line);
    return Value::Null();
}

Value Interpreter::callValue(const Value& callee, const std::vector<Value>& args,
                             const std::map<std::string, Value>& named, int line,
                             std::shared_ptr<Environment> env) {
    if (callee.type == Value::Type::Function) {
        // Nested calls inherit the caller's `this` (so helpers called from
        // update() can use this.owner).
        Value thisVal;
        const Value* tb = nullptr;
        if (env && env->lookup("this", thisVal)) tb = &thisVal;
        return callFunction(callee.func, args, named, callee.func->closure, line, tb);
    }
    if (callee.type == Value::Type::NativeFn) {
        if (!callee.native) fail("call of empty native function", line);
        return callee.native->call(*this, args, named);
    }
    fail("call of non-function '" + std::string(callee.typeName()) + "'", line);
    return Value::Null();
}

Value Interpreter::callFunction(std::shared_ptr<Function> fn, const std::vector<Value>& args,
                                const std::map<std::string, Value>& named,
                                std::shared_ptr<Environment> closure, int line,
                                const Value* thisBinding) {
    if (!fn) fail("call of empty function", line);
    if (++depth > maxDepth) {
        depth--;
        fail("stack overflow (max call depth 256)", line);
    }
    auto callEnv = std::make_shared<Environment>(closure ? closure : globals);
    if (thisBinding) callEnv->define("this", *thisBinding);
    // Positional params in order; named params matched by name.
    std::map<std::string, Value> remaining = named;
    for (size_t i = 0; i < fn->params.size(); i++) {
        auto it = remaining.find(fn->params[i]);
        if (it != remaining.end()) {
            callEnv->define(fn->params[i], it->second);
            remaining.erase(it);
        } else if (i < args.size()) {
            callEnv->define(fn->params[i], args[i]);
        } else {
            callEnv->define(fn->params[i], Value::Null());
        }
    }
    if (!remaining.empty()) {
        depth--;
        fail("unknown named argument '" + remaining.begin()->first + "'", line);
    }
    Value ret = Value::Null();
    try {
        execBlock(fn->body, callEnv);
    } catch (const ReturnJump& r) {
        ret = r.value;
    }
    depth--;
    return ret;
}

// --- Member / index access ---

Value Interpreter::getMember(const Value& obj, const std::string& member, int line) {
    switch (obj.type) {
        case Value::Type::Map: {
            auto it = obj.map->find(member);
            if (it == obj.map->end()) return Value::Null();
            return it->second;
        }
        case Value::Type::Array:
            if (member == "length") return Value::Int((int64_t)obj.arr->size());
            fail("arrays only have 'length'", line);
            break;
        case Value::Type::String:
            if (member == "length") return Value::Int((int64_t)obj.str.size());
            fail("strings only have 'length'", line);
            break;
        case Value::Type::Vec3:
            if (member == "x") return Value::Float(obj.vec.x);
            if (member == "y") return Value::Float(obj.vec.y);
            if (member == "z") return Value::Float(obj.vec.z);
            fail("Vector has x/y/z", line);
            break;
        case Value::Type::Color:
            if (member == "r") return Value::Float(obj.color.r);
            if (member == "g") return Value::Float(obj.color.g);
            if (member == "b") return Value::Float(obj.color.b);
            if (member == "a") return Value::Float(obj.color.a);
            fail("Color has r/g/b/a", line);
            break;
        case Value::Type::ObjectRef: {
            Object* o = obj.object;
            if (!o) fail("use of destroyed Object", line);
            if (member == "transform") return Value::TrRef(o);
            if (member == "sprite") return Value::SpRef(o);
            if (member == "name") return Value::String(o->getName());
            if (member == "destroy") {
                Object* target = o;
                return makeNative([this, target](Interpreter&, const std::vector<Value>&,
                                                 const std::map<std::string, Value>&) {
                    auto it = std::find(orphans.begin(), orphans.end(), target);
                    if (it != orphans.end()) orphans.erase(it);
                    delete target;
                    return Value::Null();
                });
            }
            fail("Object has transform/sprite/name/destroy()", line);
            break;
        }
        case Value::Type::TransformRef: {
            Object* o = obj.object;
            if (!o) fail("use of destroyed Object", line);
            Position p = o->getPosition();
            Scale s = o->getScale();
            Rotation r = o->getRotation();
            if (member == "position") return Value::Vec({p.x, p.y, p.z});
            if (member == "scale") return Value::Vec({s.x, s.y, s.z});
            if (member == "rotation") return Value::Vec({r.x, r.y, r.z});
            fail("Transform has position/scale/rotation", line);
            break;
        }
        case Value::Type::SpriteRef: {
            Object* o = obj.object;
            if (!o) fail("use of destroyed Object", line);
            if (member == "color") return Value::Col(internal::readEngineColor(o));
            if (member == "texture") {
                auto it = spriteTextures.find(o);
                return Value::String(it != spriteTextures.end() ? it->second : "");
            }
            fail("Sprite has color/texture", line);
            break;
        }
        case Value::Type::AudioRef: {
            std::shared_ptr<Audio> a = obj.audio;
            if (!a) fail("use of destroyed Audio", line);
            if (member == "play") {
                return makeNative([a](Interpreter& ip, const std::vector<Value>& args,
                                       const std::map<std::string, Value>& named) {
                    int loop = 0;
                    if (const Value* lv = internal::namedArg(named, "loop")) {
                        if (lv->type == Value::Type::Bool)
                            loop = lv->boolean ? -1 : 0;
                        else if (lv->type == Value::Type::Int)
                            loop = (int)lv->integer;
                        else
                            ip.fail("play(loop) needs bool or int", 1);
                    } else if (!args.empty()) {
                        if (args[0].type == Value::Type::Bool)
                            loop = args[0].boolean ? -1 : 0;
                        else if (args[0].type == Value::Type::Int)
                            loop = (int)args[0].integer;
                        else
                            ip.fail("play(loop) needs bool or int", 1);
                    }
                    a->play(loop);
                    return Value::Null();
                });
            }
            if (member == "stop") {
                return makeNative([a](Interpreter&, const std::vector<Value>&,
                                       const std::map<std::string, Value>&) {
                    a->stop();
                    return Value::Null();
                });
            }
            if (member == "seek" || member == "forward" || member == "reverse" ||
                member == "setVolume" || member == "setLoop") {
                return makeNative([member](Interpreter&, const std::vector<Value>&,
                                            const std::map<std::string, Value>&) {
                    // Engine Audio has no seek/volume yet: accept and ignore.
                    std::cout << "[Kodo] Audio." << member
                              << "() not backed by engine audio yet (ignored)" << std::endl;
                    return Value::Null();
                });
            }
            fail("Audio has play/stop/seek/forward/reverse/setVolume/setLoop", line);
            break;
        }
        case Value::Type::Module:
            return getModuleMember(obj.module, member, line);
        default:
            fail("'" + std::string(obj.typeName()) + "' has no members", line);
            break;
    }
    return Value::Null();
}

void Interpreter::setMember(Value& obj, const std::string& member, const Value& val, int line) {
    switch (obj.type) {
        case Value::Type::Map:
            (*obj.map)[member] = val;
            return;
        case Value::Type::TransformRef: {
            Object* o = obj.object;
            if (!o) fail("use of destroyed Object", line);
            if (val.type != Value::Type::Vec3) fail("Transform." + member + " needs a Vector", line);
            if (member == "position")
                o->setPosition(Position(val.vec.x, val.vec.y, val.vec.z));
            else if (member == "scale")
                o->setScale(Scale(val.vec.x, val.vec.y, val.vec.z));
            else if (member == "rotation")
                o->setRotation(Rotation(val.vec.x, val.vec.y, val.vec.z));
            else
                fail("Transform has position/scale/rotation", line);
            return;
        }
        case Value::Type::SpriteRef: {
            Object* o = obj.object;
            if (!o) fail("use of destroyed Object", line);
            if (member == "color") {
                if (val.type == Value::Type::Color)
                    internal::applyEngineColor(o, val.color);
                else if (val.type == Value::Type::SpriteSpec && val.spriteSpec.hasColor)
                    internal::applyEngineColor(o, val.spriteSpec.color);
                else
                    fail("Sprite.color needs a Color", line);
            } else if (member == "texture") {
                if (val.type != Value::Type::String) fail("Sprite.texture needs a path string", line);
                spriteTextures[o] = val.str;
            } else
                fail("Sprite has color/texture", line);
            return;
        }
        case Value::Type::ObjectRef: {
            Object* o = obj.object;
            if (!o) fail("use of destroyed Object", line);
            if (member == "name") {
                if (val.type != Value::Type::String) fail("Object.name needs a string", line);
                o->setName(val.str);
            } else if (member == "sprite") {
                if (val.type != Value::Type::SpriteSpec) fail("Object.sprite needs a Sprite()", line);
                applySpriteSpec(o, val.spriteSpec);
            } else if (member == "transform") {
                if (val.type != Value::Type::TransformSpec)
                    fail("Object.transform needs a Transform()", line);
                const TransformSpec& t = val.transformSpec;
                o->setPosition(Position(t.position.x, t.position.y, t.position.z));
                o->setScale(Scale(t.scale.x, t.scale.y, t.scale.z));
                o->setRotation(Rotation(t.rotation.x, t.rotation.y, t.rotation.z));
            } else
                fail("Object.name/sprite/transform are settable", line);
            return;
        }
        default:
            fail("cannot set members on '" + std::string(obj.typeName()) + "'", line);
            return;
    }
}

Value Interpreter::getIndex(const Value& obj, const Value& idx, int line) {
    if (obj.type == Value::Type::Array) {
        if (idx.type != Value::Type::Int) fail("array index needs an int", line);
        int64_t i = idx.integer;
        if (i < 0 || (size_t)i >= obj.arr->size()) fail("array index out of range", line);
        return (*obj.arr)[(size_t)i];
    }
    if (obj.type == Value::Type::Map) {
        if (idx.type != Value::Type::String) fail("object index needs a string", line);
        auto it = obj.map->find(idx.str);
        if (it == obj.map->end()) return Value::Null();
        return it->second;
    }
    if (obj.type == Value::Type::String) {
        if (idx.type != Value::Type::Int) fail("string index needs an int", line);
        int64_t i = idx.integer;
        if (i < 0 || (size_t)i >= obj.str.size()) fail("string index out of range", line);
        return Value::String(std::string(1, obj.str[(size_t)i]));
    }
    fail("'" + std::string(obj.typeName()) + "' is not indexable", line);
    return Value::Null();
}

void Interpreter::setIndex(Value& obj, const Value& idx, const Value& val, int line) {
    if (obj.type == Value::Type::Array) {
        if (idx.type != Value::Type::Int) fail("array index needs an int", line);
        int64_t i = idx.integer;
        if (i < 0 || (size_t)i >= obj.arr->size()) fail("array index out of range", line);
        (*obj.arr)[(size_t)i] = val;
        return;
    }
    if (obj.type == Value::Type::Map) {
        if (idx.type != Value::Type::String) fail("object index needs a string", line);
        (*obj.map)[idx.str] = val;
        return;
    }
    fail("'" + std::string(obj.typeName()) + "' is not index-assignable", line);
}

} // namespace Kodo
