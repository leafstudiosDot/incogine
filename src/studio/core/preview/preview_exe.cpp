// Incogine Studio - preview executable resolution implementation.
// Part of Incogine by leafstudiosDot (MPL-2.0). See LICENSE.
#include "preview_exe.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <sstream>

#include "core/crypto/sha256.h"

namespace icg {
namespace studio {
namespace preview {
namespace {

namespace fs = std::filesystem;

std::string Canonical(const std::string& path) {
    std::error_code ec;
    fs::path p = fs::weakly_canonical(fs::path(path), ec);
    if (ec) {
        return path;
    }
    return p.generic_string();
}

void ToLowerInPlace(std::string& s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return std::tolower(c); });
}

bool SamePath(const std::string& a, const std::string& b) {
    std::string x = Canonical(a), y = Canonical(b);
#ifdef _WIN32
    ToLowerInPlace(x);
    ToLowerInPlace(y);
#endif
    return x == y;
}

// Minimal "KEY:VALUE=..." CMakeCache parser (enough for the keys we need).
std::string CacheValue(const std::string& cachePath, const std::string& key) {
    std::ifstream in(cachePath, std::ios::binary);
    if (!in) {
        return {};
    }
    std::string line;
    const std::string prefix = key + ":";
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (line.compare(0, prefix.size(), prefix) != 0) {
            continue;
        }
        const size_t eq = line.find('=', prefix.size());
        if (eq == std::string::npos) {
            continue;
        }
        return line.substr(eq + 1);
    }
    return {};
}

uint64_t FileMtime(const fs::path& p) {
    std::error_code ec;
    const auto t = fs::last_write_time(p, ec);
    if (ec) {
        return 0;
    }
    return static_cast<uint64_t>(t.time_since_epoch().count());
}

void ConsiderCandidate(const fs::path& exe, const std::string& buildDir,
                       const std::string& config, const std::string& generator,
                       uint64_t& bestMtime, PreviewExeInfo& best) {
    std::error_code ec;
    if (!fs::is_regular_file(exe, ec)) {
        return;
    }
    const uint64_t mtime = FileMtime(exe);
    if (mtime > bestMtime) {
        bestMtime = mtime;
        best.exePath = Canonical(exe.string());
        best.buildDir = buildDir;
        best.config = config;
        best.generator = generator;
    }
}

bool StartsWith(const std::string& s, const std::string& prefix) {
    return s.compare(0, prefix.size(), prefix) == 0;
}

} // namespace

bool LocatePreviewExe(const std::string& projectRoot, const std::string& exeBaseName,
                      const std::vector<std::string>& hintDirs, PreviewExeInfo& out,
                      std::string& error) {
    if (projectRoot.empty() || exeBaseName.empty()) {
        error = "project root and executable name are required";
        return false;
    }
#ifdef _WIN32
    const std::string exeFile = exeBaseName + ".exe";
#else
    const std::string exeFile = exeBaseName;
#endif
    PreviewExeInfo best;
    uint64_t bestMtime = 0;

    // 1. CMake build trees: <root>/build* with a matching CMakeCache.
    std::error_code ec;
    fs::directory_iterator it(projectRoot, ec);
    if (!ec) {
        for (const auto& entry : it) {
            if (!entry.is_directory(ec)) {
                continue;
            }
            const std::string dirName = entry.path().filename().string();
            if (dirName != "build" && !StartsWith(dirName, "build-") &&
                !StartsWith(dirName, "build_")) {
                continue;
            }
            const std::string cache =
                (entry.path() / "CMakeCache.txt").string();
            if (!fs::is_regular_file(cache, ec)) {
                continue;
            }
            if (!SamePath(CacheValue(cache, "CMAKE_HOME_DIRECTORY"), projectRoot)) {
                continue; // another project's build tree
            }
            const std::string generator = CacheValue(cache, "CMAKE_GENERATOR");
            const std::string dir = Canonical(entry.path().string());
            if (StartsWith(generator, "Visual Studio") || StartsWith(generator, "Xcode")) {
                // Multi-config: per-config subdirs; macOS bundles nest deeper.
                for (const char* cfg : {"Debug", "Release", "RelWithDebInfo",
                                        "MinSizeRel"}) {
                    ConsiderCandidate(entry.path() / cfg / exeFile, dir, cfg,
                                      generator, bestMtime, best);
                    ConsiderCandidate(entry.path() / cfg / (exeBaseName + ".app") /
                                          "Contents" / "MacOS" / exeBaseName,
                                      dir, cfg, generator, bestMtime, best);
                }
            } else {
                // Single-config (Ninja/Make): binary at the tree root.
                const std::string buildType = CacheValue(cache, "CMAKE_BUILD_TYPE");
                ConsiderCandidate(entry.path() / exeFile, dir, buildType, generator,
                                  bestMtime, best);
            }
        }
    }

    // 2. Caller hint dirs (e.g. next to Studio itself): existence only.
    for (const std::string& hint : hintDirs) {
        if (hint.empty()) {
            continue;
        }
        ConsiderCandidate(fs::path(hint) / exeFile, std::string(), std::string(),
                          std::string(), bestMtime, best);
    }

    if (best.exePath.empty()) {
        error = "no development " + exeFile +
                " found (build the project with CMake first)";
        return false;
    }
    out = best;
    return true;
}

bool VerifyPreviewExe(const std::string& projectRoot, const std::string& exeBaseName,
                      const std::string& exePath, PreviewBinding& out,
                      std::string& error) {
    std::error_code ec;
    if (!fs::is_regular_file(exePath, ec)) {
        error = "executable not found: " + exePath;
        return false;
    }
    const fs::path exe(exePath);
    std::string stem = exe.stem().string(); // strips .exe on Windows
#ifdef _WIN32
    std::string lowered = stem;
    ToLowerInPlace(lowered);
    std::string wanted = exeBaseName;
    ToLowerInPlace(wanted);
#else
    const std::string& lowered = stem;
    const std::string& wanted = exeBaseName;
#endif
    if (lowered != wanted) {
        error = "executable '" + stem + "' does not match this project (" +
                exeBaseName + ")";
        return false;
    }

    // Sidecar written by CMake POST_BUILD next to the binary.
    const fs::path sidecar = exe.parent_path() / (stem + ".sha256");
    std::ifstream in(sidecar, std::ios::binary);
    if (!in) {
        error = "missing checksum sidecar " + sidecar.string() +
                " - not a Studio development build";
        return false;
    }
    std::string recorded;
    in >> recorded;
    std::transform(recorded.begin(), recorded.end(), recorded.begin(),
                   [](unsigned char c) { return std::tolower(c); });

    std::string actual, hashError;
    if (!sha256::HexOfFile(exePath, actual, hashError)) {
        error = hashError;
        return false;
    }
    if (actual != recorded) {
        error = "checksum mismatch for " + exePath +
                " - binary does not match the development build "
                "(released or replaced executable?)";
        return false;
    }

    // Best-effort build-dir attribution: nearest ancestor with a matching
    // CMakeCache (informational only; the hash above is the real gate).
    out.exePath = Canonical(exePath);
    out.sha256 = actual;
    fs::path dir = exe.parent_path();
    for (int i = 0; i < 4 && !dir.empty(); ++i) {
        const std::string cache = (dir / "CMakeCache.txt").string();
        if (fs::is_regular_file(cache, ec) &&
            SamePath(CacheValue(cache, "CMAKE_HOME_DIRECTORY"), projectRoot)) {
            out.buildDir = Canonical(dir.string());
            break;
        }
        dir = dir.parent_path();
    }
    return true;
}

} // namespace preview
} // namespace studio
} // namespace icg


