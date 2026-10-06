// Incogine Studio - scene discovery (Qt-free, read-only).
// Part of Incogine by leafstudiosDot (MPL-2.0). See LICENSE.
//
// Scenes are currently imperative C++ (`class X : public Scene` under
// `src/scenes/`). There is no serializable object list to round-trip, so
// Phase 1 only discovers scene classes and reports their source locations.
// Visual write-back waits for a dedicated scene format (see docs/studio.md).
#pragma once

#include <string>
#include <vector>

namespace icg {
namespace studio {

struct SceneInfo {
    std::string className;      // e.g. "MainScene"
    std::string declaredName;   // e.g. Scene("Main Scene") when found
    std::string headerFile;     // absolute path to the .h
    std::string sourceFile;     // absolute path to the .cpp (may be empty)
    bool inPurokoLib = true;    // false for splash/ + settings/ (linked into exe)
};

class SceneDiscovery {
public:
    // Scans `scenesDir` recursively for `class X : public Scene` headers.
    static std::vector<SceneInfo> Scan(const std::string& scenesDir);
};

} // namespace studio
} // namespace icg

