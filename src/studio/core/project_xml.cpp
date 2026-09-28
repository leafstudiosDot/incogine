// Incogine Studio — ProjectXml implementation (Qt-free, stdlib only).
// Part of Incogine by leafstudiosDot (MPL-2.0). See LICENSE.
#include "project_xml.h"

#include <fstream>
#include <sstream>

namespace icg {
namespace studio {
namespace {

std::string Trim(const std::string& s) {
    const char* ws = " \t\r\n";
    const size_t b = s.find_first_not_of(ws);
    if (b == std::string::npos) {
        return {};
    }
    const size_t e = s.find_last_not_of(ws);
    return s.substr(b, e - b + 1);
}

// Returns the inner text of the first <tag>...</tag> occurrence.
bool FindTag(const std::string& xml, const std::string& tag, std::string& inner) {
    const std::string open = "<" + tag + ">";
    const std::string close = "</" + tag + ">";
    const size_t b = xml.find(open);
    if (b == std::string::npos) {
        return false;
    }
    const size_t e = xml.find(close, b + open.size());
    if (e == std::string::npos) {
        return false;
    }
    inner = xml.substr(b + open.size(), e - b - open.size());
    return true;
}

std::string Escape(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        switch (c) {
            case '&': out += "&amp;"; break;
            case '<': out += "&lt;"; break;
            case '>': out += "&gt;"; break;
            default: out += c; break;
        }
    }
    return out;
}

std::string Unescape(const std::string& s) {
    std::string out = s;
    const char* pairs[][2] = {{"&amp;", "&"}, {"&lt;", "<"}, {"&gt;", ">"}, {"&quot;", "\""}, {"&apos;", "'"}};
    for (const auto& p : pairs) {
        size_t pos = 0;
        while ((pos = out.find(p[0], pos)) != std::string::npos) {
            out.replace(pos, std::string(p[0]).size(), p[1]);
            pos += std::string(p[1]).size();
        }
    }
    return out;
}

} // namespace

bool ProjectXml::ParseText(const std::string& text, ProjectXml& out, std::string& error) {
    ProjectXml tmp;
    std::string v;
    if (FindTag(text, "name", v)) {
        tmp.name = Trim(Unescape(v));
    }
    if (FindTag(text, "window_name", v)) {
        tmp.windowName = Trim(Unescape(v));
    }
    if (FindTag(text, "identifier", v)) {
        tmp.identifier = Trim(Unescape(v));
    }
    if (FindTag(text, "version", v)) {
        tmp.version = Trim(Unescape(v));
    }
    if (FindTag(text, "description", v)) {
        tmp.description = Trim(Unescape(v));
    }
    if (FindTag(text, "incogine_version", v)) {
        tmp.incogineVersion = Trim(Unescape(v));
    }
    if (FindTag(text, "author", v)) {
        tmp.author = Trim(Unescape(v));
    }
    if (FindTag(text, "copyright", v)) {
        tmp.copyright = Trim(Unescape(v));
    }
    if (FindTag(text, "settings", v)) {
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
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        error = "cannot open " + path;
        return false;
    }
    std::ostringstream ss;
    ss << in.rdbuf();
    return ParseText(ss.str(), out, error);
}

std::string ProjectXml::Serialize() const {
    std::string xml = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n<project>\n";
    xml += "    <name>" + Escape(name) + "</name>\n";
    xml += "    <window_name>" + Escape(windowName) + "</window_name>\n";
    xml += "    <identifier>" + Escape(identifier) + "</identifier>\n";
    xml += "    <version>" + Escape(version) + "</version>\n";
    xml += "    <description>" + Escape(description) + "</description>\n";
    xml += "    <incogine_version>" + Escape(incogineVersion) + "</incogine_version>\n";
    xml += "    <author>" + Escape(author) + "</author>\n";
    xml += "    <copyright>" + Escape(copyright) + "</copyright>\n";
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
