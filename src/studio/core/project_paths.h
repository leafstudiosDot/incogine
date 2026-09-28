// Incogine Studio — project root / path resolution (Qt-free).
// Part of Incogine by leafstudiosDot (MPL-2.0). See LICENSE.
#pragma once

#include <filesystem>
#include <string>

namespace icg {
namespace studio {

// NOTE on layout: the Studio proposal described root-level
// `credits.xml`, `projects.xml`, `assets/` and `studio/` paths. The actual
// repository keeps project files under `src/` (singular `project.xml`):
//   src/project.xml, src/credits.xml, src/assets/, src/scenes/,
//   src/scripts/, src/project/, src/studio/.
// Studio therefore resolves everything relative to the repository root
// (the directory that contains `src/project.xml`).
class ProjectPaths {
public:
    // Walks up from `startDir` until a directory containing
    // `src/project.xml` is found. Returns empty path when not found.
    static std::filesystem::path FindRoot(const std::filesystem::path& startDir);

    explicit ProjectPaths(std::filesystem::path root) : root_(std::move(root)) {}

    const std::filesystem::path& root() const { return root_; }
    std::filesystem::path projectXml() const { return root_ / "src" / "project.xml"; }
    std::filesystem::path creditsXml() const { return root_ / "src" / "credits.xml"; }
    std::filesystem::path assetsDir() const { return root_ / "src" / "assets"; }
    std::filesystem::path scenesDir() const { return root_ / "src" / "scenes"; }
    std::filesystem::path scriptsDir() const { return root_ / "src" / "scripts"; }
    std::filesystem::path projectSrcDir() const { return root_ / "src" / "project"; }
    std::filesystem::path studioSrcDir() const { return root_ / "src" / "studio"; }
    std::filesystem::path engineSrcDir() const { return root_ / "src" / "core"; }

    bool valid() const;

private:
    std::filesystem::path root_;
};

} // namespace studio
} // namespace icg
