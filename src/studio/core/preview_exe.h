// Incogine Studio — preview executable resolution + dev-build binding
// (Qt-free, stdlib only).
// Part of Incogine by leafstudiosDot (MPL-2.0). See LICENSE.
//
// Two layers keep Studio off foreign executables:
//
//   1. Provenance: LocatePreviewExe() only accepts executables inside a
//      CMake build tree whose CMakeCacheHOME points at this project root
//      (plus caller-provided hint dirs, e.g. next to Studio itself).
//   2. Integrity: VerifyPreviewExe() requires the exe stem to match the
//      project <name> and its SHA-256 to match the CMake-generated
//      `<stem>.sha256` sidecar sitting next to it. A released (or any
//      swapped-in) binary fails the sidecar check.
//
// A launch that fails either layer must be refused with an error.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace icg {
namespace studio {
namespace preview {

struct PreviewExeInfo {
    std::string exePath;   // canonical executable path
    std::string buildDir;  // CMake binary dir (empty for hint-dir hits)
    std::string config;    // e.g. Debug (multi-config) or CMAKE_BUILD_TYPE
    std::string generator; // CMAKE_GENERATOR string, may be empty
};

// Finds the project's development game executable: scans <root>/build*
// for CMakeCache-verified build trees (VS 2026, Xcode, Ninja/Make), then
// falls back to hintDirs (checked for existence only). Picks the newest
// existing binary. exeBaseName is the <name> from src/project.xml.
bool LocatePreviewExe(const std::string& projectRoot, const std::string& exeBaseName,
                      const std::vector<std::string>& hintDirs, PreviewExeInfo& out,
                      std::string& error);

struct PreviewBinding {
    std::string exePath;   // canonical path that was verified
    std::string buildDir;  // ancestor holding a matching CMakeCache, if any
    std::string sha256;    // verified hex digest
};

// Verifies one concrete executable: exists, stem matches exeBaseName, and
// its SHA-256 matches the `<stem>.sha256` sidecar beside it.
bool VerifyPreviewExe(const std::string& projectRoot, const std::string& exeBaseName,
                      const std::string& exePath, PreviewBinding& out,
                      std::string& error);

} // namespace preview
} // namespace studio
} // namespace icg
