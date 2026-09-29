// Incogine Studio — CreditsXml implementation (Qt-free, stdlib only).
// Part of Incogine by leafstudiosDot (MPL-2.0). See LICENSE.
#include "credits_xml.h"

#include <fstream>

#include "xml_util.h"

namespace icg {
namespace studio {
namespace {

using xml::EscapeXml;
using xml::FindAllSimple;
using xml::FindSimpleTag;
using xml::FindTagRange;
using xml::TrimXml;
using xml::UnescapeXml;

} // namespace

bool CreditsXml::ParseText(const std::string& text, CreditsXml& out, std::string& error) {
    CreditsXml tmp;
    size_t o, b, e, c;
    // <project> section (ordered blocks).
    if (FindTagRange(text, "project", 0, o, b, e, c)) {
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
                block.text = TrimXml(UnescapeXml(inner));
            } else if (tag == "listname") {
                block.type = CreditBlock::Type::Person;
                std::string n, r;
                FindSimpleTag(inner, "name", n);
                FindSimpleTag(inner, "role", r);
                block.person.name = TrimXml(UnescapeXml(n));
                block.person.role = TrimXml(UnescapeXml(r));
            } else if (tag == "subheader") {
                block.type = CreditBlock::Type::Subheader;
                block.text = TrimXml(UnescapeXml(inner));
            } else if (tag == "gridlist") {
                block.type = CreditBlock::Type::Grid;
                block.names = FindAllSimple(inner, "name");
            } else {
                block.type = CreditBlock::Type::Unknown;
                block.raw = raw;
            }
            tmp.projectBlocks.push_back(block);
            pos = ce + close.size();
        }
    }

    // <incogine><contributors><contributor>... entries.
    if (FindTagRange(text, "incogine", 0, o, b, e, c)) {
        const std::string incBody = text.substr(b, e - b);
        size_t co, cb, ce2, cc;
        if (FindTagRange(incBody, "contributors", 0, co, cb, ce2, cc)) {
            const std::string list = incBody.substr(cb, ce2 - cb);
            size_t from = 0;
            size_t po, pb, pe, pc;
            while (FindTagRange(list, "contributor", from, po, pb, pe, pc)) {
                const std::string inner = list.substr(pb, pe - pb);
                Contributor contrib;
                std::string n, r;
                if (FindSimpleTag(inner, "name", n)) {
                    // <service> also contains <name> children; prefer the
                    // first <name> that appears before any <service>.
                    const size_t svcPos = inner.find("<service");
                    const size_t namePos = inner.find("<name");
                    if (svcPos == std::string::npos || namePos < svcPos) {
                        contrib.name = TrimXml(UnescapeXml(n));
                    }
                }
                if (FindSimpleTag(inner, "role", r)) {
                    contrib.role = TrimXml(UnescapeXml(r));
                }
                size_t so, sb, se, sc;
                if (FindTagRange(inner, "service", 0, so, sb, se, sc)) {
                    contrib.services = FindAllSimple(inner.substr(sb, se - sb), "name");
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
    std::string text;
    if (!xml::ReadTextFile(path, text, error)) {
        return false;
    }
    return ParseText(text, out, error);
}

std::string CreditsXml::Serialize() const {
    std::string xml = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n<credits>\n    <project>\n";
    for (const CreditBlock& bl : projectBlocks) {
        switch (bl.type) {
            case CreditBlock::Type::Header:
                xml += "        <header>" + EscapeXml(bl.text) + "</header>\n";
                break;
            case CreditBlock::Type::Person:
                xml += "        <listname>\n";
                xml += "            <name>" + EscapeXml(bl.person.name) + "</name>\n";
                xml += "            <role>" + EscapeXml(bl.person.role) + "</role>\n";
                xml += "        </listname>\n";
                break;
            case CreditBlock::Type::Subheader:
                xml += "        <subheader>" + EscapeXml(bl.text) + "</subheader>\n";
                break;
            case CreditBlock::Type::Grid:
                xml += "        <gridlist>\n";
                for (const std::string& n : bl.names) {
                    xml += "            <name>" + EscapeXml(n) + "</name>\n";
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
        xml += "                <name>" + EscapeXml(contrib.name) + "</name>\n";
        xml += "                <role>" + EscapeXml(contrib.role) + "</role>\n";
        if (!contrib.services.empty() || !contrib.unknownInner.empty()) {
            xml += "                <service>\n";
            for (const std::string& s : contrib.services) {
                xml += "                    <name>" + EscapeXml(s) + "</name>\n";
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
