// Incogine Studio — C++ scene round-trip parser (Qt-free, stdlib only).
// Part of Incogine by leafstudiosDot (MPL-2.0). See LICENSE.
//
// Scenes stay imperative C++; there is no separate scene format. This
// parser recognizes a convention subset inside a scene class constructor
// and rewrites only those spans, preserving everything else byte-for-byte:
//
//   Recognized (constructor body only; Update()/Render()/rest verbatim):
//     `v = new Square();` / `v = new Cube();`
//     `v = new Object("Name", Position(..), Scale(..), Rotation(..));`
//     `v->setName("Literal");` / `v->setId(123);`
//     `v->setPosition(Position(..));` (+ Scale/Rotation/Color pairs)
//     `v->addComponent(...);` / `v->setParent(...);` (recorded, raw args)
//   Everything else (font setup, if/for blocks, Update()/Render() code,
//   non-Object allocations like `new PauseMenu()`) is Unknown and never
//   rewritten. Multi-line statements are supported; edits splice exact
//   line/column spans so diffs stay minimal.
//
// Limits (v1, documented so review can extend them):
// - One construction per variable (first wins); member-vs-local is not
//   distinguished; `this->x` targets normalize to `x`.
// - setName only when the argument is a plain string literal.
// - Transform edits target the LAST matching constructor statement
//   (the runtime-effective one); per-frame Update() assignments are out
//   of scope and left verbatim.
// - AddObject appends constructor code and, when the header has exactly
//   one unambiguous `private:` label, the member declaration; otherwise
//   the declaration is left to the developer (reported).
// - RemoveObject deletes constructor statements and reports every other
//   mention (e.g. `delete v;`) for hand cleanup instead of guessing.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace icg {
namespace studio {
namespace scenecpp {

// A comma-separated argument list; doubles filled when every part parses.
struct VecExpr {
    std::vector<std::string> parts;
    std::vector<double> values;
    bool numeric = false;
};

struct Statement {
    enum class Kind {
        Construct,
        SetName,
        SetId,
        SetPosition,
        SetScale,
        SetRotation,
        SetColor,
        AddComponent,
        SetParent,
        Unknown
    };
    Kind kind = Kind::Unknown;
    std::string target;    // normalized variable, empty for Unknown
    std::string typeName;  // Construct only (Square/Cube/Object)
    std::string displayName; // Object() literal / setName literal
    uint64_t id = 0;
    bool hasId = false;
    VecExpr vec;           // transform/color argument lists (call statements)
    VecExpr position, scale, rotation; // Construct(Object) carried values
    bool hasPosition = false, hasScale = false, hasRotation = false;
    std::string rawArg;    // AddComponent/SetParent raw argument text
    std::string parentVar; // SetParent with a bare identifier, else empty
    size_t beginLine = 0;  // 1-based, into the source lines
    size_t endLine = 0;
};

struct ObjectModel {
    std::string varName;
    std::string typeName;
    std::string displayName;
    uint64_t id = 0;
    bool hasId = false;
    VecExpr position, scale, rotation, color;
    bool hasPosition = false, hasScale = false, hasRotation = false, hasColor = false;
    std::string parentVar;
    bool hasParent = false;
    std::vector<size_t> statementIndexes; // into SceneModel::ctorStatements
    size_t constructIndex = 0;
    bool hasConstruct = false;
};

struct SceneModel {
    std::string className;
    std::string declaredName; // Scene("..."), may be empty
    bool isScene = false;     // class derives from Scene
    std::string headerFile;
    std::string sourceFile;
    std::vector<Statement> ctorStatements; // recognized + Unknown, in order
    std::vector<ObjectModel> objects;
    size_t ctorBodyBeginLine = 0; // line of '{'
    size_t ctorBodyEndLine = 0;   // line of matching '}'
    std::string indent;           // detected body indent unit
    bool headerClassFound = false;
    size_t headerPrivateLine = 0; // 1-based `private:` line, 0 unless unique
    std::string headerIndent;
};

// Editable file pair: texts are mutated by the ops below (spans stay
// minimal), and the model is re-parsed from the new text afterwards.
struct SceneFile {
    SceneModel model;
    std::string headerText;
    std::string sourceText;
    std::string headerEol = "\n";
    std::string sourceEol = "\n";
    bool headerTrailingNewline = true;
    bool sourceTrailingNewline = true;
    std::vector<std::string> sourceLines;
    std::vector<std::string> headerLines;
};

bool ParseSceneText(const std::string& headerText, const std::string& sourceText,
                    const std::string& headerName, const std::string& sourceName,
                    SceneFile& out, std::string& error);
bool ParseSceneFiles(const std::string& headerPath, const std::string& sourcePath,
                     SceneFile& out, std::string& error);

// A line-span replacement (1-based, inclusive). Empty text deletes;
// insert with endLine + 1 == beginLine.
struct CppEdit {
    size_t beginLine = 0;
    size_t endLine = 0;
    std::string text; // '\n'-separated, converted to the file EOL on apply
};
bool ApplyEdits(std::vector<std::string>& lines, const std::vector<CppEdit>& edits,
                std::string& error);

// High-level ops (mutate texts, re-parse model). All return false + error
// when the pattern is absent or unsupported (never partial edits).
bool RenameObject(SceneFile& file, const std::string& var,
                  const std::string& newDisplay, std::string& error);
bool SetTransform(SceneFile& file, const std::string& var, const std::string& kind,
                  const std::string& x, const std::string& y, const std::string& z,
                  const std::string& w, std::string& error);
bool SetObjectId(SceneFile& file, const std::string& var, uint64_t id,
                 std::string& error);
struct AddResult {
    bool headerUpdated = false;
};
bool AddObject(SceneFile& file, const std::string& typeName, const std::string& varName,
               const std::string& displayName, uint64_t id, bool withPosition,
               const std::string& x, const std::string& y, const std::string& z,
               AddResult& result, std::string& error);
struct RemoveResult {
    bool needsAttention = false;
    std::vector<size_t> otherRefs; // 1-based lines mentioning var elsewhere
};
bool RemoveObject(SceneFile& file, const std::string& var, RemoveResult& result,
                  std::string& error);

// 1-based source lines mentioning `var` (word match), excluding the given
// ranges (used to skip the object's own deleted statements).
std::vector<size_t> FindReferences(const std::vector<std::string>& lines,
                                   const std::string& var,
                                   const std::vector<CppEdit>& excluded);

std::string SerializeSource(const SceneFile& file);
std::string SerializeHeader(const SceneFile& file);

} // namespace scenecpp
} // namespace studio
} // namespace icg
