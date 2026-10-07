// Incogine - `.incoanim` (2D vector animation) save/load.
// Part of Incogine by leafstudiosDot (MPL-2.0). See LICENSE.
//
// v1 is a single UTF-8 JSON document with a `formatVersion` field, chosen so
// animation files are reviewable in a pull request and diffable in version
// control (the alternative - a zip container - is deliberately deferred; the
// existing `.incoba` packer already handles shipping). Schema documentation
// lives in `docs/incoanim.md`.
//
// Migration path: every load runs `Migrate()` on the parsed JSON before
// binding, so a v0/v1/... document is always upgraded to the current version
// in one place. v1 is the first release, so `Migrate` is currently a
// pass-through that validates the version; adding v2 means one `case 1:` there.

#pragma once

#include <string>
#include <vector>

#include "anim_document.h"
#include "anim_json.h"

namespace icg {
namespace anim {

// Bumped whenever the on-disk schema changes incompatibly. Old readers reject
// anything newer (with a clear message) rather than mis-reading it.
inline constexpr int kIncoanimFormatVersion = 1;

inline constexpr const char* kIncoanimExtension = "incoanim";

// Serializes `document` as `.incoanim` v1 JSON.
std::string Serialize(const AnimDocument& document);

// Parses `.incoanim` JSON into `out`. Returns false with a human-readable
// `error` on malformed input, an unsupported future version, or a document
// that fails validation.
bool Deserialize(const std::string& text, AnimDocument& out, std::string& error);

// --- file IO (Studio authoring side; the engine uses ImportBytes) --

// Parses raw file bytes: v2 container or v1 JSON text, auto-detected. This is
// what the asset importer calls, so the game runtime reads both formats.
bool DeserializeBytes(const void* bytes, size_t size, AnimDocument& out,
                      std::string& error);

// Reads and deserializes a file (either format). False + `error` when
// unreadable or invalid.
bool LoadFile(const std::string& path, AnimDocument& out, std::string& error);

// Always saves in the v2 container format (compact + compressed), atomically
// via a sibling `.tmp` + rename, so an interrupted save never truncates the
// user's animation. v1 files migrate the first time they are saved.
bool SaveFile(const std::string& path, const AnimDocument& document,
              std::string& error);

// --- migration (exposed for tests) --

// Upgrades a parsed document from `fromVersion` to `kIncoanimFormatVersion`,
// mutating `root` in place. Returns false with `error` when the version is
// unsupported or the input cannot be upgraded.
bool Migrate(json::JsonValue& root, int fromVersion, std::string& error);

} // namespace anim
} // namespace icg
