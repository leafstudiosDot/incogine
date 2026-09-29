// Incogine Studio — C++ scene round-trip implementation (Qt-free, stdlib).
// Part of Incogine by leafstudiosDot (MPL-2.0). See LICENSE.
#include "scene_cpp.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <regex>
#include <sstream>
#include <unordered_map>

namespace icg {
namespace studio {
namespace scenecpp {
namespace {

// ---------- text utilities ----------

std::string DetectEol(const std::string& text) {
    return text.find("\r\n") != std::string::npos ? "\r\n" : "\n";
}

std::vector<std::string> SplitLines(const std::string& text, const std::string& eol) {
    std::vector<std::string> lines;
    size_t start = 0;
    while (start <= text.size()) {
        size_t pos = text.find(eol, start);
        if (pos == std::string::npos) {
            lines.push_back(text.substr(start));
            break;
        }
        lines.push_back(text.substr(start, pos - start));
        start = pos + eol.size();
    }
    // A trailing EOL produces one empty phantom line; drop it and let the
    // trailing-newline flag round-trip instead.
    if (!lines.empty() && lines.back().empty() &&
        (text.size() >= eol.size() &&
         text.compare(text.size() - eol.size(), eol.size(), eol) == 0)) {
        lines.pop_back();
    }
    return lines;
}

std::string JoinLines(const std::vector<std::string>& lines, const std::string& eol,
                      bool trailingNewline) {
    std::string out;
    for (size_t i = 0; i < lines.size(); ++i) {
        if (i > 0) {
            out += eol;
        }
        out += lines[i];
    }
    if (trailingNewline) {
        out += eol;
    }
    return out;
}

std::string Trim(const std::string& s) {
    size_t b = 0;
    while (b < s.size() && std::isspace(static_cast<unsigned char>(s[b]))) {
        ++b;
    }
    size_t e = s.size();
    while (e > b && std::isspace(static_cast<unsigned char>(s[e - 1]))) {
        --e;
    }
    return s.substr(b, e - b);
}

std::string LeadingWhitespace(const std::string& s) {
    size_t i = 0;
    while (i < s.size() && (s[i] == ' ' || s[i] == '\t')) {
        ++i;
    }
    return s.substr(0, i);
}

std::string CollapseWhitespace(const std::string& s) {
    std::string out;
    bool inSpace = true; // trim leading
    for (char c : s) {
        if (std::isspace(static_cast<unsigned char>(c))) {
            if (!inSpace) {
                out += ' ';
                inSpace = true;
            }
        } else {
            out += c;
            inSpace = false;
        }
    }
    if (!out.empty() && out.back() == ' ') {
        out.pop_back();
    }
    return out;
}

// Splits on a delimiter at paren/bracket/brace depth 0, strings/comments
// intact (comments kept in the pieces for raw fidelity).
std::vector<std::string> SplitTopLevel(const std::string& s, char delim) {
    std::vector<std::string> parts;
    std::string cur;
    int pDepth = 0, bDepth = 0, cDepth = 0;
    bool inStr = false, inChar = false, inLine = false, inBlock = false;
    char strQuote = 0;
    for (size_t i = 0; i < s.size(); ++i) {
        const char c = s[i];
        const char n = i + 1 < s.size() ? s[i + 1] : 0;
        if (inLine) {
            cur += c;
            if (c == '\n') {
                inLine = false;
            }
            continue;
        }
        if (inBlock) {
            cur += c;
            if (c == '*' && n == '/') {
                cur += n;
                ++i;
                inBlock = false;
            }
            continue;
        }
        if (inStr || inChar) {
            cur += c;
            if (c == '\\' && n) {
                cur += n;
                ++i;
            } else if (c == strQuote) {
                inStr = inChar = false;
            }
            continue;
        }
        if (c == '/' && n == '/') {
            inLine = true;
            cur += c;
            continue;
        }
        if (c == '/' && n == '*') {
            inBlock = true;
            cur += c;
            continue;
        }
        if (c == '"' || c == '\'') {
            inStr = (c == '"');
            inChar = (c == '\'');
            strQuote = c;
            cur += c;
            continue;
        }
        if (c == '(') {
            ++pDepth;
        } else if (c == ')') {
            --pDepth;
        } else if (c == '[') {
            ++bDepth;
        } else if (c == ']') {
            --bDepth;
        } else if (c == '{') {
            ++cDepth;
        } else if (c == '}') {
            --cDepth;
        }
        if (c == delim && pDepth == 0 && bDepth == 0 && cDepth == 0) {
            parts.push_back(cur);
            cur.clear();
        } else {
            cur += c;
        }
    }
    parts.push_back(cur);
    return parts;
}

// Comment-stripped copy for pattern matching (strings preserved).
std::string StripComments(const std::string& s) {
    std::string out;
    bool inStr = false, inChar = false, inLine = false, inBlock = false;
    char quote = 0;
    for (size_t i = 0; i < s.size(); ++i) {
        const char c = s[i];
        const char n = i + 1 < s.size() ? s[i + 1] : 0;
        if (inLine) {
            if (c == '\n') {
                inLine = false;
                out += c;
            } else {
                out += ' ';
            }
            continue;
        }
        if (inBlock) {
            if (c == '*' && n == '/') {
                out += "  ";
                ++i;
                inBlock = false;
            } else if (c == '\n') {
                out += c;
            } else {
                out += ' ';
            }
            continue;
        }
        if (inStr || inChar) {
            out += c;
            if (c == '\\' && n) {
                out += n;
                ++i;
            } else if (c == quote) {
                inStr = inChar = false;
            }
            continue;
        }
        if (c == '/' && n == '/') {
            inLine = true;
            out += "  ";
            ++i;
            continue;
        }
        if (c == '/' && n == '*') {
            inBlock = true;
            out += "  ";
            ++i;
            continue;
        }
        if (c == '"' || c == '\'') {
            inStr = (c == '"');
            inChar = (c == '\'');
            quote = c;
        }
        out += c;
    }
    return out;
}

bool ParseDouble(const std::string& s, double& out) {
    try {
        size_t pos = 0;
        out = std::stod(Trim(s), &pos);
        return pos == Trim(s).size();
    } catch (...) {
        return false;
    }
}

VecExpr ParseVec(const std::string& inner) {
    VecExpr vec;
    for (const std::string& part : SplitTopLevel(inner, ',')) {
        vec.parts.push_back(Trim(part));
    }
    vec.numeric = !vec.parts.empty();
    for (const std::string& part : vec.parts) {
        double v = 0;
        if (!ParseDouble(part, v)) {
            vec.numeric = false;
            break;
        }
        vec.values.push_back(v);
    }
    if (!vec.numeric) {
        vec.values.clear();
    }
    return vec;
}

std::string EscapeLiteral(const std::string& s, bool& ok) {
    ok = true;
    std::string out;
    for (char c : s) {
        if (c == '\r' || c == '\n') {
            ok = false;
            return {};
        }
        if (c == '\\' || c == '"') {
            out += '\\';
        }
        out += c;
    }
    return out;
}

bool IsIdentifier(const std::string& s) {
    if (s.empty() || !(std::isalpha(static_cast<unsigned char>(s[0])) || s[0] == '_')) {
        return false;
    }
    for (char c : s) {
        if (!std::isalnum(static_cast<unsigned char>(c)) && c != '_') {
            return false;
        }
    }
    return true;
}

// ---------- offset <-> line mapping ----------

std::vector<size_t> LineStarts(const std::string& text) {
    std::vector<size_t> starts = {0};
    for (size_t i = 0; i < text.size(); ++i) {
        if (text[i] == '\n') {
            starts.push_back(i + 1);
        }
    }
    return starts;
}

size_t OffsetToLine(const std::vector<size_t>& starts, size_t off) {
    // 1-based line containing offset off.
    size_t lo = 0, hi = starts.size();
    while (lo < hi) {
        const size_t mid = lo + (hi - lo) / 2;
        if (starts[mid] <= off) {
            lo = mid + 1;
        } else {
            hi = mid;
        }
    }
    return lo; // count of starts <= off == 1-based line
}

// Finds `close` matching the opener at `open` (which must be ( [ or {),
// strings/comments/chars skipped. Returns npos when unbalanced.
size_t MatchBracket(const std::string& text, size_t open, char close) {
    char stack[256];
    int depth = 0;
    bool inStr = false, inChar = false, inLine = false, inBlock = false;
    char quote = 0;
    for (size_t i = open; i < text.size(); ++i) {
        const char c = text[i];
        const char n = i + 1 < text.size() ? text[i + 1] : 0;
        if (inLine) {
            if (c == '\n') {
                inLine = false;
            }
            continue;
        }
        if (inBlock) {
            if (c == '*' && n == '/') {
                ++i;
                inBlock = false;
            }
            continue;
        }
        if (inStr || inChar) {
            if (c == '\\' && n) {
                ++i;
            } else if (c == quote) {
                inStr = inChar = false;
            }
            continue;
        }
        if (c == '/' && n == '/') {
            inLine = true;
            ++i;
            continue;
        }
        if (c == '/' && n == '*') {
            inBlock = true;
            ++i;
            continue;
        }
        if (c == '"' || c == '\'') {
            inStr = (c == '"');
            inChar = (c == '\'');
            quote = c;
            continue;
        }
        if (c == '(' || c == '[' || c == '{') {
            if (depth < 256) {
                stack[depth++] = c;
            }
            continue;
        }
        if (c == ')' || c == ']' || c == '}') {
            if (depth == 0) {
                return std::string::npos;
            }
            const char o = stack[--depth];
            const bool pairs = (o == '(' && c == ')') || (o == '[' && c == ']') ||
                               (o == '{' && c == '}');
            if (!pairs) {
                return std::string::npos;
            }
            if (depth == 0 && i > open) {
                return (c == close) ? i : std::string::npos;
            }
            if (depth == 0) {
                return std::string::npos;
            }
        }
    }
    return std::string::npos;
}

} // namespace

// ---------- parsing ----------

namespace {

struct RawStmt {
    std::string text; // raw slice (may span lines)
    size_t beginOff = 0, endOff = 0; // [begin, end) offsets into the source
};

// Splits [scanBegin, scanEnd) into top-level `;` statements and `{...}`
// blocks. A lone `}` at depth 0 ends the scan (constructor close brace).
std::vector<RawStmt> SplitStatements(const std::string& text, size_t scanBegin,
                                     size_t scanEnd) {
    std::vector<RawStmt> out;
    size_t i = scanBegin;
    auto skipTrivia = [&](bool& hitClose) {
        hitClose = false;
        while (i < scanEnd) {
            const char c = text[i];
            const char n = i + 1 < scanEnd ? text[i + 1] : 0;
            if (c == '/' && n == '/') {
                while (i < scanEnd && text[i] != '\n') {
                    ++i;
                }
                continue;
            }
            if (c == '/' && n == '*') {
                i += 2;
                while (i + 1 < scanEnd && !(text[i] == '*' && text[i + 1] == '/')) {
                    ++i;
                }
                i += 2;
                continue;
            }
            if (std::isspace(static_cast<unsigned char>(c))) {
                ++i;
                continue;
            }
            if (c == '}' ) {
                hitClose = true;
                return;
            }
            return;
        }
    };
    while (i < scanEnd) {
        bool hitClose = false;
        skipTrivia(hitClose);
        if (i >= scanEnd || hitClose) {
            break;
        }
        size_t stmtBegin = i;
        if (text[i] == '{') {
            const size_t close = MatchBracket(text, i, '}');
            if (close == std::string::npos) {
                break;
            }
            out.push_back({text.substr(stmtBegin, close - stmtBegin + 1), stmtBegin,
                           close + 1});
            i = close + 1;
            continue;
        }
        // Accumulate until ';' at depth 0.
        int pDepth = 0, bDepth = 0, cDepth = 0;
        bool inStr = false, inChar = false, inLine = false, inBlock = false;
        char quote = 0;
        bool done = false;
        while (i < scanEnd && !done) {
            const char c = text[i];
            const char n = i + 1 < scanEnd ? text[i + 1] : 0;
            if (inLine) {
                if (c == '\n') {
                    inLine = false;
                }
                ++i;
                continue;
            }
            if (inBlock) {
                if (c == '*' && n == '/') {
                    i += 2;
                    inBlock = false;
                } else {
                    ++i;
                }
                continue;
            }
            if (inStr || inChar) {
                if (c == '\\' && n) {
                    i += 2;
                } else {
                    if (c == quote) {
                        inStr = inChar = false;
                    }
                    ++i;
                }
                continue;
            }
            if (c == '/' && n == '/') {
                inLine = true;
                i += 2;
                continue;
            }
            if (c == '/' && n == '*') {
                inBlock = true;
                i += 2;
                continue;
            }
            if (c == '"' || c == '\'') {
                inStr = (c == '"');
                inChar = (c == '\'');
                quote = c;
                ++i;
                continue;
            }
            if (c == '(') {
                ++pDepth;
            } else if (c == ')') {
                --pDepth;
            } else if (c == '[') {
                ++bDepth;
            } else if (c == ']') {
                --bDepth;
            } else if (c == '{' && pDepth == 0 && bDepth == 0 && cDepth == 0) {
                // Control-flow header (for/if/while/...) or block: the
                // accumulated header is a complete statement, and the
                // balanced block is its own statement. Both stay Unknown
                // (verbatim), but crucially they must not swallow the
                // statements that follow them.
                if (!Trim(StripComments(text.substr(stmtBegin, i - stmtBegin))).empty()) {
                    out.push_back({text.substr(stmtBegin, i - stmtBegin), stmtBegin, i});
                }
                const size_t close = MatchBracket(text, i, '}');
                if (close == std::string::npos || close >= scanEnd) {
                    i = scanEnd;
                    break;
                }
                out.push_back({text.substr(i, close - i + 1), i, close + 1});
                i = close + 1;
                stmtBegin = i; // suppress the trailing push below (already emitted)
                done = true; // resume fresh accumulation after the block
                continue;
            } else if (c == '{') {
                ++cDepth;
            } else if (c == '}') {
                if (cDepth == 0) {
                    break; // constructor close; handled by caller range
                }
                --cDepth;
            } else if (c == ';' && pDepth == 0 && bDepth == 0 && cDepth == 0) {
                ++i;
                done = true;
                continue;
            }
            ++i;
        }
        if (i > stmtBegin) {
            out.push_back({text.substr(stmtBegin, i - stmtBegin), stmtBegin, i});
        }
    }
    return out;
}

std::string NormalizeTarget(std::string t) {
    // Collapse `this -> x` / `this->x` to `x`.
    std::string out;
    for (size_t i = 0; i < t.size();) {
        if (std::isspace(static_cast<unsigned char>(t[i]))) {
            ++i;
            continue;
        }
        if (t.compare(i, 2, "->") == 0) {
            out += "->";
            i += 2;
            continue;
        }
        out += t[i++];
    }
    const std::string prefix = "this->";
    if (out.compare(0, prefix.size(), prefix) == 0) {
        out = out.substr(prefix.size());
    }
    return out;
}

bool MatchStatement(const std::string& raw, Statement& stmt) {
    std::string code = CollapseWhitespace(StripComments(raw));
    // Normalize `a -> b` to `a->b`.
    {
        std::string norm;
        for (size_t i = 0; i < code.size();) {
            if (std::isspace(static_cast<unsigned char>(code[i])) && i + 1 < code.size() &&
                code.compare(i + 1, 2, "->") == 0) {
                norm += "->";
                i += 3;
                while (i < code.size() && std::isspace(static_cast<unsigned char>(code[i]))) {
                    ++i;
                }
                continue;
            }
            norm += code[i++];
        }
        code = norm;
    }
    if (code.empty()) {
        return false;
    }
    std::smatch m;
    static const std::regex constructRe(
        "^([A-Za-z_]\\w*)\\s*=\\s*new\\s+(Square|Cube|Object)\\s*\\((.*)\\)\\s*;$");
    if (std::regex_match(code, m, constructRe)) {
        stmt.kind = Statement::Kind::Construct;
        stmt.target = NormalizeTarget(m[1].str());
        stmt.typeName = m[2].str();
        if (stmt.typeName == "Object") {
            const std::vector<std::string> args = SplitTopLevel(m[3].str(), ',');
            if (!args.empty()) {
                static const std::regex strRe("^\\s*\"((?:[^\"\\\\]|\\\\.)*)\"\\s*$");
                std::smatch sm;
                if (std::regex_match(args[0], sm, strRe)) {
                    stmt.displayName = sm[1].str();
                }
            }
            for (size_t i = 1; i < args.size(); ++i) {
                static const std::regex vecRe("^(Position|Scale|Rotation)\\s*\\((.*)\\)\\s*$");
                std::smatch vm;
                const std::string arg = Trim(args[i]);
                if (std::regex_match(arg, vm, vecRe)) {
                    const VecExpr vec = ParseVec(vm[2].str());
                    const std::string which = vm[1].str();
                    if (which == "Position") {
                        stmt.position = vec;
                        stmt.hasPosition = true;
                    } else if (which == "Scale") {
                        stmt.scale = vec;
                        stmt.hasScale = true;
                    } else if (which == "Rotation") {
                        stmt.rotation = vec;
                        stmt.hasRotation = true;
                    }
                }
            }
        }
        return true;
    }
    static const std::regex setNameRe(
        "^(.+?)\\s*->\\s*setName\\s*\\(\\s*\"((?:[^\"\\\\]|\\\\.)*)\"\\s*\\)\\s*;$");
    if (std::regex_match(code, m, setNameRe)) {
        stmt.kind = Statement::Kind::SetName;
        stmt.target = NormalizeTarget(m[1].str());
        stmt.displayName = m[2].str();
        return true;
    }
    static const std::regex setIdRe(
        "^(.+?)\\s*->\\s*setId\\s*\\(\\s*([0-9]+)(?:[uU](?:ll|LL)?|[lL](?:l|L)?)?\\s*\\)\\s*;$");
    if (std::regex_match(code, m, setIdRe)) {
        stmt.kind = Statement::Kind::SetId;
        stmt.target = NormalizeTarget(m[1].str());
        try {
            stmt.id = std::stoull(m[2].str());
        } catch (...) {
            return false;
        }
        stmt.hasId = true;
        return true;
    }
    static const std::regex transformRe(
        "^(.+?)\\s*->\\s*set(Position|Scale|Rotation|Color)\\s*\\(\\s*"
        "(Position|Scale|Rotation|Color)\\s*\\((.*)\\)\\s*\\)\\s*;$");
    if (std::regex_match(code, m, transformRe)) {
        const std::string setter = m[2].str();
        const std::string wrapper = m[3].str();
        if (setter == wrapper) {
            if (setter == "Position") {
                stmt.kind = Statement::Kind::SetPosition;
            } else if (setter == "Scale") {
                stmt.kind = Statement::Kind::SetScale;
            } else if (setter == "Rotation") {
                stmt.kind = Statement::Kind::SetRotation;
            } else {
                stmt.kind = Statement::Kind::SetColor;
            }
            stmt.target = NormalizeTarget(m[1].str());
            stmt.vec = ParseVec(m[4].str());
            return true;
        }
    }
    static const std::regex addCompRe("^(.+?)\\s*->\\s*addComponent\\s*\\((.*)\\)\\s*;$");
    if (std::regex_match(code, m, addCompRe)) {
        stmt.kind = Statement::Kind::AddComponent;
        stmt.target = NormalizeTarget(m[1].str());
        stmt.rawArg = Trim(m[2].str());
        return true;
    }
    static const std::regex setParentRe("^(.+?)\\s*->\\s*setParent\\s*\\((.*)\\)\\s*;$");
    if (std::regex_match(code, m, setParentRe)) {
        stmt.kind = Statement::Kind::SetParent;
        stmt.target = NormalizeTarget(m[1].str());
        stmt.rawArg = Trim(m[2].str());
        if (IsIdentifier(stmt.rawArg)) {
            stmt.parentVar = stmt.rawArg;
        }
        return true;
    }
    return false;
}

} // namespace

bool ParseSceneText(const std::string& headerText, const std::string& sourceText,
                    const std::string& headerName, const std::string& sourceName,
                    SceneFile& out, std::string& error) {
    SceneFile file;
    file.headerText = headerText;
    file.sourceText = sourceText;
    file.headerEol = DetectEol(headerText);
    file.sourceEol = DetectEol(sourceText);
    file.headerTrailingNewline =
        headerText.size() >= file.headerEol.size() &&
        headerText.compare(headerText.size() - file.headerEol.size(),
                           file.headerEol.size(), file.headerEol) == 0;
    file.sourceTrailingNewline =
        sourceText.size() >= file.sourceEol.size() &&
        sourceText.compare(sourceText.size() - file.sourceEol.size(),
                           file.sourceEol.size(), file.sourceEol) == 0;
    file.headerLines = SplitLines(headerText, file.headerEol);
    file.sourceLines = SplitLines(sourceText, file.sourceEol);
    SceneModel& model = file.model;
    model.headerFile = headerName;
    model.sourceFile = sourceName;

    // Constructor: `Class::Class(...)` + body `{`.
    static const std::regex ctorRe("\\b([A-Za-z_]\\w*)::\\1\\s*\\(");
    std::smatch cm;
    if (!std::regex_search(sourceText, cm, ctorRe)) {
        error = "no constructor definition found";
        return false;
    }
    model.className = cm[1].str();
    size_t cursor = static_cast<size_t>(cm.position(0) + cm.length(0)) - 1; // at '('
    const size_t argsEnd = MatchBracket(sourceText, cursor, ')');
    if (argsEnd == std::string::npos) {
        error = "unbalanced constructor signature";
        return false;
    }
    cursor = argsEnd + 1;
    // Skip init list / qualifiers to the body '{' (paren-aware).
    size_t bodyOpen = std::string::npos;
    {
        int pDepth = 0;
        bool inStr = false, inChar = false, inLine = false, inBlock = false;
        char quote = 0;
        for (size_t i = cursor; i < sourceText.size(); ++i) {
            const char c = sourceText[i];
            const char n = i + 1 < sourceText.size() ? sourceText[i + 1] : 0;
            if (inLine) {
                if (c == '\n') {
                    inLine = false;
                }
                continue;
            }
            if (inBlock) {
                if (c == '*' && n == '/') {
                    ++i;
                    inBlock = false;
                }
                continue;
            }
            if (inStr || inChar) {
                if (c == '\\' && n) {
                    ++i;
                } else if (c == quote) {
                    inStr = inChar = false;
                }
                continue;
            }
            if (c == '/' && n == '/') {
                inLine = true;
                ++i;
                continue;
            }
            if (c == '/' && n == '*') {
                inBlock = true;
                ++i;
                continue;
            }
            if (c == '"' || c == '\'') {
                inStr = (c == '"');
                inChar = (c == '\'');
                quote = c;
                continue;
            }
            if (c == '(') {
                ++pDepth;
            } else if (c == ')') {
                --pDepth;
            } else if (c == '{' && pDepth == 0) {
                bodyOpen = i;
                break;
            } else if (c == ';' && pDepth == 0) {
                break; // declaration, not a definition
            }
        }
    }
    if (bodyOpen == std::string::npos) {
        error = "constructor body not found";
        return false;
    }
    const size_t bodyClose = MatchBracket(sourceText, bodyOpen, '}');
    if (bodyClose == std::string::npos) {
        error = "unbalanced constructor body";
        return false;
    }
    const std::vector<size_t> starts = LineStarts(sourceText);
    model.ctorBodyBeginLine = OffsetToLine(starts, bodyOpen);
    model.ctorBodyEndLine = OffsetToLine(starts, bodyClose);

    // Declared scene name from the init list (`: Scene("...")`).
    {
        static const std::regex sceneNameRe("Scene\\s*\\(\\s*\"((?:[^\"\\\\]|\\\\.)*)\"\\s*\\)");
        const std::string initRegion =
            sourceText.substr(argsEnd, bodyOpen - argsEnd);
        std::smatch sm;
        if (std::regex_search(initRegion, sm, sceneNameRe)) {
            model.declaredName = sm[1].str();
        }
    }

    // Header: Scene base + unambiguous `private:` for member inserts.
    {
        static const std::regex classRe("class\\s+([A-Za-z_]\\w*)");
        std::smatch hm;
        std::string::const_iterator searchFrom = headerText.begin();
        while (std::regex_search(searchFrom, headerText.cend(), hm, classRe)) {
            const std::string found = hm[1].str();
            if (found == model.className) {
                model.headerClassFound = true;
                const size_t classOff =
                    static_cast<size_t>(hm.position(0)) +
                    static_cast<size_t>(searchFrom - headerText.begin());
                const size_t classEnd = headerText.find("};", classOff);
                const std::string classBody = headerText.substr(
                    classOff, classEnd == std::string::npos
                                  ? std::string::npos
                                  : classEnd - classOff);
                static const std::regex privRe("(^|\n)([ \t]*)private\\s*:");
                std::sregex_iterator it(classBody.begin(), classBody.end(), privRe);
                std::sregex_iterator end;
                int privCount = 0;
                size_t privOff = 0;
                std::string privIndent;
                for (; it != end; ++it) {
                    ++privCount;
                    privOff = static_cast<size_t>((*it).position(2));
                    privIndent = (*it)[2].str();
                }
                if (privCount == 1) {
                    const std::vector<size_t> hStarts = LineStarts(headerText);
                    model.headerPrivateLine =
                        OffsetToLine(hStarts, classOff + privOff);
                    model.headerIndent = privIndent + "    ";
                }
                static const std::regex sceneBaseRe(":\\s*public\\s+Scene\\b");
                if (std::regex_search(classBody, sceneBaseRe)) {
                    model.isScene = true;
                }
                break;
            }
            searchFrom = hm.suffix().first;
        }
    }

    // Split + match the constructor body.
    const std::vector<RawStmt> rawStmts =
        SplitStatements(sourceText, bodyOpen + 1, bodyClose);
    std::unordered_map<std::string, size_t> objIndex;
    for (const RawStmt& raw : rawStmts) {
        // Lone separators carry no meaning; skipping them keeps the model
        // clean without affecting serialization (which uses raw lines).
        const std::string stripped = Trim(StripComments(raw.text));
        if (stripped.empty() || stripped == ";") {
            continue;
        }
        Statement stmt;
        stmt.beginLine = OffsetToLine(starts, raw.beginOff);
        stmt.endLine = OffsetToLine(starts, raw.endOff > 0 ? raw.endOff - 1 : raw.beginOff);
        if (MatchStatement(raw.text, stmt)) {
            if (stmt.kind == Statement::Kind::Construct) {
                if (objIndex.find(stmt.target) == objIndex.end()) {
                    ObjectModel obj;
                    obj.varName = stmt.target;
                    obj.typeName = stmt.typeName;
                    obj.displayName = stmt.displayName;
                    obj.position = stmt.position;
                    obj.hasPosition = stmt.hasPosition;
                    obj.scale = stmt.scale;
                    obj.hasScale = stmt.hasScale;
                    obj.rotation = stmt.rotation;
                    obj.hasRotation = stmt.hasRotation;
                    obj.constructIndex = model.ctorStatements.size();
                    obj.hasConstruct = true;
                    obj.statementIndexes.push_back(model.ctorStatements.size());
                    objIndex[stmt.target] = model.objects.size();
                    model.objects.push_back(obj);
                }
                // Later re-constructions of the same variable are recorded
                // as plain statements (first wins for the model).
            } else {
                const auto it = objIndex.find(stmt.target);
                if (it != objIndex.end()) {
                    ObjectModel& obj = model.objects[it->second];
                    obj.statementIndexes.push_back(model.ctorStatements.size());
                    switch (stmt.kind) {
                        case Statement::Kind::SetName:
                            obj.displayName = stmt.displayName;
                            break;
                        case Statement::Kind::SetId:
                            obj.id = stmt.id;
                            obj.hasId = true;
                            break;
                        case Statement::Kind::SetPosition:
                            obj.position = stmt.vec;
                            obj.hasPosition = true;
                            break;
                        case Statement::Kind::SetScale:
                            obj.scale = stmt.vec;
                            obj.hasScale = true;
                            break;
                        case Statement::Kind::SetRotation:
                            obj.rotation = stmt.vec;
                            obj.hasRotation = true;
                            break;
                        case Statement::Kind::SetColor:
                            obj.color = stmt.vec;
                            obj.hasColor = true;
                            break;
                        case Statement::Kind::SetParent:
                            if (!stmt.parentVar.empty()) {
                                obj.parentVar = stmt.parentVar;
                                obj.hasParent = true;
                            }
                            break;
                        default:
                            break;
                    }
                }
            }
            model.ctorStatements.push_back(stmt);
        } else {
            Statement unknown;
            unknown.kind = Statement::Kind::Unknown;
            unknown.beginLine = stmt.beginLine;
            unknown.endLine = stmt.endLine;
            model.ctorStatements.push_back(unknown);
        }
    }

    // Body indent unit: first non-empty body line's leading whitespace.
    model.indent = "\t";
    for (size_t ln = model.ctorBodyBeginLine + 1; ln < model.ctorBodyEndLine && ln <= file.sourceLines.size(); ++ln) {
        const std::string& line = file.sourceLines[ln - 1];
        if (!Trim(line).empty()) {
            const std::string ws = LeadingWhitespace(line);
            if (!ws.empty()) {
                model.indent = ws;
            }
            break;
        }
    }

    out = std::move(file);
    return true;
}

bool ParseSceneFiles(const std::string& headerPath, const std::string& sourcePath,
                     SceneFile& out, std::string& error) {
    std::ifstream hIn(headerPath, std::ios::binary);
    if (!hIn) {
        error = "cannot open " + headerPath;
        return false;
    }
    std::ifstream sIn(sourcePath, std::ios::binary);
    if (!sIn) {
        error = "cannot open " + sourcePath;
        return false;
    }
    std::ostringstream hs, ss;
    hs << hIn.rdbuf();
    ss << sIn.rdbuf();
    return ParseSceneText(hs.str(), ss.str(), headerPath, sourcePath, out, error);
}

namespace {

bool Reparse(SceneFile& file, std::string& error) {
    // Edits mutate the line vectors; fold them back into the texts first,
    // otherwise re-parsing the stale text would drop every change.
    file.sourceText = SerializeSource(file);
    file.headerText = SerializeHeader(file);
    SceneFile fresh;
    if (!ParseSceneText(file.headerText, file.sourceText, file.model.headerFile,
                        file.model.sourceFile, fresh, error)) {
        return false;
    }
    file.model = std::move(fresh.model);
    file.sourceLines = std::move(fresh.sourceLines);
    file.headerLines = std::move(fresh.headerLines);
    return true;
}

const ObjectModel* FindObject(const SceneModel& model, const std::string& var) {
    for (const ObjectModel& obj : model.objects) {
        if (obj.varName == var) {
            return &obj;
        }
    }
    return nullptr;
}

size_t LastStmt(const SceneModel& model, const ObjectModel& obj, Statement::Kind kind) {
    size_t found = static_cast<size_t>(-1);
    for (size_t idx : obj.statementIndexes) {
        if (model.ctorStatements[idx].kind == kind) {
            found = idx;
        }
    }
    return found;
}

std::string StmtText(const SceneFile& file, const Statement& stmt) {
    std::string text;
    for (size_t ln = stmt.beginLine; ln <= stmt.endLine && ln <= file.sourceLines.size();
         ++ln) {
        if (ln > stmt.beginLine) {
            text += '\n';
        }
        text += file.sourceLines[ln - 1];
    }
    return text;
}

// Offset within StmtText() coordinates -> (1-based line, 0-based column).
std::pair<size_t, size_t> StmtOffsetToPos(const SceneFile& file, const Statement& stmt,
                                          size_t off) {
    size_t ln = stmt.beginLine;
    size_t rem = off;
    while (ln < stmt.endLine && ln <= file.sourceLines.size()) {
        const size_t len = file.sourceLines[ln - 1].size() + 1; // + '\n'
        if (rem < len) {
            break;
        }
        rem -= len;
        ++ln;
    }
    return {ln, rem};
}

// Span [q1, q2) of the first string literal at/after `from` (quotes incl.).
bool FindLiteral(const std::string& text, size_t from, size_t& q1, size_t& q2) {
    q1 = text.find('"', from);
    if (q1 == std::string::npos) {
        return false;
    }
    size_t i = q1 + 1;
    while (i < text.size()) {
        if (text[i] == '\\' && i + 1 < text.size()) {
            i += 2;
            continue;
        }
        if (text[i] == '"') {
            q2 = i + 1;
            return true;
        }
        ++i;
    }
    return false;
}

// Replaces [bLine,bCol)..[eLine,eCol) (1-based lines, 0-based cols) with
// '\n'-separated text.
bool SpliceRange(std::vector<std::string>& lines, size_t bLine, size_t bCol,
                 size_t eLine, size_t eCol, const std::string& text,
                 std::string& error) {
    if (bLine < 1 || eLine < bLine || eLine > lines.size()) {
        error = "splice range out of file";
        return false;
    }
    const std::string& first = lines[bLine - 1];
    const std::string& last = lines[eLine - 1];
    if (bCol > first.size() || eCol > last.size()) {
        error = "splice column out of line";
        return false;
    }
    const std::string merged = first.substr(0, bCol) + text + last.substr(eCol);
    std::vector<std::string> rep;
    size_t s = 0;
    while (true) {
        const size_t pos = merged.find('\n', s);
        if (pos == std::string::npos) {
            rep.push_back(merged.substr(s));
            break;
        }
        rep.push_back(merged.substr(s, pos - s));
        s = pos + 1;
    }
    lines.erase(lines.begin() + (bLine - 1), lines.begin() + eLine);
    lines.insert(lines.begin() + (bLine - 1), rep.begin(), rep.end());
    return true;
}

} // namespace

bool ApplyEdits(std::vector<std::string>& lines, const std::vector<CppEdit>& edits,
                std::string& error) {
    struct Op {
        size_t b, e;
        std::string text;
        bool insert;
    };
    std::vector<Op> ops;
    for (const CppEdit& ed : edits) {
        if (ed.beginLine == 0) {
            error = "edit with line 0";
            return false;
        }
        if (ed.endLine + 1 == ed.beginLine) {
            // Insert before beginLine.
            if (ed.beginLine > lines.size() + 1) {
                error = "insert past end of file";
                return false;
            }
            ops.push_back({ed.beginLine, ed.endLine, ed.text, true});
        } else {
            if (ed.endLine < ed.beginLine || ed.endLine > lines.size()) {
                error = "edit range out of file";
                return false;
            }
            ops.push_back({ed.beginLine, ed.endLine, ed.text, false});
        }
    }
    std::sort(ops.begin(), ops.end(), [](const Op& a, const Op& b) {
        if (a.b != b.b) {
            return a.b < b.b;
        }
        return a.insert && !b.insert; // inserts at a line go first
    });
    auto pushText = [](std::vector<std::string>& out, const std::string& text) {
        size_t s = 0;
        while (true) {
            const size_t pos = text.find('\n', s);
            if (pos == std::string::npos) {
                out.push_back(text.substr(s));
                break;
            }
            out.push_back(text.substr(s, pos - s));
            s = pos + 1;
        }
    };
    std::vector<std::string> out;
    size_t cursor = 0; // consumed lines
    for (const Op& op : ops) {
        if (op.insert) {
            if (op.b - 1 < cursor) {
                error = "overlapping edits";
                return false;
            }
            while (cursor < op.b - 1) {
                out.push_back(lines[cursor]);
                ++cursor;
            }
            if (!op.text.empty()) {
                pushText(out, op.text);
            }
        } else {
            if (op.b <= cursor) {
                error = "overlapping edits";
                return false;
            }
            while (cursor < op.b - 1) {
                out.push_back(lines[cursor]);
                ++cursor;
            }
            if (!op.text.empty()) {
                pushText(out, op.text);
            }
            cursor = op.e;
        }
    }
    while (cursor < lines.size()) {
        out.push_back(lines[cursor]);
        ++cursor;
    }
    lines = std::move(out);
    return true;
}

bool RenameObject(SceneFile& file, const std::string& var,
                  const std::string& newDisplay, std::string& error) {
    const ObjectModel* obj = FindObject(file.model, var);
    if (!obj) {
        error = "unknown object '" + var + "'";
        return false;
    }
    bool escOk = false;
    const std::string lit = EscapeLiteral(newDisplay, escOk);
    if (!escOk) {
        error = "display name must not contain line breaks";
        return false;
    }
    const size_t si =
        LastStmt(file.model, *obj, Statement::Kind::SetName);
    if (si == static_cast<size_t>(-1)) {
        error = "no setName call for '" + var + "' (unsupported pattern)";
        return false;
    }
    const Statement& stmt = file.model.ctorStatements[si];
    const std::string text = StmtText(file, stmt);
    static const std::regex setNameCall("setName\\s*\\(");
    std::smatch m;
    if (!std::regex_search(text, m, setNameCall)) {
        error = "cannot locate setName call";
        return false;
    }
    size_t q1 = 0, q2 = 0;
    if (!FindLiteral(text, static_cast<size_t>(m.position(0) + m.length(0)), q1, q2)) {
        error = "cannot locate setName literal";
        return false;
    }
    const auto [l1, c1] = StmtOffsetToPos(file, stmt, q1);
    const auto [l2, c2] = StmtOffsetToPos(file, stmt, q2);
    if (!SpliceRange(file.sourceLines, l1, c1, l2, c2, "\"" + lit + "\"", error)) {
        return false;
    }
    return Reparse(file, error);
}

bool SetTransform(SceneFile& file, const std::string& var, const std::string& kind,
                  const std::string& x, const std::string& y, const std::string& z,
                  const std::string& w, std::string& error) {
    const ObjectModel* obj = FindObject(file.model, var);
    if (!obj) {
        error = "unknown object '" + var + "'";
        return false;
    }
    Statement::Kind stmtKind = Statement::Kind::Unknown;
    std::string wrapper;
    std::string args;
    if (kind == "position") {
        stmtKind = Statement::Kind::SetPosition;
        wrapper = "Position";
        args = x + ", " + y + ", " + z;
    } else if (kind == "scale") {
        stmtKind = Statement::Kind::SetScale;
        wrapper = "Scale";
        args = x + ", " + y + ", " + z;
    } else if (kind == "rotation") {
        stmtKind = Statement::Kind::SetRotation;
        wrapper = "Rotation";
        args = x + ", " + y + ", " + z;
    } else if (kind == "color") {
        stmtKind = Statement::Kind::SetColor;
        wrapper = "Color";
        args = x + ", " + y + ", " + z + ", " + w;
    } else {
        error = "kind must be position|scale|rotation|color";
        return false;
    }
    // setXxx factory name mirrors the wrapper name.
    std::string setter = "set" + wrapper;
    const size_t si = LastStmt(file.model, *obj, stmtKind);
    if (si == static_cast<size_t>(-1)) {
        if (!obj->hasConstruct) {
            error = "no construction site for '" + var + "'";
            return false;
        }
        const Statement& cs = file.model.ctorStatements[obj->constructIndex];
        const CppEdit ins{cs.endLine + 1, cs.endLine,
                          file.model.indent + var + "->" + setter + "(" + wrapper +
                              "(" + args + "));"};
        if (!ApplyEdits(file.sourceLines, {ins}, error)) {
            return false;
        }
        return Reparse(file, error);
    }
    const Statement& stmt = file.model.ctorStatements[si];
    const std::string text = StmtText(file, stmt);
    const std::regex wrapRe("\\b" + wrapper + "\\s*\\(");
    std::smatch m;
    if (!std::regex_search(text, m, wrapRe)) {
        error = "cannot locate " + wrapper + " call";
        return false;
    }
    const size_t openOff = static_cast<size_t>(m.position(0) + m.length(0)) - 1;
    const size_t closeOff = MatchBracket(text, openOff, ')');
    if (closeOff == std::string::npos) {
        error = "unbalanced " + wrapper + " call";
        return false;
    }
    const auto [l1, c1] = StmtOffsetToPos(file, stmt, openOff + 1);
    const auto [l2, c2] = StmtOffsetToPos(file, stmt, closeOff);
    if (!SpliceRange(file.sourceLines, l1, c1, l2, c2, args, error)) {
        return false;
    }
    return Reparse(file, error);
}

bool SetObjectId(SceneFile& file, const std::string& var, uint64_t id,
                 std::string& error) {
    const ObjectModel* obj = FindObject(file.model, var);
    if (!obj) {
        error = "unknown object '" + var + "'";
        return false;
    }
    const size_t si = LastStmt(file.model, *obj, Statement::Kind::SetId);
    if (si == static_cast<size_t>(-1)) {
        if (!obj->hasConstruct) {
            error = "no construction site for '" + var + "'";
            return false;
        }
        const Statement& cs = file.model.ctorStatements[obj->constructIndex];
        const CppEdit ins{cs.endLine + 1, cs.endLine,
                          file.model.indent + var + "->setId(" + std::to_string(id) +
                              ");"};
        if (!ApplyEdits(file.sourceLines, {ins}, error)) {
            return false;
        }
        return Reparse(file, error);
    }
    const Statement& stmt = file.model.ctorStatements[si];
    const std::string text = StmtText(file, stmt);
    static const std::regex idCall("setId\\s*\\(\\s*");
    std::smatch m;
    if (!std::regex_search(text, m, idCall)) {
        error = "cannot locate setId call";
        return false;
    }
    const size_t numOff = static_cast<size_t>(m.position(0) + m.length(0));
    static const std::regex numRe("[0-9]+(?:[uU](?:ll|LL)?|[lL](?:l|L)?)?");
    std::smatch nm;
    const std::string tail = text.substr(numOff);
    if (!std::regex_search(tail, nm, numRe) || nm.position(0) != 0) {
        error = "cannot locate setId literal";
        return false;
    }
    const auto [l1, c1] = StmtOffsetToPos(file, stmt, numOff);
    const auto [l2, c2] =
        StmtOffsetToPos(file, stmt, numOff + static_cast<size_t>(nm.length(0)));
    if (!SpliceRange(file.sourceLines, l1, c1, l2, c2, std::to_string(id), error)) {
        return false;
    }
    return Reparse(file, error);
}

bool AddObject(SceneFile& file, const std::string& typeName, const std::string& varName,
               const std::string& displayName, uint64_t id, bool withPosition,
               const std::string& x, const std::string& y, const std::string& z,
               AddResult& result, std::string& error) {
    if (typeName != "Square" && typeName != "Cube" && typeName != "Object") {
        error = "type must be Square|Cube|Object";
        return false;
    }
    if (!IsIdentifier(varName)) {
        error = "invalid variable name '" + varName + "'";
        return false;
    }
    if (FindObject(file.model, varName)) {
        error = "object '" + varName + "' already exists";
        return false;
    }
    bool escOk = false;
    const std::string lit = EscapeLiteral(displayName, escOk);
    if (!escOk) {
        error = "display name must not contain line breaks";
        return false;
    }
    const std::string& indent = file.model.indent;
    std::string block;
    if (typeName == "Object") {
        const std::string pos = withPosition ? (x + ", " + y + ", " + z) : "0, 0, 0";
        block = indent + varName + " = new Object(\"" + lit + "\", Position(" + pos +
                "), Scale(1, 1, 1), Rotation(0, 0, 0));";
    } else {
        block = indent + varName + " = new " + typeName + "();\n" + indent + varName +
                "->setName(\"" + lit + "\");";
        if (withPosition) {
            block += "\n" + indent + varName + "->setPosition(Position(" + x + ", " +
                     y + ", " + z + "));";
        }
    }
    block += "\n" + indent + varName + "->setId(" + std::to_string(id) + ");";
    const size_t endLine = file.model.ctorBodyEndLine;
    if (endLine < 1 || endLine > file.sourceLines.size() + 1) {
        error = "constructor body end out of range";
        return false;
    }
    if (!ApplyEdits(file.sourceLines, {CppEdit{endLine, endLine - 1, block}}, error)) {
        return false;
    }
    result.headerUpdated = false;
    if (file.model.headerPrivateLine > 0 &&
        file.model.headerPrivateLine <= file.headerLines.size()) {
        const std::string decl =
            file.model.headerIndent + typeName + "* " + varName + " = nullptr;";
        if (ApplyEdits(file.headerLines,
                       {CppEdit{file.model.headerPrivateLine + 1,
                               file.model.headerPrivateLine, decl}},
                       error)) {
            result.headerUpdated = true;
        } else {
            error.clear(); // header is best-effort; source edit stands
        }
    }
    return Reparse(file, error);
}

bool RemoveObject(SceneFile& file, const std::string& var, RemoveResult& result,
                  std::string& error) {
    const ObjectModel* obj = FindObject(file.model, var);
    if (!obj) {
        error = "unknown object '" + var + "'";
        return false;
    }
    std::vector<CppEdit> dels;
    for (size_t idx : obj->statementIndexes) {
        const Statement& stmt = file.model.ctorStatements[idx];
        dels.push_back(CppEdit{stmt.beginLine, stmt.endLine, ""});
    }
    result.otherRefs = FindReferences(file.sourceLines, var, dels);
    result.needsAttention = !result.otherRefs.empty();
    if (!ApplyEdits(file.sourceLines, dels, error)) {
        return false;
    }
    return Reparse(file, error);
}

std::vector<size_t> FindReferences(const std::vector<std::string>& lines,
                                   const std::string& var,
                                   const std::vector<CppEdit>& excluded) {
    std::vector<size_t> refs;
    if (!IsIdentifier(var)) {
        return refs;
    }
    const std::regex word("\\b" + var + "\\b");
    for (size_t i = 0; i < lines.size(); ++i) {
        const size_t ln = i + 1;
        bool skip = false;
        for (const CppEdit& ed : excluded) {
            if (ed.endLine >= ed.beginLine && ln >= ed.beginLine && ln <= ed.endLine) {
                skip = true;
                break;
            }
        }
        if (skip) {
            continue;
        }
        if (std::regex_search(lines[i], word)) {
            refs.push_back(ln);
        }
    }
    return refs;
}

std::string SerializeSource(const SceneFile& file) {
    return JoinLines(file.sourceLines, file.sourceEol, file.sourceTrailingNewline);
}

std::string SerializeHeader(const SceneFile& file) {
    return JoinLines(file.headerLines, file.headerEol, file.headerTrailingNewline);
}

} // namespace scenecpp
} // namespace studio
} // namespace icg
