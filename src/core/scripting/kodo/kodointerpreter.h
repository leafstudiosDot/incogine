// Kodo tree-walk interpreter: executes a parsed Program with engine bindings.
// One Interpreter lives per script (KodoScriptHandler); globals persist
// across frames. Safety rails: call-depth limit + per-dispatch step budget
// so a runaway script fails loudly instead of hanging the game loop.

#ifndef KODOINTERPRETER_H
#define KODOINTERPRETER_H

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "kodoast.h"

class Object;

namespace Kodo {

struct EvalError {
    std::string message;
    int line = 1;
};

struct ReturnJump { Value value; };
struct BreakJump {};
struct ContinueJump {};

struct Environment : std::enable_shared_from_this<Environment> {
    explicit Environment(std::shared_ptr<Environment> parent = nullptr)
        : parent(std::move(parent)) {}
    std::map<std::string, Value> vars;
    std::shared_ptr<Environment> parent;

    bool define(const std::string& name, const Value& v) {
        vars[name] = v;
        return true;
    }
    // Assign to the nearest scope holding the name; false when undefined.
    bool assign(const std::string& name, const Value& v) {
        auto it = vars.find(name);
        if (it != vars.end()) {
            it->second = v;
            return true;
        }
        if (parent) return parent->assign(name, v);
        return false;
    }
    bool lookup(const std::string& name, Value& out) const {
        auto it = vars.find(name);
        if (it != vars.end()) {
            out = it->second;
            return true;
        }
        if (parent) return parent->lookup(name, out);
        return false;
    }
};

class Interpreter {
    public:
        explicit Interpreter(const std::string& originDir = "", const std::string& originFile = "");
        ~Interpreter();

        // Run top-level statements (globals + function declarations).
        // Returns false on error (see error()).
        bool run(Program& prog);

        // Call a global lifecycle function if defined: start/update/onDestroy.
        // `thisObj` becomes `this.owner` (null Object* allowed).
        bool callLifecycle(const std::string& name, Object* thisObj);

        bool hasError() const { return !lastError.empty(); }
        const std::string& error() const { return lastError; }

        // Called by native bindings to raise catchable script errors.
        [[noreturn]] void fail(const std::string& msg, int line);

        // Budget accounting (called per evaluated node).
        void tick(int line);

        // Script-visible formatting/comparison (also used by bindings).
        static std::string stringify(const Value& v);
        static bool equals(const Value& a, const Value& b);

    private:
        std::shared_ptr<Environment> globals;
        std::string originDir;
        std::string originFile; // script path (error messages)
        std::string currentFile; // for error messages (top-level file or import)
        std::string lastError;
        int depth = 0;
        int steps = 0;
        int stepBudget = 500000;
        static const int maxDepth = 256;

        // Import cache: resolved path -> ran already + module scope
        std::set<std::string> importedFiles;
        std::map<std::string, std::shared_ptr<Environment>> importedEnvs;

        // Objects created via Object.create: freed if still alive at shutdown
        std::vector<Object*> orphans;

        // Input edge detection: last observed down-state per key/button
        std::map<std::string, bool> lastKeyState;

        // Sprite texture paths (engine Sprite has no texture field yet)
        std::map<Object*, std::string> spriteTextures;

        void installBuiltins();
        Value getModuleMember(const std::string& module, const std::string& member, int line);
        std::string resolveImport(const std::string& path);

        Value evalExpr(Expr* e, std::shared_ptr<Environment> env);
        void execStmt(Stmt* s, std::shared_ptr<Environment> env);
        void execBlock(const std::vector<std::unique_ptr<Stmt>>& stmts,
                       std::shared_ptr<Environment> env);
        void assignTarget(Expr* target, const Value& val,
                          std::shared_ptr<Environment> env);
        Value callValue(const Value& callee, const std::vector<Value>& args,
                        const std::map<std::string, Value>& named, int line,
                        std::shared_ptr<Environment> env);
        Value callFunction(std::shared_ptr<Function> fn,
                           const std::vector<Value>& args,
                           const std::map<std::string, Value>& named,
                           std::shared_ptr<Environment> closure, int line,
                           const Value* thisBinding = nullptr);

        // Member access
        Value getMember(const Value& obj, const std::string& member, int line);
        void setMember(Value& obj, const std::string& member, const Value& val, int line);
        Value getIndex(const Value& obj, const Value& idx, int line);
        void setIndex(Value& obj, const Value& idx, const Value& val, int line);

        // Engine-bound helpers
        Value wrapObject(Object* o);
        void applySpriteSpec(Object* o, const SpriteSpec& spec);
        static double numOf(const Value& v, int line);
};

} // namespace Kodo

#endif
