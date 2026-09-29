// Incogine Studio — shared XML helpers (Qt-free, stdlib).
// Part of Incogine by leafstudiosDot (MPL-2.0). See LICENSE.
//
// Trimming, escaping, tag lookup, and file loading shared by the
// project/credits XML models and scene discovery, so entity handling
// stays identical everywhere.
#pragma once

#include <string>
#include <vector>

namespace icg {
namespace studio {
namespace xml {

std::string TrimXml(const std::string& s);
// Escapes & < > (text-node safe).
std::string EscapeXml(const std::string& s);
// Decodes &amp; &lt; &gt; &quot; &apos; (absent entities pass through).
std::string UnescapeXml(const std::string& s);
// Attribute-tolerant <tag ...>...</tag> lookup; false when absent/malformed.
bool FindTagRange(const std::string& xml, const std::string& tag, size_t from,
                  size_t& openPos, size_t& innerBegin, size_t& innerEnd,
                  size_t& closeEnd);
// Inner text of the first <tag...>...</tag> occurrence.
bool FindSimpleTag(const std::string& xml, const std::string& tag, std::string& inner);
// Trimmed + unescaped inner texts of every <tag...>...</tag> occurrence.
std::vector<std::string> FindAllSimple(const std::string& xml, const std::string& tag);
// Whole file as text; false + error when unreadable.
bool ReadTextFile(const std::string& path, std::string& out, std::string& error);

} // namespace xml
} // namespace studio
} // namespace icg
