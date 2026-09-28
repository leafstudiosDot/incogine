#include "kodointerpreter.h"
#include "kodoparser.h"
#include "kodoengine.h"

#include <cstdio>
#include <fstream>
#include <iterator>

namespace Kodo {

Interpreter::Interpreter(const std::string& originDir, const std::string& originFile)
    : originDir(originDir), originFile(originFile) {
    globals = std::make_shared<Environment>();
    installBuiltins();
}

Interpreter::~Interpreter() {
    for (Object* o : orphans) delete o;
    orphans.clear();
}

void Interpreter::fail(const std::string& msg, int line) {
    std::string where = currentFile.empty() ? ("line " + std::to_string(line))
                                            : (currentFile + ": line " + std::to_string(line));
    throw EvalError{where + ": " + msg, line};
}

void Interpreter::tick(int line) {
    if (++steps > stepBudget) fail("execution budget exceeded (possible infinite loop)", line);
}

bool Interpreter::run(Program& prog) {
    lastError.clear();
    steps = 0;
    depth = 0;
    currentFile = originFile.empty() ? originDir : originFile;
    try {
        execBlock(prog.stmts, globals);
    } catch (const EvalError& e) {
        lastError = e.message;
        return false;
    } catch (const ReturnJump&) {
        lastError = "return outside function";
        return false;
    } catch (const BreakJump&) {
        lastError = "break outside loop";
        return false;
    } catch (const ContinueJump&) {
        lastError = "continue outside loop";
        return false;
    }
    return true;
}

bool Interpreter::callLifecycle(const std::string& name, Object* thisObj) {
    lastError.clear();
    Value fn;
    if (!globals->lookup(name, fn)) return true; // lifecycle methods are optional
    if (fn.type != Value::Type::Function) return true;
    steps = 0;
    depth = 0;
    Value thisMap = Value::Map();
    thisMap.map->emplace("owner", wrapObject(thisObj));
    thisMap.map->emplace("object", wrapObject(thisObj)); // alias
    try {
        callFunction(fn.func, {}, {}, fn.func->closure, 1, &thisMap);
    } catch (const EvalError& e) {
        lastError = e.message;
        return false;
    } catch (const ReturnJump&) {
        // bare `return` at lifecycle top level: value ignored
    } catch (const BreakJump&) {
        lastError = "break outside loop in '" + name + "'";
        return false;
    } catch (const ContinueJump&) {
        lastError = "continue outside loop in '" + name + "'";
        return false;
    }
    return true;
}

std::string Interpreter::stringify(const Value& v) {
    char buf[64];
    switch (v.type) {
        case Value::Type::Null: return "null";
        case Value::Type::Bool: return v.boolean ? "true" : "false";
        case Value::Type::Int: return std::to_string(v.integer);
        case Value::Type::Float:
            std::snprintf(buf, sizeof(buf), "%g", v.number);
            return buf;
        case Value::Type::String: return v.str;
        case Value::Type::Array: {
            std::string s = "[";
            for (size_t i = 0; i < v.arr->size(); i++) {
                if (i) s += ", ";
                s += stringify((*v.arr)[i]);
            }
            return s + "]";
        }
        case Value::Type::Map: {
            std::string s = "{";
            bool first = true;
            for (const auto& kv : *v.map) {
                if (!first) s += ", ";
                first = false;
                s += kv.first + ": " + stringify(kv.second);
            }
            return s + "}";
        }
        case Value::Type::Function:
        case Value::Type::NativeFn: return "<function>";
        case Value::Type::Vec3:
            std::snprintf(buf, sizeof(buf), "Vector(%g, %g, %g)", v.vec.x, v.vec.y, v.vec.z);
            return buf;
        case Value::Type::Color:
            std::snprintf(buf, sizeof(buf), "Color(%g, %g, %g, %g)",
                          v.color.r, v.color.g, v.color.b, v.color.a);
            return buf;
        case Value::Type::ObjectRef:
            return v.object ? ("Object(" + v.object->getName() + ")") : "null";
        case Value::Type::TransformRef: return "Transform";
        case Value::Type::SpriteRef: return "Sprite";
        case Value::Type::AudioRef: return "Audio";
        case Value::Type::SpriteSpec: return "Sprite";
        case Value::Type::TransformSpec: return "Transform";
        case Value::Type::Module: return v.module;
    }
    return "unknown";
}

bool Interpreter::equals(const Value& a, const Value& b) {
    if (a.type == Value::Type::Int && b.type == Value::Type::Int)
        return a.integer == b.integer;
    if ((a.type == Value::Type::Int || a.type == Value::Type::Float) &&
        (b.type == Value::Type::Int || b.type == Value::Type::Float)) {
        double x, y;
        a.asNumber(x);
        b.asNumber(y);
        return x == y;
    }
    if (a.type != b.type) return false;
    switch (a.type) {
        case Value::Type::Null: return true;
        case Value::Type::Bool: return a.boolean == b.boolean;
        case Value::Type::String: return a.str == b.str;
        case Value::Type::Vec3:
            return a.vec.x == b.vec.x && a.vec.y == b.vec.y && a.vec.z == b.vec.z;
        case Value::Type::Color:
            return a.color.r == b.color.r && a.color.g == b.color.g &&
                   a.color.b == b.color.b && a.color.a == b.color.a;
        case Value::Type::ObjectRef:
        case Value::Type::TransformRef:
        case Value::Type::SpriteRef: return a.object == b.object;
        case Value::Type::AudioRef: return a.audio == b.audio;
        case Value::Type::Function: return a.func == b.func;
        case Value::Type::NativeFn: return a.native == b.native;
        case Value::Type::Array: return a.arr == b.arr;
        case Value::Type::Map: return a.map == b.map;
        case Value::Type::Module: return a.module == b.module;
        default: return false;
    }
}

double Interpreter::numOf(const Value& v, int line) {
    double out = 0.0;
    if (v.asNumber(out)) return out;
    throw EvalError{"expected number, got " + std::string(v.typeName()), line};
}

Value Interpreter::wrapObject(Object* o) {
    if (!o) return Value::Null();
    return Value::ObjRef(o);
}

void Interpreter::applySpriteSpec(Object* o, const SpriteSpec& spec) {
    if (!o) return;
    if (spec.hasColor) internal::applyEngineColor(o, spec.color);
    if (spec.hasTexture) spriteTextures[o] = spec.texture;
}

// --- Statements ---

void Interpreter::execBlock(const std::vector<std::unique_ptr<Stmt>>& stmts,
                            std::shared_ptr<Environment> env) {
    for (const auto& s : stmts) execStmt(s.get(), env);
}

void Interpreter::execStmt(Stmt* s, std::shared_ptr<Environment> env) {
    tick(s->line);
    if (auto* v = dynamic_cast<VarStmt*>(s)) {
        Value init = evalExpr(v->init.get(), env);
        env->define(v->name, init);
    } else if (auto* e = dynamic_cast<ExprStmt*>(s)) {
        evalExpr(e->expr.get(), env);
    } else if (auto* b = dynamic_cast<BlockStmt*>(s)) {
        execBlock(b->stmts, env); // flat function scope (no new env)
    } else if (auto* d = dynamic_cast<FuncDeclStmt*>(s)) {
        d->func->closure = env;
        env->define(d->func->name, Value::Fn(d->func));
    } else if (auto* i = dynamic_cast<IfStmt*>(s)) {
        if (evalExpr(i->cond.get(), env).truthy())
            execStmt(i->thenBranch.get(), env);
        else if (i->elseBranch)
            execStmt(i->elseBranch.get(), env);
    } else if (auto* w = dynamic_cast<WhileStmt*>(s)) {
        while (evalExpr(w->cond.get(), env).truthy()) {
            try {
                execStmt(w->body.get(), env);
            } catch (const BreakJump&) {
                break;
            } catch (const ContinueJump&) {
                continue;
            }
        }
    } else if (auto* f = dynamic_cast<ForStmt*>(s)) {
        if (f->init) execStmt(f->init.get(), env);
        while (!f->cond || evalExpr(f->cond.get(), env).truthy()) {
            try {
                execStmt(f->body.get(), env);
            } catch (const BreakJump&) {
                break;
            } catch (const ContinueJump&) {
                // fall through to incr
            }
            if (f->incr) evalExpr(f->incr.get(), env);
        }
    } else if (auto* fe = dynamic_cast<ForeachStmt*>(s)) {
        Value it = evalExpr(fe->iterable.get(), env);
        if (it.type == Value::Type::Array) {
            for (const Value& item : *it.arr) {
                env->define(fe->itemName, item);
                try {
                    execStmt(fe->body.get(), env);
                } catch (const BreakJump&) {
                    break;
                } catch (const ContinueJump&) {
                    continue;
                }
            }
        } else if (it.type == Value::Type::Map) {
            for (const auto& kv : *it.map) {
                env->define(fe->itemName, Value::String(kv.first));
                try {
                    execStmt(fe->body.get(), env);
                } catch (const BreakJump&) {
                    break;
                } catch (const ContinueJump&) {
                    continue;
                }
            }
        } else if (it.type == Value::Type::String) {
            for (char c : it.str) {
                env->define(fe->itemName, Value::String(std::string(1, c)));
                try {
                    execStmt(fe->body.get(), env);
                } catch (const BreakJump&) {
                    break;
                } catch (const ContinueJump&) {
                    continue;
                }
            }
        } else {
            fail("foreach needs array, object or string", s->line);
        }
    } else if (auto* sw = dynamic_cast<SwitchStmt*>(s)) {
        Value val = evalExpr(sw->value.get(), env);
        size_t start = sw->cases.size();
        for (size_t i = 0; i < sw->cases.size(); i++) {
            if (sw->cases[i].test && equals(val, evalExpr(sw->cases[i].test.get(), env))) {
                start = i;
                break;
            }
        }
        if (start == sw->cases.size()) {
            for (size_t i = 0; i < sw->cases.size(); i++) {
                if (!sw->cases[i].test) {
                    start = i;
                    break;
                }
            }
        }
        if (start < sw->cases.size()) {
            try {
                for (size_t i = start; i < sw->cases.size(); i++)
                    execBlock(sw->cases[i].stmts, env);
            } catch (const BreakJump&) {
                // break exits the switch
            }
        }
    } else if (dynamic_cast<BreakStmt*>(s)) {
        throw BreakJump{};
    } else if (dynamic_cast<ContinueStmt*>(s)) {
        throw ContinueJump{};
    } else if (auto* r = dynamic_cast<ReturnStmt*>(s)) {
        Value v = r->value ? evalExpr(r->value.get(), env) : Value::Null();
        throw ReturnJump{v};
    } else if (auto* t = dynamic_cast<TryStmt*>(s)) {
        try {
            execBlock(t->body, env);
        } catch (const EvalError& e) {
            auto child = std::make_shared<Environment>(env);
            child->define(t->catchName, Value::String(e.message));
            execBlock(t->handler, child);
        }
    } else if (auto* im = dynamic_cast<ImportStmt*>(s)) {
        std::string resolved = resolveImport(im->path);
        std::shared_ptr<Environment> modEnv;
        auto cached = importedEnvs.find(resolved);
        if (cached == importedEnvs.end()) {
            std::ifstream in(resolved);
            if (!in) fail("cannot open import '" + im->path + "'", s->line);
            std::string src((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
            Parser p;
            if (!p.ParseSource(src, resolved)) fail("import parse error: " + p.error(), s->line);
            modEnv = std::make_shared<Environment>(globals);
            std::string savedFile = currentFile;
            currentFile = resolved;
            try {
                execBlock(p.program().stmts, modEnv);
            } catch (...) {
                currentFile = savedFile;
                throw;
            }
            currentFile = savedFile;
            importedFiles.insert(resolved);
            importedEnvs[resolved] = modEnv;
        } else {
            modEnv = cached->second;
        }
        if (!im->names.empty()) {
            for (const std::string& nm : im->names) {
                Value v;
                if (!modEnv->lookup(nm, v)) fail("'" + nm + "' not found in '" + im->path + "'", s->line);
                env->define(nm, v);
            }
        } else if (!im->alias.empty()) {
            Value m = Value::Map();
            for (const auto& kv : modEnv->vars) m.map->emplace(kv.first, kv.second);
            env->define(im->alias, m);
        } else if (im->importAll) {
            for (const auto& kv : modEnv->vars) env->define(kv.first, kv.second);
        }
    } else {
        fail("unknown statement", s->line);
    }
}

std::string Interpreter::resolveImport(const std::string& path) {
    if (!originDir.empty() && path.size() > 0 && path[0] != '/' && path[0] != '\\' &&
        !(path.size() > 1 && path[1] == ':')) {
        std::string cand = originDir + "/" + path;
        std::ifstream t(cand);
        if (t) return cand;
    }
    return path;
}

void Interpreter::assignTarget(Expr* target, const Value& val, std::shared_ptr<Environment> env) {
    if (auto* id = dynamic_cast<IdentExpr*>(target)) {
        if (!env->assign(id->name, val))
            fail("assign to undefined variable '" + id->name + "' (use 'var' first)", target->line);
    } else if (auto* m = dynamic_cast<MemberExpr*>(target)) {
        Value obj = evalExpr(m->object.get(), env);
        setMember(obj, m->member, val, target->line);
    } else if (auto* ix = dynamic_cast<IndexExpr*>(target)) {
        Value obj = evalExpr(ix->object.get(), env);
        Value idx = evalExpr(ix->index.get(), env);
        setIndex(obj, idx, val, target->line);
    } else {
        fail("invalid assignment target", target->line);
    }
}

} // namespace Kodo
