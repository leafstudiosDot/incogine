// Incogine Studio — CreditsXml implementation (Qt-free, stdlib only).
// Part of Incogine by leafstudiosDot (MPL-2.0). See LICENSE.
#include "credits_xml.h"

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

bool FindTag(const std::string& xml, const std::string& tag, size_t from, size_t& openPos, size_t& innerBegin, size_t& innerEnd, size_t& closeEnd) {
    const std::string open = "<" + tag;
    const std::string close = "</" + tag + ">";
    openPos = xml.find(open, from);
    while (openPos != std::string::npos) {
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
    return false;
}

bool SimpleTag(const std::string& xml, const std::string& tag, std::string& inner) {
    size_t o, b, e, c;
    if (!FindTag(xml, tag, 0, o, b, e, c)) {
        return false;
    }
    inner = xml.substr(b, e - b);
    return true;
}

std::string Escape(const std::string& s) {
    std::string out;
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
    const char* pairs[][2] = {{"&amp;", "&"}, {"&lt;", "<"}, {"&gt;", ">"}};
    for (const auto& p : pairs) {
        size_t pos = 0;
        while ((pos = out.find(p[0], pos)) != std::string::npos) {
            out.replace(pos, std::string(p[0]).size(), p[1]);
            pos += std::string(p[1]).size();
        }
    }
    return out;
}

std::vector<std::string> AllSimple(const std::string& xml, const std::string& tag) {
    std::vector<std::string> out;
    size_t from = 0;
    size_t o, b, e, c;
    while (FindTag(xml, tag, from, o, b, e, c)) {
        out.push_back(Trim(Unescape(xml.substr(b, e - b))));
        from = c;
    }
    return out;
}

} // namespace

bool CreditsXml::ParseText(const std::string& text, CreditsXml& out, std::string& error) {
    CreditsXml tmp;
    size_t o, b, e, c;
    // <project> section (ordered blocks).
    if (FindTag(text, "project", 0, o, b, e, c)) {
        const std::string body = text.substr(b, e - b);
        size_t pos = 0;
        while (pos < body.size()) {
            const size_t lt = body.find('<', pos);
            if (lt == std::string::npos) {
                break;
            }
            const size_t gt = body.find('>', lt);
            if (gt == std::string::npos) {
                break;
            }
            std::string tag = body.substr(lt + 1, gt - lt - 1);
            const size_t sp = tag.find_first_of(" \t\r\n/");
            if (sp != std::string::npos) {
                tag = tag.substr(0, sp);
            }
            if (tag.empty() || tag[0] == '/' || tag[0] == '?' || tag[0] == '!') {
                pos = gt + 1;
                continue;
            }
            const std::string close = "</" + tag + ">";
            const size_t ce = body.find(close, gt + 1);
            if (ce == std::string::npos) {
                pos = gt + 1;
                continue;
            }
            const std::string inner = body.substr(gt + 1, ce - gt - 1);
            const std::string raw = body.substr(lt, ce + close.size() - lt);
            CreditBlock block;
            if (tag == "header") {
                block.type = CreditBlock::Type::Header;
                block.text = Trim(Unescape(inner));
            } else if (tag == "listname") {
                block.type = CreditBlock::Type::Person;
                std::string n, r;
                SimpleTag(inner, "name", n);
                SimpleTag(inner, "role", r);
                block.person.name = Trim(Unescape(n));
                block.person.role = Trim(Unescape(r));
            } else if (tag == "subheader") {
                block.type = CreditBlock::Type::Subheader;
                block.text = Trim(Unescape(inner));
            } else if (tag == "gridlist") {
                block.type = CreditBlock::Type::Grid;
                block.names = AllSimple(inner, "name");
            } else {
                block.type = CreditBlock::Type::Unknown;
                block.raw = raw;
            }
            tmp.projectBlocks.push_back(block);
            pos = ce + close.size();
        }
    }

    // <incogine><contributors><contributor>... entries.
    if (FindTag(text, "incogine", 0, o, b, e, c)) {
        const std::string incBody = text.substr(b, e - b);
        size_t co, cb, ce2, cc;
        if (FindTag(incBody, "contributors", 0, co, cb, ce2, cc)) {
            const std::string list = incBody.substr(cb, ce2 - cb);
            size_t from = 0;
            size_t po, pb, pe, pc;
            while (FindTag(list, "contributor", from, po, pb, pe, pc)) {
                const std::string inner = list.substr(pb, pe - pb);
                Contributor contrib;
                std::string n, r;
                if (SimpleTag(inner, "name", n)) {
                    // <service> also contains <name> children; prefer the
                    // first <name> that appears before any <service>.
                    const size_t svcPos = inner.find("<service");
                    const size_t namePos = inner.find("<name");
                    if (svcPos == std::string::npos || namePos < svcPos) {
                        contrib.name = Trim(Unescape(n));
                    }
                }
                if (SimpleTag(inner, "role", r)) {
                    contrib.role = Trim(Unescape(r));
                }
                size_t so, sb, se, sc;
                if (FindTag(inner, "service", 0, so, sb, se, sc)) {
                    contrib.services = AllSimple(inner.substr(sb, se - sb), "name");
                }
                tmp.contributors.push_back(contrib);
                from = pc;
            }
        }
    }

    if (tmp.projectBlocks.empty() && tmp.contributors.empty()) {
        error = "credits.xml: no <project> entries or <contributors> found";
        return false;
    }
    out = tmp;
    return true;
}

bool CreditsXml::ParseFile(const std::string& path, CreditsXml& out, std::string& error) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        error = "cannot open " + path;
        return false;
    }
    std::ostringstream ss;
    ss << in.rdbuf();
    return ParseText(ss.str(), out, error);
}

std::string CreditsXml::Serialize() const {
    std::string xml = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n<credits>\n    <project>\n";
    for (const CreditBlock& bl : projectBlocks) {
        switch (bl.type) {
            case CreditBlock::Type::Header:
                xml += "        <header>" + Escape(bl.text) + "</header>\n";
                break;
            case CreditBlock::Type::Person:
                xml += "        <listname>\n";
                xml += "            <name>" + Escape(bl.person.name) + "</name>\n";
                xml += "            <role>" + Escape(bl.person.role) + "</role>\n";
                xml += "        </listname>\n";
                break;
            case CreditBlock::Type::Subheader:
                xml += "        <subheader>" + Escape(bl.text) + "</subheader>\n";
                break;
            case CreditBlock::Type::Grid:
                xml += "        <gridlist>\n";
                for (const std::string& n : bl.names) {
                    xml += "            <name>" + Escape(n) + "</name>\n";
                }
                xml += "        </gridlist>\n";
                break;
            case CreditBlock::Type::Unknown:
                xml += "        " + bl.raw + "\n";
                break;
        }
    }
    xml += "    </project>\n    <incogine>\n        <contributors>\n";
    for (const Contributor& contrib : contributors) {
        xml += "            <contributor>\n";
        xml += "                <name>" + Escape(contrib.name) + "</name>\n";
        xml += "                <role>" + Escape(contrib.role) + "</role>\n";
        if (!contrib.services.empty() || !contrib.unknownInner.empty()) {
            xml += "                <service>\n";
            for (const std::string& s : contrib.services) {
                xml += "                    <name>" + Escape(s) + "</name>\n";
            }
            if (!contrib.unknownInner.empty()) {
                xml += contrib.unknownInner;
            }
            xml += "                </service>\n";
        }
        xml += "            </contributor>\n";
    }
    xml += "        </contributors>\n";
    if (!unknownIncogineInner.empty()) {
        xml += unknownIncogineInner;
    }
    xml += "    </incogine>\n";
    if (!unknownTopLevel.empty()) {
        xml += unknownTopLevel;
    }
    xml += "</credits>\n";
    return xml;
}

bool CreditsXml::SaveFile(const std::string& path, std::string& error) const {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) {
        error = "cannot write " + path;
        return false;
    }
    out << Serialize();
    return true;
}

void CreditsXml::AddPerson(const std::string& name, const std::string& role) {
    CreditBlock block;
    block.type = CreditBlock::Type::Person;
    block.person.name = name;
    block.person.role = role;
    projectBlocks.push_back(block);
}

bool CreditsXml::RemovePerson(size_t personIndex) {
    size_t seen = 0;
    for (size_t i = 0; i < projectBlocks.size(); ++i) {
        if (projectBlocks[i].type == CreditBlock::Type::Person) {
            if (seen == personIndex) {
                projectBlocks.erase(projectBlocks.begin() + i);
                return true;
            }
            ++seen;
        }
    }
    return false;
}

void CreditsXml::AddContributor(const std::string& name, const std::string& role) {
    Contributor c;
    c.name = name;
    c.role = role;
    contributors.push_back(c);
}

} // namespace studio
} // namespace icg
