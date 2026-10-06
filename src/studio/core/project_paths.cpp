// Incogine Studio - ProjectPaths implementation (Qt-free).
// Part of Incogine by leafstudiosDot (MPL-2.0). See LICENSE.
#include "project_paths.h"

#include <filesystem>

namespace icg {
namespace studio {

std::filesystem::path ProjectPaths::FindRoot(const std::filesystem::path& startDir) {
    std::error_code ec;
    std::filesystem::path dir = std::filesystem::absolute(startDir, ec);
    if (ec) {
        return {};
    }
    for (int i = 0; i < 16; ++i) {
        if (std::filesystem::exists(dir / "src" / "project.xml", ec)) {
            return dir;
        }
        if (dir == dir.root_path() || !dir.has_parent_path()) {
            break;
        }
        dir = dir.parent_path();
    }
    return {};
}

bool ProjectPaths::valid() const {
    std::error_code ec;
    return !root_.empty() && std::filesystem::exists(projectXml(), ec);
}

} // namespace studio
} // namespace icg

