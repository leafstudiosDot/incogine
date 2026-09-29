// Incogine Studio — C++ scene round-trip parser core (Qt-free, stdlib).
// Part of Incogine by leafstudiosDot (MPL-2.0). See LICENSE.
//
// Constructor scan + statement matching. Text/bracket utilities live in
// scene_cpp_text.cpp, font labels in scene_cpp_fonts.cpp, mutations in
// scene_cpp_ops.cpp; shared declarations in scene_cpp_detail.h.
// Public API: scene_cpp.h.
#include "scene_cpp.h"
#include "scene_cpp_detail.h"

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
            const size_t close = detail::MatchBracket(text, i, '}');
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
                if (!detail::Trim(detail::StripComments(text.substr(stmtBegin, i - stmtBegin))).empty()) {
                    out.push_back({text.substr(stmtBegin, i - stmtBegin), stmtBegin, i});
                }
                const size_t close = detail::MatchBracket(text, i, '}');
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
    std::string code = detail::CollapseWhitespace(detail::StripComments(raw));
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
            const std::vector<std::string> args = detail::SplitTopLevel(m[3].str(), ',');
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
                const std::string arg = detail::Trim(args[i]);
                if (std::regex_match(arg, vm, vecRe)) {
                    const VecExpr vec = detail::ParseVec(vm[2].str());
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
            stmt.vec = detail::ParseVec(m[4].str());
            return true;
        }
    }
    static const std::regex addCompRe("^(.+?)\\s*->\\s*addComponent\\s*\\((.*)\\)\\s*;$");
    if (std::regex_match(code, m, addCompRe)) {
        stmt.kind = Statement::Kind::AddComponent;
        stmt.target = NormalizeTarget(m[1].str());
        stmt.rawArg = detail::Trim(m[2].str());
        return true;
    }
    static const std::regex setParentRe("^(.+?)\\s*->\\s*setParent\\s*\\((.*)\\)\\s*;$");
    if (std::regex_match(code, m, setParentRe)) {
        stmt.kind = Statement::Kind::SetParent;
        stmt.target = NormalizeTarget(m[1].str());
        stmt.rawArg = detail::Trim(m[2].str());
        if (detail::IsIdentifier(stmt.rawArg)) {
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
    file.headerEol = detail::DetectEol(headerText);
    file.sourceEol = detail::DetectEol(sourceText);
    file.headerTrailingNewline =
        headerText.size() >= file.headerEol.size() &&
        headerText.compare(headerText.size() - file.headerEol.size(),
                           file.headerEol.size(), file.headerEol) == 0;
    file.sourceTrailingNewline =
        sourceText.size() >= file.sourceEol.size() &&
        sourceText.compare(sourceText.size() - file.sourceEol.size(),
                           file.sourceEol.size(), file.sourceEol) == 0;
    file.headerLines = detail::SplitLines(headerText, file.headerEol);
    file.sourceLines = detail::SplitLines(sourceText, file.sourceEol);
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
    const size_t argsEnd = detail::MatchBracket(sourceText, cursor, ')');
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
    const size_t bodyClose = detail::MatchBracket(sourceText, bodyOpen, '}');
    if (bodyClose == std::string::npos) {
        error = "unbalanced constructor body";
        return false;
    }
    const std::vector<size_t> starts = detail::LineStarts(sourceText);
    model.ctorBodyBeginLine = detail::OffsetToLine(starts, bodyOpen);
    model.ctorBodyEndLine = detail::OffsetToLine(starts, bodyClose);

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
                    const std::vector<size_t> hStarts = detail::LineStarts(headerText);
                    model.headerPrivateLine =
                        detail::OffsetToLine(hStarts, classOff + privOff);
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
        const std::string stripped = detail::Trim(detail::StripComments(raw.text));
        if (stripped.empty() || stripped == ";") {
            continue;
        }
        Statement stmt;
        stmt.beginLine = detail::OffsetToLine(starts, raw.beginOff);
        stmt.endLine = detail::OffsetToLine(starts, raw.endOff > 0 ? raw.endOff - 1 : raw.beginOff);
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

    // Font/text labels: header decls + file-wide call sites.
    detail::ParseFonts(headerText, sourceText, model);

    // Body indent unit: first non-empty body line's leading whitespace.
    model.indent = "\t";
    for (size_t ln = model.ctorBodyBeginLine + 1; ln < model.ctorBodyEndLine && ln <= file.sourceLines.size(); ++ln) {
        const std::string& line = file.sourceLines[ln - 1];
        if (!detail::Trim(line).empty()) {
            const std::string ws = detail::LeadingWhitespace(line);
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
} // namespace scenecpp
} // namespace studio
} // namespace icg
