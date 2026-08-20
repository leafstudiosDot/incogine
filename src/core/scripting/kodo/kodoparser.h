#include <string>
#include <vector>

#ifndef KODOPARSER_H
#define KODOPARSER_H

namespace Kodo {

// Placeholder parser for the Kodo scripting language.
// Once the syntax is defined, this will contain the lexer, AST,
// and interpreter/bytecode compiler.

class Parser {
    private:
        std::string source;
        std::string filePath;

    public:
        Parser();
        ~Parser();

        bool Load(const std::string& path);
        bool Parse();
};

} // namespace Kodo

#endif
