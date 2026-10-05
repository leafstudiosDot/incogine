// Kodo lexer: source text -> tokens. Newlines are significant (statement
// terminators) only outside (), []. Inside parens/brackets they are skipped
// so calls may span lines. Braces do NOT suppress newlines: block bodies
// need them as separators (the parser skips newlines inside object literals
// explicitly).

#ifndef KODOLEXER_H
#define KODOLEXER_H

#include <string>
#include <vector>

namespace Kodo {

struct Token {
    enum class Type {
        End, Newline, Semi,
        Ident, Int, Float, String,
        // Keywords
        Var, Function, Return, If, Else, While, For, Foreach, Switch, Case,
        Default, Break, Continue, Try, Catch, Import, From, As,
        True, False, Null, This,
        // Operators / punctuation
        Plus, Minus, Star, Slash, Percent,
        Eq, EqEq, Bang, BangEq, Lt, LtEq, Gt, GtEq,
        AmpAmp, PipePipe,
        PlusEq, MinusEq, StarEq, SlashEq, PlusPlus, MinusMinus,
        Dot, Comma, Colon, Question,
        LParen, RParen, LBracket, RBracket, LBrace, RBrace,
    };

    Type type = Type::End;
    std::string text; // identifier / string / number spelling
    int64_t integer = 0;
    double number = 0.0;
    int line = 1;
};

class Lexer {
    public:
        explicit Lexer(const std::string& source);
        std::vector<Token> tokenize(bool& ok, std::string& error);

    private:
        const std::string& src;
        size_t pos = 0;
        int line = 1;
        int depth = 0; // (), [], {} nesting
        std::vector<Token> out;
        bool failed = false;
        std::string error;

        char peek() const;
        char peekNext() const;
        char advance();
        bool atEnd() const;
        void fail(const std::string& msg);
        void skipLineComment();
        bool skipBlockComment();
        void lexNumber();
        void lexString(char quote);
        void lexIdent();
        Token::Type keyword(const std::string& word);
};

} // namespace Kodo

#endif
