// Incogine Studio - C++ scene edit operations (Qt-free, stdlib).
// Part of Incogine by leafstudiosDot (MPL-2.0). See LICENSE.
//
// Line-span splicing plus the high-level mutations (rename, transform,
// id, parent, add, remove) and serialization. Shared helpers live in
// scene_cpp_detail.h; text primitives in scene_cpp_text.cpp.
// Public API: scene_cpp.h.
#include "scene_cpp.h"
#include "scene_cpp_detail.h"

#include <algorithm>
#include <regex>
#include <utility>

namespace icg {
namespace studio {
namespace scenecpp {
namespace {
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

} // namespace

namespace detail {

bool Reparse(SceneFile& file, std::string& error) {
    // Edits mutate the line vectors; fold them back into the texts first,
    // otherwise re-parsing the stale text would drop every change.
    file.sourceText = SerializeSource(file);
    file.headerText = SerializeHeader(file);
    SceneFile fresh;
    fresh.windowSize = file.windowSize;
    fresh.measure = file.measure;
    if (!ParseSceneText(file.headerText, file.sourceText, file.model.headerFile,
                        file.model.sourceFile, fresh, error, file.windowSize,
                        file.measure)) {
        return false;
    }
    file.model = std::move(fresh.model);
    file.sourceLines = std::move(fresh.sourceLines);
    file.headerLines = std::move(fresh.headerLines);
    return true;
}

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

} // namespace detail

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
    const std::string lit = detail::EscapeLiteral(newDisplay, escOk);
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
    if (!detail::SpliceRange(file.sourceLines, l1, c1, l2, c2, "\"" + lit + "\"", error)) {
        return false;
    }
    return detail::Reparse(file, error);
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
        return detail::Reparse(file, error);
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
    const size_t closeOff = detail::MatchBracket(text, openOff, ')');
    if (closeOff == std::string::npos) {
        error = "unbalanced " + wrapper + " call";
        return false;
    }
    const auto [l1, c1] = StmtOffsetToPos(file, stmt, openOff + 1);
    const auto [l2, c2] = StmtOffsetToPos(file, stmt, closeOff);
    if (!detail::SpliceRange(file.sourceLines, l1, c1, l2, c2, args, error)) {
        return false;
    }
    return detail::Reparse(file, error);
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
        return detail::Reparse(file, error);
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
    if (!detail::SpliceRange(file.sourceLines, l1, c1, l2, c2, std::to_string(id), error)) {
        return false;
    }
    return detail::Reparse(file, error);
}

bool AddObject(SceneFile& file, const std::string& typeName, const std::string& varName,
               const std::string& displayName, uint64_t id, bool withPosition,
               const std::string& x, const std::string& y, const std::string& z,
               AddResult& result, std::string& error) {
    if (typeName != "Square" && typeName != "Cube" && typeName != "Object") {
        error = "type must be Square|Cube|Object";
        return false;
    }
    if (!detail::IsIdentifier(varName)) {
        error = "invalid variable name '" + varName + "'";
        return false;
    }
    if (FindObject(file.model, varName)) {
        error = "object '" + varName + "' already exists";
        return false;
    }
    bool escOk = false;
    const std::string lit = detail::EscapeLiteral(displayName, escOk);
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
    return detail::Reparse(file, error);
}

bool SetParentObject(SceneFile& file, const std::string& var,
                     const std::string& parentVar, std::string& error) {
    const ObjectModel* obj = FindObject(file.model, var);
    if (!obj) {
        error = "unknown object '" + var + "'";
        return false;
    }
    if (parentVar == var) {
        error = "object cannot parent to itself";
        return false;
    }
    if (!parentVar.empty() && !FindObject(file.model, parentVar)) {
        error = "unknown parent object '" + parentVar + "'";
        return false;
    }
    if (parentVar.empty()) {
        // Unparent: delete existing setParent statements.
        std::vector<CppEdit> dels;
        for (size_t idx : obj->statementIndexes) {
            const Statement& stmt = file.model.ctorStatements[idx];
            if (stmt.kind == Statement::Kind::SetParent) {
                dels.push_back(CppEdit{stmt.beginLine, stmt.endLine, ""});
            }
        }
        if (!ApplyEdits(file.sourceLines, dels, error)) {
            return false;
        }
        return detail::Reparse(file, error);
    }
    const size_t si = LastStmt(file.model, *obj, Statement::Kind::SetParent);
    if (si == static_cast<size_t>(-1)) {
        if (!obj->hasConstruct) {
            error = "no construction site for '" + var + "'";
            return false;
        }
        const Statement& cs = file.model.ctorStatements[obj->constructIndex];
        const CppEdit ins{cs.endLine + 1, cs.endLine,
                          file.model.indent + var + "->setParent(" + parentVar + ");"};
        if (!ApplyEdits(file.sourceLines, {ins}, error)) {
            return false;
        }
        return detail::Reparse(file, error);
    }
    const Statement& stmt = file.model.ctorStatements[si];
    const std::string text = StmtText(file, stmt);
    static const std::regex parentCall("setParent\\s*\\(");
    std::smatch m;
    if (!std::regex_search(text, m, parentCall)) {
        error = "cannot locate setParent call";
        return false;
    }
    const size_t openOff = text.find('(', static_cast<size_t>(m.position(0)));
    if (openOff == std::string::npos) {
        error = "cannot locate setParent call";
        return false;
    }
    const size_t closeOff = detail::MatchBracket(text, openOff, ')');
    if (closeOff == std::string::npos) {
        error = "unbalanced setParent call";
        return false;
    }
    const auto [l1, c1] = StmtOffsetToPos(file, stmt, openOff + 1);
    const auto [l2, c2] = StmtOffsetToPos(file, stmt, closeOff);
    if (!detail::SpliceRange(file.sourceLines, l1, c1, l2, c2, parentVar, error)) {
        return false;
    }
    return detail::Reparse(file, error);
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
    return detail::Reparse(file, error);
}

std::vector<size_t> FindReferences(const std::vector<std::string>& lines,
                                   const std::string& var,
                                   const std::vector<CppEdit>& excluded) {
    std::vector<size_t> refs;
    if (!detail::IsIdentifier(var)) {
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
    return detail::JoinLines(file.sourceLines, file.sourceEol, file.sourceTrailingNewline);
}
std::string SerializeHeader(const SceneFile& file) {
    return detail::JoinLines(file.headerLines, file.headerEol, file.headerTrailingNewline);
}
} // namespace scenecpp
} // namespace studio
} // namespace icg

