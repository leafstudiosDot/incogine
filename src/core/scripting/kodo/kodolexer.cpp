#include "kodolexer.h"

namespace Kodo {

Lexer::Lexer(const std::string& source) : src(source) {}

char Lexer::peek() const { return atEnd() ? '\0' : src[pos]; }
char Lexer::peekNext() const { return (pos + 1 >= src.size()) ? '\0' : src[pos + 1]; }
char Lexer::advance() { return atEnd() ? '\0' : src[pos++]; }
bool Lexer::atEnd() const { return pos >= src.size(); }

void Lexer::fail(const std::string& msg) {
    if (!failed) {
        failed = true;
        error = "line " + std::to_string(line) + ": " + msg;
    }
}

void Lexer::skipLineComment() {
    while (!atEnd() && peek() != '\n') advance();
}

bool Lexer::skipBlockComment() {
    advance(); // '*'
    while (!atEnd()) {
        char c = advance();
        if (c == '\n') line++;
        if (c == '*' && peek() == '/') {
            advance();
            return true;
        }
    }
    fail("unterminated block comment");
    return false;
}

void Lexer::lexNumber() {
    Token t;
    t.line = line;
    size_t start = pos;
    bool isFloat = false;
    while (peek() >= '0' && peek() <= '9') advance();
    if (peek() == '.' && peekNext() >= '0' && peekNext() <= '9') {
        isFloat = true;
        advance();
        while (peek() >= '0' && peek() <= '9') advance();
    }
    t.text = src.substr(start, pos - start);
    try {
        if (isFloat) {
            t.type = Token::Type::Float;
            t.number = std::stod(t.text);
        } else {
            t.type = Token::Type::Int;
            t.integer = std::stoll(t.text);
        }
    } catch (...) {
        fail("invalid number '" + t.text + "'");
        return;
    }
    out.push_back(t);
}

void Lexer::lexString(char quote) {
    Token t;
    t.type = Token::Type::String;
    t.line = line;
    std::string value;
    while (!atEnd()) {
        char c = advance();
        if (c == quote) {
            t.text = value;
            out.push_back(t);
            return;
        }
        if (c == '\n') {
            line++;
            fail("unterminated string");
            return;
        }
        if (c == '\\') {
            if (atEnd()) break;
            char e = advance();
            switch (e) {
                case 'n': value += '\n'; break;
                case 't': value += '\t'; break;
                case 'r': value += '\r'; break;
                case '\\': value += '\\'; break;
                case '\'': value += '\''; break;
                case '"': value += '"'; break;
                case '0': value += '\0'; break;
                default: value += e; break;
            }
        } else {
            value += c;
        }
    }
    fail("unterminated string");
}

Token::Type Lexer::keyword(const std::string& word) {
    if (word == "var") return Token::Type::Var;
    if (word == "function") return Token::Type::Function;
    if (word == "return") return Token::Type::Return;
    if (word == "if") return Token::Type::If;
    if (word == "else") return Token::Type::Else;
    if (word == "while") return Token::Type::While;
    if (word == "for") return Token::Type::For;
    if (word == "foreach") return Token::Type::Foreach;
    if (word == "switch") return Token::Type::Switch;
    if (word == "case") return Token::Type::Case;
    if (word == "default") return Token::Type::Default;
    if (word == "break") return Token::Type::Break;
    if (word == "continue") return Token::Type::Continue;
    if (word == "try") return Token::Type::Try;
    if (word == "catch") return Token::Type::Catch;
    if (word == "import") return Token::Type::Import;
    if (word == "from") return Token::Type::From;
    if (word == "as") return Token::Type::As;
    if (word == "true") return Token::Type::True;
    if (word == "false") return Token::Type::False;
    if (word == "null") return Token::Type::Null;
    if (word == "this") return Token::Type::This;
    return Token::Type::Ident; // `in` stays contextual (parsed in foreach headers)
}

void Lexer::lexIdent() {
    size_t start = pos;
    while (!atEnd()) {
        unsigned char uc = static_cast<unsigned char>(peek());
        if (uc >= 0x80 || peek() == '_' || (peek() >= 'a' && peek() <= 'z') ||
            (peek() >= 'A' && peek() <= 'Z') || (peek() >= '0' && peek() <= '9')) {
            advance();
        } else {
            break;
        }
    }
    std::string word = src.substr(start, pos - start);
    Token t;
    t.line = line;
    t.type = keyword(word);
    t.text = word;
    out.push_back(t);
}

std::vector<Token> Lexer::tokenize(bool& ok, std::string& err) {
    while (!atEnd() && !failed) {
        char c = peek();
        if (c == ' ' || c == '\t' || c == '\r' || c == '\f' || c == '\v') {
            advance();
            continue;
        }
        if (c == '\n') {
            if (depth == 0) {
                Token t;
                t.type = Token::Type::Newline;
                t.line = line;
                out.push_back(t);
            }
            advance();
            line++;
            continue;
        }
        if (c == '/' && peekNext() == '/') {
            advance();
            advance();
            skipLineComment();
            continue;
        }
        if (c == '/' && peekNext() == '*') {
            advance();
            if (!skipBlockComment()) break;
            continue;
        }
        if ((c >= '0' && c <= '9') || (c == '.' && peekNext() >= '0' && peekNext() <= '9')) {
            if (c == '.') {
                // Leading-dot float (".5"): rewind-safe path via lexNumber at dot
                Token t;
                t.line = line;
                size_t start = pos;
                advance();
                while (peek() >= '0' && peek() <= '9') advance();
                t.type = Token::Type::Float;
                t.text = src.substr(start, pos - start);
                try {
                    t.number = std::stod(t.text);
                } catch (...) {
                    fail("invalid number '" + t.text + "'");
                    break;
                }
                out.push_back(t);
                continue;
            }
            lexNumber();
            continue;
        }
        if (c == '"' || c == '\'') {
            advance();
            lexString(c);
            continue;
        }
        if (c == '_' || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
            static_cast<unsigned char>(c) >= 0x80) {
            lexIdent();
            continue;
        }

        Token t;
        t.line = line;
        switch (c) {
            case '(': t.type = Token::Type::LParen; depth++; advance(); break;
            case ')': t.type = Token::Type::RParen; if (depth > 0) depth--; advance(); break;
            case '[': t.type = Token::Type::LBracket; depth++; advance(); break;
            case ']': t.type = Token::Type::RBracket; if (depth > 0) depth--; advance(); break;
            case '{': t.type = Token::Type::LBrace; advance(); break;
            case '}': t.type = Token::Type::RBrace; advance(); break;
            case ',': t.type = Token::Type::Comma; advance(); break;
            case ';': t.type = Token::Type::Semi; advance(); break;
            case ':': t.type = Token::Type::Colon; advance(); break;
            case '?': t.type = Token::Type::Question; advance(); break;
            case '.': t.type = Token::Type::Dot; advance(); break;
            case '+':
                advance();
                if (peek() == '+') { advance(); t.type = Token::Type::PlusPlus; }
                else if (peek() == '=') { advance(); t.type = Token::Type::PlusEq; }
                else t.type = Token::Type::Plus;
                break;
            case '-':
                advance();
                if (peek() == '-') { advance(); t.type = Token::Type::MinusMinus; }
                else if (peek() == '=') { advance(); t.type = Token::Type::MinusEq; }
                else t.type = Token::Type::Minus;
                break;
            case '*':
                advance();
                if (peek() == '=') { advance(); t.type = Token::Type::StarEq; }
                else t.type = Token::Type::Star;
                break;
            case '/':
                advance();
                if (peek() == '=') { advance(); t.type = Token::Type::SlashEq; }
                else t.type = Token::Type::Slash;
                break;
            case '%': t.type = Token::Type::Percent; advance(); break;
            case '=':
                advance();
                if (peek() == '=') { advance(); t.type = Token::Type::EqEq; }
                else t.type = Token::Type::Eq;
                break;
            case '!':
                advance();
                if (peek() == '=') { advance(); t.type = Token::Type::BangEq; }
                else t.type = Token::Type::Bang;
                break;
            case '<':
                advance();
                if (peek() == '=') { advance(); t.type = Token::Type::LtEq; }
                else t.type = Token::Type::Lt;
                break;
            case '>':
                advance();
                if (peek() == '=') { advance(); t.type = Token::Type::GtEq; }
                else t.type = Token::Type::Gt;
                break;
            case '&':
                advance();
                if (peek() == '&') { advance(); t.type = Token::Type::AmpAmp; }
                else { fail("unexpected '&' (use '&&')"); }
                break;
            case '|':
                advance();
                if (peek() == '|') { advance(); t.type = Token::Type::PipePipe; }
                else { fail("unexpected '|' (use '||')"); }
                break;
            default:
                fail(std::string("unexpected character '") + c + "'");
                advance();
                continue;
        }
        if (!failed) out.push_back(t);
    }
    Token end;
    end.type = Token::Type::End;
    end.line = line;
    out.push_back(end);
    ok = !failed;
    err = error;
    return out;
}

} // namespace Kodo
