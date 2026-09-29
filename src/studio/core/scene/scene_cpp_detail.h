// Incogine Studio — C++ scene parser shared internals (Qt-free, stdlib).
// Part of Incogine by leafstudiosDot (MPL-2.0). See LICENSE.
//
// Internal declarations shared by the scene_cpp_* translation units. The
// public round-trip API stays in scene_cpp.h and is unchanged.
#pragma once

#include <string>
#include <vector>

#include "scene_cpp.h"

namespace icg {
namespace studio {
namespace scenecpp {
namespace detail {

// ---------- text primitives (scene_cpp_text.cpp) ----------

std::string DetectEol(const std::string& text);
std::vector<std::string> SplitLines(const std::string& text, const std::string& eol);
std::string JoinLines(const std::vector<std::string>& lines, const std::string& eol,
                      bool trailingNewline);
std::string Trim(const std::string& s);
std::string LeadingWhitespace(const std::string& s);
std::string CollapseWhitespace(const std::string& s);
// Splits on a delimiter at paren/bracket/brace depth 0.
std::vector<std::string> SplitTopLevel(const std::string& s, char delim);
// Comment-stripped copy for pattern matching (strings preserved).
std::string StripComments(const std::string& s);
bool ParseDouble(const std::string& s, double& out);
VecExpr ParseVec(const std::string& inner);
std::string EscapeLiteral(const std::string& s, bool& ok);
bool IsIdentifier(const std::string& s);

// ---------- offset <-> line mapping ----------

std::vector<size_t> LineStarts(const std::string& text);
// 1-based line containing offset off.
size_t OffsetToLine(const std::vector<size_t>& starts, size_t off);
// Finds `close` matching the opener at `open`; npos when unbalanced.
size_t MatchBracket(const std::string& text, size_t open, char close);

// ---------- cross-TU passes ----------

// Header `Font` declarations + file-wide call sites (scene_cpp_fonts.cpp).
void ParseFonts(const std::string& headerText, const std::string& sourceText,
                SceneModel& model);
// Fold line vectors back into texts and re-parse (scene_cpp_ops.cpp).
bool Reparse(SceneFile& file, std::string& error);
// Replaces [bLine,bCol)..[eLine,eCol) (1-based lines, 0-based cols).
bool SpliceRange(std::vector<std::string>& lines, size_t bLine, size_t bCol,
                 size_t eLine, size_t eCol, const std::string& text,
                 std::string& error);

} // namespace detail
} // namespace scenecpp
} // namespace studio
} // namespace icg
