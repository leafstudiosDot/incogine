// Kodo AST nodes (recursive-descent parser output, interpreter input).

#ifndef KODOAST_H
#define KODOAST_H

#include "kodovalue.h"
#include "kodolexer.h"

namespace Kodo {

struct Expr {
    virtual ~Expr() = default;
    int line = 1;
};

struct Stmt {
    virtual ~Stmt() = default;
    int line = 1;
};

struct LiteralExpr : Expr {
    Value value;
};

struct IdentExpr : Expr {
    std::string name;
};

struct ThisExpr : Expr {};

struct ArrayExpr : Expr {
    std::vector<std::unique_ptr<Expr>> items;
};

struct MapExpr : Expr {
    std::vector<std::pair<std::string, std::unique_ptr<Expr>>> pairs;
};

struct FunctionExpr : Expr {
    std::vector<std::string> params;
    std::vector<std::unique_ptr<Stmt>> body;
};

// function name(params) { body } — also used for start/update/onDestroy.
struct Function {
    std::string name; // Empty for anonymous
    std::vector<std::string> params;
    std::vector<std::unique_ptr<Stmt>> body;
    int line = 1;
    std::shared_ptr<struct Environment> closure; // defining scope (set at declaration)
};

struct CallExpr : Expr {
    std::unique_ptr<Expr> callee;
    std::vector<std::unique_ptr<Expr>> args;
    std::vector<std::string> argNames; // "" = positional, else named (name=expr)
};

struct MemberExpr : Expr {
    std::unique_ptr<Expr> object;
    std::string member;
};

struct IndexExpr : Expr {
    std::unique_ptr<Expr> object;
    std::unique_ptr<Expr> index;
};

struct UnaryExpr : Expr {
    Token op;
    std::unique_ptr<Expr> operand;
};

struct BinaryExpr : Expr {
    Token op;
    std::unique_ptr<Expr> left;
    std::unique_ptr<Expr> right;
};

struct AssignExpr : Expr {
    Token op; // =, +=, -=, *=, /=
    std::unique_ptr<Expr> target; // IdentExpr, MemberExpr or IndexExpr
    std::unique_ptr<Expr> value;
};

struct PostfixExpr : Expr {
    Token op; // ++ or --
    std::unique_ptr<Expr> target;
};

struct PrefixExpr : Expr {
    Token op; // ++ or --
    std::unique_ptr<Expr> target;
};

struct ExprStmt : Stmt {
    std::unique_ptr<Expr> expr;
};

struct VarStmt : Stmt {
    std::string name;
    std::unique_ptr<Expr> init; // Always set (defaults to null literal)
};

struct BlockStmt : Stmt {
    std::vector<std::unique_ptr<Stmt>> stmts;
};

struct IfStmt : Stmt {
    std::unique_ptr<Expr> cond;
    std::unique_ptr<Stmt> thenBranch;
    std::unique_ptr<Stmt> elseBranch; // May be null
};

struct WhileStmt : Stmt {
    std::unique_ptr<Expr> cond;
    std::unique_ptr<Stmt> body;
};

struct ForStmt : Stmt {
    std::unique_ptr<Stmt> init; // VarStmt, ExprStmt or null
    std::unique_ptr<Expr> cond; // May be null (= true)
    std::unique_ptr<Expr> incr; // May be null
    std::unique_ptr<Stmt> body;
};

struct ForeachStmt : Stmt {
    std::string itemName;
    std::unique_ptr<Expr> iterable;
    std::unique_ptr<Stmt> body;
};

struct SwitchStmt : Stmt {
    std::unique_ptr<Expr> value;
    struct Case {
        std::unique_ptr<Expr> test; // Null for default
        std::vector<std::unique_ptr<Stmt>> stmts;
    };
    std::vector<Case> cases;
};

struct BreakStmt : Stmt {};
struct ContinueStmt : Stmt {};

struct ReturnStmt : Stmt {
    std::unique_ptr<Expr> value; // May be null
};

struct FuncDeclStmt : Stmt {
    std::shared_ptr<Function> func;
};

struct TryStmt : Stmt {
    std::vector<std::unique_ptr<Stmt>> body;
    std::string catchName;
    std::vector<std::unique_ptr<Stmt>> handler;
};

struct ImportStmt : Stmt {
    std::string path;
    std::vector<std::string> names; // from-import list (empty = plain import)
    std::string alias; // import-as name (empty = none)
    bool importAll = false; // import "x" as *
};

struct Program {
    std::vector<std::unique_ptr<Stmt>> stmts;
};

} // namespace Kodo

#endif
