// Incogine Studio — src/credits.xml model (Qt-free).
// Part of Incogine by leafstudiosDot (MPL-2.0). See LICENSE.
//
// Document-order block model: <project> children become an ordered list of
// blocks (header/people/subheader/grid/unknown) so saves preserve layout.
// Unknown elements are kept verbatim and written back unchanged.
#pragma once

#include <string>
#include <vector>

namespace icg {
namespace studio {

struct CreditPerson {
    std::string name;
    std::string role;
};

struct CreditBlock {
    enum class Type { Header, Person, Subheader, Grid, Unknown };
    Type type = Type::Unknown;
    // Header/Subheader: text. Grid: names. Unknown: raw XML chunk.
    std::string text;
    CreditPerson person;
    std::vector<std::string> names;
    std::string raw;
};

struct Contributor {
    std::string name;
    std::string role;
    std::vector<std::string> services;
    // Verbatim inner XML of any unrecognized <contributor> children.
    std::string unknownInner;
};

struct CreditsXml {
    std::vector<CreditBlock> projectBlocks;
    std::vector<Contributor> contributors;
    // Verbatim inner XML of unrecognized <incogine> children outside
    // <contributors>, plus unrecognized top-level sections.
    std::string unknownIncogineInner;
    std::string unknownTopLevel;

    static bool ParseFile(const std::string& path, CreditsXml& out, std::string& error);
    static bool ParseText(const std::string& text, CreditsXml& out, std::string& error);
    std::string Serialize() const;
    bool SaveFile(const std::string& path, std::string& error) const;

    // Convenience helpers used by the Credits form.
    void AddPerson(const std::string& name, const std::string& role);
    bool RemovePerson(size_t personIndex);
    void AddContributor(const std::string& name, const std::string& role);
};

} // namespace studio
} // namespace icg
