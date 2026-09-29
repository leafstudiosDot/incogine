// Incogine Studio — SceneDiscovery implementation (Qt-free).
// Part of Incogine by leafstudiosDot (MPL-2.0). See LICENSE.
#include "scene_discovery.h"

#include <filesystem>
#include <regex>

#include "core/xml/xml_util.h"

namespace icg {
namespace studio {

std::vector<SceneInfo> SceneDiscovery::Scan(const std::string& scenesDir) {
    std::vector<SceneInfo> out;
    std::error_code ec;
    if (scenesDir.empty() || !std::filesystem::exists(scenesDir, ec)) {
        return out;
    }
    const std::regex classRe(R"(class\s+(\w+)\s*:\s*public\s+Scene\b)");
    const std::regex nameRe(R"(Scene\s*\(\s*\"([^\"]*)\"\s*\))");

    for (const auto& entry : std::filesystem::recursive_directory_iterator(scenesDir, ec)) {
        if (ec) {
            break;
        }
        if (!entry.is_regular_file()) {
            continue;
        }
        if (entry.path().extension() != ".h") {
            continue;
        }
        std::string headerText;
        std::string ignored;
        if (!xml::ReadTextFile(entry.path().string(), headerText, ignored)) {
            continue;
        }
        std::smatch m;
        if (!std::regex_search(headerText, m, classRe)) {
            continue;
        }
        SceneInfo info;
        info.className = m[1].str();
        info.headerFile = entry.path().string();

        std::filesystem::path cpp = entry.path();
        cpp.replace_extension(".cpp");
        std::string haystack = headerText;
        if (std::filesystem::exists(cpp)) {
            info.sourceFile = cpp.string();
            std::string cppText;
            if (xml::ReadTextFile(cpp.string(), cppText, ignored)) {
                haystack += "\n" + cppText;
            }
        }
        if (std::regex_search(haystack, m, nameRe)) {
            info.declaredName = m[1].str();
        }
        const std::string posix = entry.path().generic_string();
        info.inPurokoLib = posix.find("/splash/") == std::string::npos &&
                           posix.find("/settings/") == std::string::npos;
        out.push_back(info);
    }
    return out;
}

} // namespace studio
} // namespace icg
