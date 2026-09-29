// Incogine Studio — shared XML helpers implementation (Qt-free, stdlib).
// Part of Incogine by leafstudiosDot (MPL-2.0). See LICENSE.
#include "xml_util.h"

#include <fstream>
#include <sstream>

namespace icg {
namespace studio {
namespace xml {

std::string TrimXml(const std::string& s) {
    const char* ws = " \t\r\n";
    const size_t b = s.find_first_not_of(ws);
    if (b == std::string::npos) {
        return {};
    }
    const size_t e = s.find_last_not_of(ws);
    return s.substr(b, e - b + 1);
}

std::string EscapeXml(const std::string& s) {
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

std::string UnescapeXml(const std::string& s) {
    std::string out = s;
    const char* pairs[][2] = {
        {"&amp;", "&"}, {"&lt;", "<"}, {"&gt;", ">"}, {"&quot;", "\""}, {"&apos;", "'"}};
    for (const auto& p : pairs) {
        size_t pos = 0;
        while ((pos = out.find(p[0], pos)) != std::string::npos) {
            out.replace(pos, std::string(p[0]).size(), p[1]);
            pos += std::string(p[1]).size();
        }
    }
    return out;
}

bool FindTagRange(const std::string& xml, const std::string& tag, size_t from,
                  size_t& openPos, size_t& innerBegin, size_t& innerEnd,
                  size_t& closeEnd) {
    const std::string open = "<" + tag;
    const std::string close = "</" + tag + ">";
    openPos = xml.find(open, from);
    if (openPos == std::string::npos) {
        return false;
    }
    const size_t gt = xml.find('>', openPos);
    if (gt == std::string::npos) {
        return false;
    }
    innerBegin = gt + 1;
    innerEnd = xml.find(close, innerBegin);
    if (innerEnd == std::string::npos) {
        return false;
    }
    closeEnd = innerEnd + close.size();
    return true;
}

bool FindSimpleTag(const std::string& xml, const std::string& tag, std::string& inner) {
    size_t o, b, e, c;
    if (!FindTagRange(xml, tag, 0, o, b, e, c)) {
        return false;
    }
    inner = xml.substr(b, e - b);
    return true;
}

std::vector<std::string> FindAllSimple(const std::string& xml, const std::string& tag) {
    std::vector<std::string> out;
    size_t from = 0;
    size_t o, b, e, c;
    while (FindTagRange(xml, tag, from, o, b, e, c)) {
        out.push_back(TrimXml(UnescapeXml(xml.substr(b, e - b))));
        from = c;
    }
    return out;
}

bool ReadTextFile(const std::string& path, std::string& out, std::string& error) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        error = "cannot open " + path;
        return false;
    }
    std::ostringstream ss;
    ss << in.rdbuf();
    out = ss.str();
    return true;
}

} // namespace xml
} // namespace studio
} // namespace icg
