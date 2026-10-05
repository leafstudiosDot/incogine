// Kodo parser: recursive descent over lexer tokens -> Program AST.
// Newlines terminate statements; ';' is also accepted as a terminator
// (and required as the separator inside for headers).

#ifndef KODOPARSER_H
#define KODOPARSER_H

#include <memory>
#include <string>
#include <vector>

#include "kodoast.h"
#include "kodolexer.h"

namespace Kodo {

class Parser {
    public:
        Parser();
        ~Parser();

        // Load source from disk (path as given, e.g. "scripts/kodo/x.kodo").
        bool Load(const std::string& path);
        // Tokenize + parse the loaded source. True on success.
        bool Parse();
        // Parse source text directly (used by tests/imports).
        bool ParseSource(const std::string& source, const std::string& origin);

        bool ok() const { return failed == false; }
        const std::string& error() const { return parseError; }
        const std::string& filePath() const { return path; }
        Program& program() { return prog; }

    private:
        std::string path;
        std::string source;
        std::vector<Token> tokens;
        size_t pos = 0;
        bool failed = false;
        std::string parseError;
        Program prog;

        const Token& peek() const;
        const Token& previous() const;
        bool atEnd() const;
        bool check(Token::Type t) const;
        bool match(Token::Type t);
        void fail(const std::string& msg);
        void skipSeparators();
        bool expect(Token::Type t, const std::string& what);

        std::unique_ptr<Stmt> parseStatement();
        std::unique_ptr<Stmt> parseBlock();
        std::unique_ptr<Stmt> parseVar();
        std::unique_ptr<Stmt> parseIf();
        std::unique_ptr<Stmt> parseWhile();
        std::unique_ptr<Stmt> parseFor();
        std::unique_ptr<Stmt> parseForeach();
        std::unique_ptr<Stmt> parseSwitch();
        std::unique_ptr<Stmt> parseTry();
        std::unique_ptr<Stmt> parseImport();
        std::unique_ptr<Stmt> parseFromImport();
        std::shared_ptr<Function> parseFunctionDef(const std::string& name, int line);

        std::unique_ptr<Expr> parseExpression();
        std::unique_ptr<Expr> parseAssignment();
        std::unique_ptr<Expr> parseOr();
        std::unique_ptr<Expr> parseAnd();
        std::unique_ptr<Expr> parseEquality();
        std::unique_ptr<Expr> parseComparison();
        std::unique_ptr<Expr> parseAdditive();
        std::unique_ptr<Expr> parseMultiplicative();
        std::unique_ptr<Expr> parseUnary();
        std::unique_ptr<Expr> parsePostfix();
        std::unique_ptr<Expr> parsePrimary();
        std::unique_ptr<Expr> parseCall(std::unique_ptr<Expr> callee);
};

} // namespace Kodo

#endif
