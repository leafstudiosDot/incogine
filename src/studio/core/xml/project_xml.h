// Incogine Studio - src/project.xml model (Qt-free).
// Part of Incogine by leafstudiosDot (MPL-2.0). See LICENSE.
//
// Models the known project-identity fields and preserves the unknown
// `<settings>` payload verbatim so future settings survive a Studio save.
#pragma once

#include <string>

namespace icg {
namespace studio {

struct ProjectXml {
    std::string name;
    std::string windowName;
    std::string identifier;
    std::string version;
    std::string description;
    std::string incogineVersion;
    std::string author;
    std::string copyright;
    // Raw inner XML of <settings>...</settings>, preserved verbatim.
    std::string settingsInner;

    // Parses file content; returns false + error message on failure.
    static bool ParseFile(const std::string& path, ProjectXml& out, std::string& error);
    static bool ParseText(const std::string& text, ProjectXml& out, std::string& error);
    // Serializes back to XML (UTF-8 declaration + <project> root).
    std::string Serialize() const;
    bool SaveFile(const std::string& path, std::string& error) const;
};

} // namespace studio
} // namespace icg

