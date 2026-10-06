// Incogine Studio - ProjectXml implementation (Qt-free, stdlib only).
// Part of Incogine by leafstudiosDot (MPL-2.0). See LICENSE.
#include "project_xml.h"

#include <fstream>

#include "xml_util.h"

namespace icg {
namespace studio {

bool ProjectXml::ParseText(const std::string& text, ProjectXml& out, std::string& error) {
    ProjectXml tmp;
    std::string v;
    if (xml::FindSimpleTag(text, "name", v)) {
        tmp.name = xml::TrimXml(xml::UnescapeXml(v));
    }
    if (xml::FindSimpleTag(text, "window_name", v)) {
        tmp.windowName = xml::TrimXml(xml::UnescapeXml(v));
    }
    if (xml::FindSimpleTag(text, "identifier", v)) {
        tmp.identifier = xml::TrimXml(xml::UnescapeXml(v));
    }
    if (xml::FindSimpleTag(text, "version", v)) {
        tmp.version = xml::TrimXml(xml::UnescapeXml(v));
    }
    if (xml::FindSimpleTag(text, "description", v)) {
        tmp.description = xml::TrimXml(xml::UnescapeXml(v));
    }
    if (xml::FindSimpleTag(text, "incogine_version", v)) {
        tmp.incogineVersion = xml::TrimXml(xml::UnescapeXml(v));
    }
    if (xml::FindSimpleTag(text, "author", v)) {
        tmp.author = xml::TrimXml(xml::UnescapeXml(v));
    }
    if (xml::FindSimpleTag(text, "copyright", v)) {
        tmp.copyright = xml::TrimXml(xml::UnescapeXml(v));
    }
    if (xml::FindSimpleTag(text, "settings", v)) {
        tmp.settingsInner = v; // preserved verbatim (may be whitespace-only)
    }
    if (tmp.name.empty()) {
        error = "project.xml: missing <name>";
        return false;
    }
    out = tmp;
    return true;
}

bool ProjectXml::ParseFile(const std::string& path, ProjectXml& out, std::string& error) {
    std::string text;
    if (!xml::ReadTextFile(path, text, error)) {
        return false;
    }
    return ParseText(text, out, error);
}

std::string ProjectXml::Serialize() const {
    std::string xml = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n<project>\n";
    xml += "    <name>" + xml::EscapeXml(name) + "</name>\n";
    xml += "    <window_name>" + xml::EscapeXml(windowName) + "</window_name>\n";
    xml += "    <identifier>" + xml::EscapeXml(identifier) + "</identifier>\n";
    xml += "    <version>" + xml::EscapeXml(version) + "</version>\n";
    xml += "    <description>" + xml::EscapeXml(description) + "</description>\n";
    xml += "    <incogine_version>" + xml::EscapeXml(incogineVersion) + "</incogine_version>\n";
    xml += "    <author>" + xml::EscapeXml(author) + "</author>\n";
    xml += "    <copyright>" + xml::EscapeXml(copyright) + "</copyright>\n";
    xml += "    <settings>";
    xml += settingsInner;
    if (settingsInner.empty() || settingsInner.find('\n') == std::string::npos) {
        xml += "\n    ";
    }
    xml += "</settings>\n</project>\n";
    return xml;
}

bool ProjectXml::SaveFile(const std::string& path, std::string& error) const {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) {
        error = "cannot write " + path;
        return false;
    }
    out << Serialize();
    return true;
}

} // namespace studio
} // namespace icg

