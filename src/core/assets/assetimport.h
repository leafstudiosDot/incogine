// Incogine - asset importer interface + registry.
// Part of Incogine by leafstudiosDot (MPL-2.0). See LICENSE.
//
// One place that answers "what kind of asset is this file, and how do I turn
// its bytes into something usable?". `.incoanim` (2D vector animation) is the
// first importer; later formats (FBX/OBJ/glTF/`.blend` for 3D animation) plug
// in through the same interface instead of scattering suffix checks across
// Studio and the engine.
//
// Deliberately tiny: no dependency graph, no import pipeline, no caching, no
// settings. If a future importer needs those, add them when there is a second
// importer that wants them.
//
// Bytes, not paths: Studio reads authoring files from `src/assets/` while the
// engine reads through `AssetManager::Open()` (disk / `.incoba` bundle /
// embedded), so the importer must not care where the bytes came from.

#pragma once

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

namespace icg {
namespace assets {

// Coarse classification, used by the asset browser for icons and by the engine
// to decide how to load. Deliberately format-agnostic: it describes how the
// asset is *used*, not which parser reads it.
enum class AssetKind {
    Unknown,
    Texture,     // .png .jpg .bmp ...
    Audio,       // .ogg .wav .mp3 ...
    Font,        // .ttf .otf
    Animation2D, // .incoanim
    Text,        // .txt .json .xml .md
    Source,      // .cpp .h .cs .kodo
    Binary,      // raw/structured data no editor opens
};

// Parses one asset format from memory. Implementations own their result type
// (e.g. the 2D animation importer produces an AnimDocument), so the interface
// stays format-agnostic and the registry never has to know the concrete types.
class IAssetImporter {
public:
    virtual ~IAssetImporter() = default;

    // Which asset kind this importer produces.
    virtual AssetKind kind() const = 0;

    // Lowercase extension without the dot, e.g. "incoanim". This is the
    // registry key; one extension maps to at most one importer.
    virtual const char* extension() const = 0;

    // Human-readable name for the asset browser ("2D Animation").
    virtual const char* displayName() const = 0;

    // Parses `size` bytes. Returns false and fills `error` with a
    // human-readable reason on malformed input. `bytes` may be null only when
    // `size` is 0.
    virtual bool ImportBytes(const void* bytes, size_t size, std::string& error) = 0;
};

// Registry of importers, keyed by file extension. Registration is explicit
// (never by static constructor) so the set of linked importers is visible in
// one place.
//
// The engine and Studio share the process-wide Instance(). The constructor is
// public as well so tests can build isolated registries with stub importers
// instead of mutating the shared one.
class AssetImporterRegistry {
public:
    AssetImporterRegistry() = default;
    static AssetImporterRegistry& Instance();

    // Registers an importer. Returns false (and registers nothing) when the
    // extension is empty or an importer already claims it, so the first
    // registration wins and a duplicate is a programming error worth
    // surfacing rather than silently shadowing.
    bool Register(std::unique_ptr<IAssetImporter> importer);

    // Registers the importers built into this binary. The single switch point
    // for adding formats: add one call here per new importer.
    void RegisterBuiltins();

    // Lookups by extension. `extension` is normalized (lowercased, leading dot
    // stripped), so ".INCOANIM", "incoanim" and "IncoAnim" all match.
    // Returned pointers are non-const because ImportBytes() parses into
    // importer-owned state.
    IAssetImporter* FindByExtension(const char* extension) const;

    // Lookups by asset path - extension of the path, normalized the same way.
    // Paths without an extension return nullptr.
    IAssetImporter* Find(const char* path) const;

    // All registered importers, in registration order.
    std::vector<IAssetImporter*> All() const;

    // Lowercase + strips a leading dot. Exposed because Studio needs the same
    // normalization when it dispatches on a file suffix.
    static std::string NormalizeExtension(const char* extension);

    // Extension of `path` (after the last dot of the last path segment),
    // normalized. Empty when the name has no dot.
    static std::string ExtensionOf(const char* path);

    // Drops every registration. The engine and Studio never call this; it
    // exists so a test can reuse a local registry between cases.
    void Clear() { importers_.clear(); }

private:
    std::vector<std::unique_ptr<IAssetImporter>> importers_;
};

} // namespace assets
} // namespace icg
