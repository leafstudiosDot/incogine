#include "kodoparser.h"

#include <fstream>
#include <sstream>

namespace Kodo {

Parser::Parser() {}
Parser::~Parser() {}

bool Parser::Load(const std::string& p) {
    path = p;
    std::ifstream in(p);
    if (!in) {
        failed = true;
        parseError = "cannot open file: " + p;
        return false;
    }
    std::ostringstream ss;
    ss << in.rdbuf();
    source = ss.str();
    return true;
}

bool Parser::ParseSource(const std::string& src, const std::string& origin) {
    path = origin;
    source = src;
    return Parse();
}

bool Parser::Parse() {
    prog.stmts.clear();
    pos = 0;
    failed = false;
    parseError.clear();

    Lexer lexer(source);
    bool ok = false;
    std::string err;
    tokens = lexer.tokenize(ok, err);
    if (!ok) {
        failed = true;
        parseError = path + ":" + err;
        return false;
    }

    skipSeparators();
    while (!atEnd() && !failed) {
        auto s = parseStatement();
        if (failed) return false;
        if (s) prog.stmts.push_back(std::move(s));
        skipSeparators();
    }
    return !failed;
}

const Token& Parser::peek() const { return tokens[pos < tokens.size() ? pos : tokens.size() - 1]; }
const Token& Parser::previous() const { return tokens[pos > 0 ? pos - 1 : 0]; }
bool Parser::atEnd() const { return peek().type == Token::Type::End; }
bool Parser::check(Token::Type t) const { return peek().type == t; }

bool Parser::match(Token::Type t) {
    if (check(t)) {
        pos++;
        return true;
    }
    return false;
}

void Parser::fail(const std::string& msg) {
    if (!failed) {
        failed = true;
        parseError = path + ": line " + std::to_string(peek().line) + ": " + msg;
    }
}

void Parser::skipSeparators() {
    while (check(Token::Type::Newline) || check(Token::Type::Semi)) pos++;
}

bool Parser::expect(Token::Type t, const std::string& what) {
    if (match(t)) return true;
    fail("expected " + what);
    return false;
}

std::unique_ptr<Stmt> Parser::parseStatement() {
    skipSeparators();
    if (atEnd()) return nullptr;
    const Token& t = peek();
    switch (t.type) {
        case Token::Type::Var: {
            pos++;
            return parseVar();
        }
        case Token::Type::Function: {
            int line = t.line;
            pos++;
            // function name(params) { } — name required for declarations
            if (!check(Token::Type::Ident)) {
                fail("expected function name");
                return nullptr;
            }
            std::string name = peek().text;
            pos++;
            auto fn = parseFunctionDef(name, line);
            if (failed) return nullptr;
            auto decl = std::make_unique<FuncDeclStmt>();
            decl->line = line;
            decl->func = fn;
            return decl;
        }
        case Token::Type::If:
            pos++;
            return parseIf();
        case Token::Type::While:
            pos++;
            return parseWhile();
        case Token::Type::For:
            pos++;
            return parseFor();
        case Token::Type::Foreach:
            pos++;
            return parseForeach();
        case Token::Type::Switch:
            pos++;
            return parseSwitch();
        case Token::Type::Break: {
            auto s = std::make_unique<BreakStmt>();
            s->line = t.line;
            pos++;
            return s;
        }
        case Token::Type::Continue: {
            auto s = std::make_unique<ContinueStmt>();
            s->line = t.line;
            pos++;
            return s;
        }
        case Token::Type::Return: {
            auto s = std::make_unique<ReturnStmt>();
            s->line = t.line;
            pos++;
            if (!check(Token::Type::Newline) && !check(Token::Type::Semi) &&
                !check(Token::Type::RBrace) && !atEnd()) {
                s->value = parseExpression();
            }
            return s;
        }
        case Token::Type::Try:
            pos++;
            return parseTry();
        case Token::Type::Import:
            pos++;
            return parseImport();
        case Token::Type::From:
            pos++;
            return parseFromImport();
        case Token::Type::LBrace:
            return parseBlock();
        case Token::Type::RBrace:
            fail("unexpected '}'");
            return nullptr;
        default: {
            auto e = parseExpression();
            if (failed) return nullptr;
            auto s = std::make_unique<ExprStmt>();
            s->line = t.line;
            s->expr = std::move(e);
            return s;
        }
    }
}

std::unique_ptr<Stmt> Parser::parseBlock() {
    int line = peek().line;
    if (!expect(Token::Type::LBrace, "'{'")) return nullptr;
    auto block = std::make_unique<BlockStmt>();
    block->line = line;
    skipSeparators();
    while (!check(Token::Type::RBrace) && !atEnd()) {
        auto s = parseStatement();
        if (failed) return nullptr;
        if (s) block->stmts.push_back(std::move(s));
        skipSeparators();
    }
    if (!expect(Token::Type::RBrace, "'}'")) return nullptr;
    return block;
}

std::unique_ptr<Stmt> Parser::parseVar() {
    int line = previous().line;
    if (!check(Token::Type::Ident)) {
        fail("expected variable name after 'var'");
        return nullptr;
    }
    std::string name = peek().text;
    pos++;
    auto stmt = std::make_unique<VarStmt>();
    stmt->line = line;
    stmt->name = name;
    if (match(Token::Type::Eq)) {
        stmt->init = parseExpression();
        if (failed) return nullptr;
    } else {
        auto lit = std::make_unique<LiteralExpr>();
        lit->line = line;
        lit->value = Value::Null();
        stmt->init = std::move(lit);
    }
    return stmt;
}

std::unique_ptr<Stmt> Parser::parseIf() {
    int line = previous().line;
    auto stmt = std::make_unique<IfStmt>();
    stmt->line = line;
    if (!expect(Token::Type::LParen, "'(' after 'if'")) return nullptr;
    stmt->cond = parseExpression();
    if (failed) return nullptr;
    if (!expect(Token::Type::RParen, "')'")) return nullptr;
    skipSeparators();
    stmt->thenBranch = parseStatement();
    if (failed) return nullptr;
    skipSeparators();
    if (match(Token::Type::Else)) {
        skipSeparators();
        stmt->elseBranch = parseStatement();
        if (failed) return nullptr;
    }
    return stmt;
}

std::unique_ptr<Stmt> Parser::parseWhile() {
    int line = previous().line;
    auto stmt = std::make_unique<WhileStmt>();
    stmt->line = line;
    if (!expect(Token::Type::LParen, "'(' after 'while'")) return nullptr;
    stmt->cond = parseExpression();
    if (failed) return nullptr;
    if (!expect(Token::Type::RParen, "')'")) return nullptr;
    skipSeparators();
    stmt->body = parseStatement();
    if (failed) return nullptr;
    return stmt;
}

std::unique_ptr<Stmt> Parser::parseFor() {
    int line = previous().line;
    auto stmt = std::make_unique<ForStmt>();
    stmt->line = line;
    if (!expect(Token::Type::LParen, "'(' after 'for'")) return nullptr;
    skipSeparators();
    if (!check(Token::Type::Semi)) {
        if (check(Token::Type::Var)) {
            pos++;
            auto v = parseVar();
            if (failed) return nullptr;
            stmt->init = std::move(v);
        } else {
            auto e = parseExpression();
            if (failed) return nullptr;
            auto es = std::make_unique<ExprStmt>();
            es->line = line;
            es->expr = std::move(e);
            stmt->init = std::move(es);
        }
    }
    if (!expect(Token::Type::Semi, "';' in for header")) return nullptr;
    skipSeparators();
    if (!check(Token::Type::Semi)) {
        stmt->cond = parseExpression();
        if (failed) return nullptr;
    }
    if (!expect(Token::Type::Semi, "';' in for header")) return nullptr;
    skipSeparators();
    if (!check(Token::Type::RParen)) {
        stmt->incr = parseExpression();
        if (failed) return nullptr;
    }
    if (!expect(Token::Type::RParen, "')'")) return nullptr;
    skipSeparators();
    stmt->body = parseStatement();
    if (failed) return nullptr;
    return stmt;
}

std::unique_ptr<Stmt> Parser::parseForeach() {
    int line = previous().line;
    auto stmt = std::make_unique<ForeachStmt>();
    stmt->line = line;
    if (!expect(Token::Type::LParen, "'(' after 'foreach'")) return nullptr;
    if (match(Token::Type::Var)) { /* optional */ }
    if (!check(Token::Type::Ident)) {
        fail("expected loop variable in foreach");
        return nullptr;
    }
    stmt->itemName = peek().text;
    pos++;
    // `in` is contextual: plain identifier here
    if (!check(Token::Type::Ident) || peek().text != "in") {
        fail("expected 'in' in foreach");
        return nullptr;
    }
    pos++;
    stmt->iterable = parseExpression();
    if (failed) return nullptr;
    if (!expect(Token::Type::RParen, "')'")) return nullptr;
    skipSeparators();
    stmt->body = parseStatement();
    if (failed) return nullptr;
    return stmt;
}

std::unique_ptr<Stmt> Parser::parseSwitch() {
    int line = previous().line;
    auto stmt = std::make_unique<SwitchStmt>();
    stmt->line = line;
    if (!expect(Token::Type::LParen, "'(' after 'switch'")) return nullptr;
    stmt->value = parseExpression();
    if (failed) return nullptr;
    if (!expect(Token::Type::RParen, "')'")) return nullptr;
    skipSeparators();
    if (!expect(Token::Type::LBrace, "'{' after switch'")) return nullptr;
    skipSeparators();
    bool hasDefault = false;
    while (!check(Token::Type::RBrace) && !atEnd()) {
        SwitchStmt::Case c;
        if (match(Token::Type::Case)) {
            c.test = parseExpression();
            if (failed) return nullptr;
        } else if (match(Token::Type::Default)) {
            if (hasDefault) {
                fail("multiple 'default' in switch");
                return nullptr;
            }
            hasDefault = true;
            c.test = nullptr;
        } else {
            fail("expected 'case' or 'default' in switch");
            return nullptr;
        }
        if (!expect(Token::Type::Colon, "':' after case")) return nullptr;
        skipSeparators();
        while (!check(Token::Type::Case) && !check(Token::Type::Default) &&
               !check(Token::Type::RBrace) && !atEnd()) {
            auto s = parseStatement();
            if (failed) return nullptr;
            if (s) c.stmts.push_back(std::move(s));
            skipSeparators();
        }
        stmt->cases.push_back(std::move(c));
    }
    if (!expect(Token::Type::RBrace, "'}'")) return nullptr;
    return stmt;
}

std::unique_ptr<Stmt> Parser::parseTry() {
    int line = previous().line;
    auto stmt = std::make_unique<TryStmt>();
    stmt->line = line;
    skipSeparators();
    if (!expect(Token::Type::LBrace, "'{' after 'try'")) return nullptr;
    skipSeparators();
    while (!check(Token::Type::RBrace) && !atEnd()) {
        auto s = parseStatement();
        if (failed) return nullptr;
        if (s) stmt->body.push_back(std::move(s));
        skipSeparators();
    }
    if (!expect(Token::Type::RBrace, "'}'")) return nullptr;
    skipSeparators();
    if (!expect(Token::Type::Catch, "'catch' after try block")) return nullptr;
    skipSeparators();
    if (!expect(Token::Type::LParen, "'(' after 'catch'")) return nullptr;
    if (!check(Token::Type::Ident)) {
        fail("expected error variable in catch");
        return nullptr;
    }
    stmt->catchName = peek().text;
    pos++;
    if (!expect(Token::Type::RParen, "')'")) return nullptr;
    skipSeparators();
    if (!expect(Token::Type::LBrace, "'{' after catch'")) return nullptr;
    skipSeparators();
    while (!check(Token::Type::RBrace) && !atEnd()) {
        auto s = parseStatement();
        if (failed) return nullptr;
        if (s) stmt->handler.push_back(std::move(s));
        skipSeparators();
    }
    if (!expect(Token::Type::RBrace, "'}'")) return nullptr;
    return stmt;
}

std::unique_ptr<Stmt> Parser::parseImport() {
    int line = previous().line;
    auto stmt = std::make_unique<ImportStmt>();
    stmt->line = line;
    if (!check(Token::Type::String)) {
        fail("expected module path string after 'import'");
        return nullptr;
    }
    stmt->path = peek().text;
    pos++;
    if (match(Token::Type::As)) {
        if (match(Token::Type::Star)) {
            stmt->importAll = true; // import "x" as * : all exports into scope
        } else {
            if (!check(Token::Type::Ident)) {
                fail("expected alias after 'as'");
                return nullptr;
            }
            stmt->alias = peek().text;
            pos++;
        }
    }
    return stmt;
}

std::unique_ptr<Stmt> Parser::parseFromImport() {
    int line = previous().line;
    auto stmt = std::make_unique<ImportStmt>();
    stmt->line = line;
    if (!check(Token::Type::String)) {
        fail("expected module path string after 'from'");
        return nullptr;
    }
    stmt->path = peek().text;
    pos++;
    // `import` is a keyword token here, not an identifier
    if (peek().type != Token::Type::Import) {
        fail("expected 'import' after module path");
        return nullptr;
    }
    pos++;
    if (!expect(Token::Type::LBrace, "'{' in from-import")) return nullptr;
    while (!check(Token::Type::RBrace) && !atEnd()) {
        if (!check(Token::Type::Ident)) {
            fail("expected name in import list");
            return nullptr;
        }
        stmt->names.push_back(peek().text);
        pos++;
        if (!match(Token::Type::Comma)) break;
    }
    if (!expect(Token::Type::RBrace, "'}' in from-import")) return nullptr;
    return stmt;
}

std::shared_ptr<Function> Parser::parseFunctionDef(const std::string& name, int line) {
    auto fn = std::make_shared<Function>();
    fn->name = name;
    fn->line = line;
    if (!expect(Token::Type::LParen, "'(' after function name")) return nullptr;
    if (!check(Token::Type::RParen)) {
        while (true) {
            if (!check(Token::Type::Ident)) {
                fail("expected parameter name");
                return nullptr;
            }
            fn->params.push_back(peek().text);
            pos++;
            if (!match(Token::Type::Comma)) break;
        }
    }
    if (!expect(Token::Type::RParen, "')' after parameters")) return nullptr;
    skipSeparators();
    if (!expect(Token::Type::LBrace, "'{' for function body")) return nullptr;
    skipSeparators();
    while (!check(Token::Type::RBrace) && !atEnd()) {
        auto s = parseStatement();
        if (failed) return nullptr;
        if (s) fn->body.push_back(std::move(s));
        skipSeparators();
    }
    if (!expect(Token::Type::RBrace, "'}'")) return nullptr;
    return fn;
}

std::unique_ptr<Expr> Parser::parseExpression() {
    return parseAssignment();
}

std::unique_ptr<Expr> Parser::parseAssignment() {
    auto e = parseOr();
    if (failed) return nullptr;
    Token::Type t = peek().type;
    if (t == Token::Type::Eq || t == Token::Type::PlusEq || t == Token::Type::MinusEq ||
        t == Token::Type::StarEq || t == Token::Type::SlashEq) {
        Token op = peek();
        pos++;
        skipSeparators();
        auto rhs = parseAssignment(); // right-associative
        if (failed) return nullptr;
        // Target must be Ident/Member/Index — validated by the interpreter
        auto a = std::make_unique<AssignExpr>();
        a->line = op.line;
        a->op = op;
        a->target = std::move(e);
        a->value = std::move(rhs);
        return a;
    }
    return e;
}

#define KODO_BINARY(next, ...)                                     \
    do {                                                           \
        Token::Type types[] = {__VA_ARGS__};                       \
        auto e = parse##next();                                    \
        if (failed) return nullptr;                                \
        while (!failed) {                                          \
            bool isOp = false;                                     \
            for (Token::Type t : types)                            \
                if (check(t)) { isOp = true; break; }              \
            if (!isOp) break;                                      \
            Token op = peek();                                     \
            pos++;                                                 \
            auto rhs = parse##next();                              \
            if (failed) return nullptr;                            \
            auto b = std::make_unique<BinaryExpr>();               \
            b->line = op.line;                                     \
            b->op = op;                                            \
            b->left = std::move(e);                                \
            b->right = std::move(rhs);                             \
            e = std::move(b);                                      \
        }                                                          \
        return e;                                                  \
    } while (0)

std::unique_ptr<Expr> Parser::parseOr() { KODO_BINARY(And, Token::Type::PipePipe); }
std::unique_ptr<Expr> Parser::parseAnd() { KODO_BINARY(Equality, Token::Type::AmpAmp); }

std::unique_ptr<Expr> Parser::parseEquality() {
    // == / !=. Note: a single '=' never reaches this level — assignment is
    // handled one level up in parseAssignment.
    auto e = parseComparison();
    if (failed) return nullptr;
    while (check(Token::Type::EqEq) || check(Token::Type::BangEq)) {
        Token op = peek();
        pos++;
        auto rhs = parseComparison();
        if (failed) return nullptr;
        auto b = std::make_unique<BinaryExpr>();
        b->line = op.line;
        b->op = op;
        b->left = std::move(e);
        b->right = std::move(rhs);
        e = std::move(b);
    }
    return e;
}

std::unique_ptr<Expr> Parser::parseComparison() {
    KODO_BINARY(Additive, Token::Type::Lt, Token::Type::LtEq, Token::Type::Gt, Token::Type::GtEq);
}

std::unique_ptr<Expr> Parser::parseAdditive() {
    KODO_BINARY(Multiplicative, Token::Type::Plus, Token::Type::Minus);
}

std::unique_ptr<Expr> Parser::parseMultiplicative() {
    KODO_BINARY(Unary, Token::Type::Star, Token::Type::Slash, Token::Type::Percent);
}

std::unique_ptr<Expr> Parser::parseUnary() {
    if (check(Token::Type::Bang) || check(Token::Type::Minus) ||
        check(Token::Type::PlusPlus) || check(Token::Type::MinusMinus)) {
        Token op = peek();
        pos++;
        auto operand = parseUnary();
        if (failed) return nullptr;
        if (op.type == Token::Type::PlusPlus || op.type == Token::Type::MinusMinus) {
            auto p = std::make_unique<PrefixExpr>();
            p->line = op.line;
            p->op = op;
            p->target = std::move(operand);
            return p;
        }
        auto u = std::make_unique<UnaryExpr>();
        u->line = op.line;
        u->op = op;
        u->operand = std::move(operand);
        return u;
    }
    return parsePostfix();
}

std::unique_ptr<Expr> Parser::parsePostfix() {
    auto e = parsePrimary();
    if (failed) return nullptr;
    while (true) {
        if (check(Token::Type::PlusPlus) || check(Token::Type::MinusMinus)) {
            Token op = peek();
            pos++;
            auto p = std::make_unique<PostfixExpr>();
            p->line = op.line;
            p->op = op;
            p->target = std::move(e);
            e = std::move(p);
        } else if (check(Token::Type::Dot)) {
            pos++;
            if (!check(Token::Type::Ident)) {
                fail("expected member name after '.'");
                return nullptr;
            }
            auto m = std::make_unique<MemberExpr>();
            m->line = e->line;
            m->object = std::move(e);
            m->member = peek().text;
            pos++;
            e = std::move(m);
        } else if (check(Token::Type::LBracket)) {
            pos++;
            auto idx = parseExpression();
            if (failed) return nullptr;
            if (!expect(Token::Type::RBracket, "']'")) return nullptr;
            auto ix = std::make_unique<IndexExpr>();
            ix->line = idx->line;
            ix->object = std::move(e);
            ix->index = std::move(idx);
            e = std::move(ix);
        } else if (check(Token::Type::LParen)) {
            e = parseCall(std::move(e));
            if (failed) return nullptr;
        } else {
            break;
        }
    }
    return e;
}

std::unique_ptr<Expr> Parser::parseCall(std::unique_ptr<Expr> callee) {
    auto c = std::make_unique<CallExpr>();
    c->line = callee->line;
    c->callee = std::move(callee);
    pos++; // '('
    skipSeparators();
    if (!check(Token::Type::RParen)) {
        while (true) {
            // Named argument: name = expr (only when '=' is on the same line
            // and the name is a bare identifier — parsed as assignment then
            // unwrapped here).
            if (check(Token::Type::Ident)) {
                // Look ahead: Ident Eq (not EqEq)
                size_t saved = pos;
                std::string nm = peek().text;
                pos++;
                if (check(Token::Type::Eq)) {
                    pos++;
                    skipSeparators();
                    auto v = parseAssignment();
                    if (failed) return nullptr;
                    c->argNames.push_back(nm);
                    c->args.push_back(std::move(v));
                    if (match(Token::Type::Comma)) {
                        skipSeparators();
                        continue;
                    }
                    break;
                }
                pos = saved;
            }
            auto a = parseAssignment();
            if (failed) return nullptr;
            c->argNames.push_back("");
            c->args.push_back(std::move(a));
            if (!match(Token::Type::Comma)) break;
            skipSeparators();
        }
    }
    if (!expect(Token::Type::RParen, "')' after call arguments")) return nullptr;
    return c;
}

std::unique_ptr<Expr> Parser::parsePrimary() {
    const Token& t = peek();
    switch (t.type) {
        case Token::Type::Int: {
            auto e = std::make_unique<LiteralExpr>();
            e->line = t.line;
            e->value = Value::Int(t.integer);
            pos++;
            return e;
        }
        case Token::Type::Float: {
            auto e = std::make_unique<LiteralExpr>();
            e->line = t.line;
            e->value = Value::Float(t.number);
            pos++;
            return e;
        }
        case Token::Type::String: {
            auto e = std::make_unique<LiteralExpr>();
            e->line = t.line;
            e->value = Value::String(t.text);
            pos++;
            return e;
        }
        case Token::Type::True: {
            auto e = std::make_unique<LiteralExpr>();
            e->line = t.line;
            e->value = Value::Bool(true);
            pos++;
            return e;
        }
        case Token::Type::False: {
            auto e = std::make_unique<LiteralExpr>();
            e->line = t.line;
            e->value = Value::Bool(false);
            pos++;
            return e;
        }
        case Token::Type::Null: {
            auto e = std::make_unique<LiteralExpr>();
            e->line = t.line;
            e->value = Value::Null();
            pos++;
            return e;
        }
        case Token::Type::This: {
            auto e = std::make_unique<ThisExpr>();
            e->line = t.line;
            pos++;
            // Postfix loop in parsePostfix handles .owner chains
            return e;
        }
        case Token::Type::Ident: {
            auto e = std::make_unique<IdentExpr>();
            e->line = t.line;
            e->name = t.text;
            pos++;
            return e;
        }
        case Token::Type::Function: {
            int line = t.line;
            pos++;
            std::string name;
            if (check(Token::Type::Ident)) {
                // Named function expression: function(a) vs function name(a)?
                // Ambiguity: `function foo(` could be a named expression.
                // Peek: Ident followed by '(' -> anonymous with first param? No —
                // treat `function name(` as named only when followed by '(' after name
                // AND ... simplest: if next-next is '(', it's a name.
                size_t saved = pos;
                pos++;
                if (check(Token::Type::LParen)) {
                    name = tokens[saved].text;
                } else {
                    pos = saved; // param list starts with this identifier
                }
            }
            auto fn = parseFunctionDef(name, line);
            if (failed) return nullptr;
            auto e = std::make_unique<FunctionExpr>();
            e->line = line;
            e->params = fn->params;
            e->body = std::move(fn->body);
            return e;
        }
        case Token::Type::LParen: {
            pos++;
            auto e = parseExpression();
            if (failed) return nullptr;
            if (!expect(Token::Type::RParen, "')'")) return nullptr;
            return e;
        }
        case Token::Type::LBracket: {
            int line = t.line;
            pos++;
            auto e = std::make_unique<ArrayExpr>();
            e->line = line;
            skipSeparators();
            if (!check(Token::Type::RBracket)) {
                while (true) {
                    auto item = parseExpression();
                    if (failed) return nullptr;
                    e->items.push_back(std::move(item));
                    if (!match(Token::Type::Comma)) break;
                    skipSeparators();
                }
            }
            if (!expect(Token::Type::RBracket, "']'")) return nullptr;
            return e;
        }
        case Token::Type::LBrace: {
            int line = t.line;
            pos++;
            auto e = std::make_unique<MapExpr>();
            e->line = line;
            skipSeparators();
            if (!check(Token::Type::RBrace)) {
                while (true) {
                    std::string key;
                    if (check(Token::Type::Ident) || check(Token::Type::String)) {
                        key = peek().text;
                        pos++;
                    } else {
                        fail("expected key in object literal");
                        return nullptr;
                    }
                    if (!expect(Token::Type::Colon, "':' in object literal")) return nullptr;
                    skipSeparators();
                    auto v = parseExpression();
                    if (failed) return nullptr;
                    e->pairs.emplace_back(key, std::move(v));
                    if (!match(Token::Type::Comma)) break;
                    skipSeparators();
                }
            }
            if (!expect(Token::Type::RBrace, "'}'")) return nullptr;
            return e;
        }
        default:
            fail("unexpected token in expression");
            return nullptr;
    }
}

} // namespace Kodo
