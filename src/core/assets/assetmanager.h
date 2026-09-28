#ifndef ASSETMANAGER_H
#define ASSETMANAGER_H

#include <SDL3/SDL.h>

#include <cstdint>
#include <string>
#include <vector>

// Central asset access. Resolution order:
//
//   1. Mounted update bundles (newest mount first) — downloadable content
//      packs (gacha events, patches) mounted via MountBundleDir/File.
//   2. Loose files on disk (platform asset root) — developer override.
//   3. `.incoba` asset bundles next to the assets (`index.incobai` plus
//      payload-capped `*.incoba`, or a single legacy bundle).
//      The index maps every path to its bundle + file offset, so lookups
//      never open or scan bundles that cannot contain the asset.
//   4. Embedded data (only when compiled with ICG_EMBED_ASSETS=ON).
//
// Bundle layout (little-endian, v1 — see `src/studio/core/incoba.h`, the
// reference implementation; this parser must stay in sync with it):
//   bundle: magic[6]="INCOBA", version u16=1, flags u16=0, entryCount u32,
//           per entry { pathLen u16, path, offset u64, size u64,
//                       storedSize u64, method u8, crc32 u32 }, then blobs.
//   index:  magic[7]="INCOBAI", version u16=1, flags u16=0, bundleCount u16,
//           bundle names, entryCount u32,
//           per entry { pathLen u16, path, bundleIdx u16, offset u64,
//                       size u64, crc32 u32, method u8 } (sorted by path).
// v1 only supports method 0 (stored/uncompressed); anything else is
// rejected. CRC-32 is a corruption check, not security.
//
// Paths use forward slashes and are relative to the asset root, e.g.
// "fonts/main_font.ttf" or "audio/testbgm.ogg".
class AssetManager {
public:
    static AssetManager& Instance();

    // Opens an asset for reading. The caller owns the returned stream and
    // must close it with SDL_CloseIO(). Returns nullptr if the asset could
    // not be found anywhere.
    SDL_IOStream* Open(const char* path);

    // Returns true if the asset can be resolved (disk, bundle, or embedded).
    bool Exists(const char* path) const;

    // True once an index or single-bundle file has been found next to the
    // assets (triggers the lazy bundle scan).
    bool HasBundles() const;
    // Number of indexed bundle entries (0 when no bundles are mounted).
    size_t BundleEntryCount() const;

    // Mounts a downloadable content/update set at runtime (e.g. files the
    // game fetched from its update server into a writable directory).
    // MountBundleDir accepts a directory holding `index.incobai` (a split
    // patch set) or a lone `a.incoba`/`game.incoba`; MountBundleFile
    // accepts one `.incoba` bundle or `.incobai` index directly. Mounted
    // entries override same-path base-install assets; when several sets
    // are mounted, the newest mount wins. Mounts work live — no restart
    // needed. Returns false when nothing usable was found.
    bool MountBundleDir(const char* dir);
    bool MountBundleFile(const char* path);
    // Number of runtime-mounted update sets.
    size_t MountedBundleCount() const;
    // Drops all runtime mounts (base install bundles are unaffected).
    void ClearMounts();

    // Embedded fallback lookup (only populated when ICG_EMBED_ASSETS=ON).
    // Returns nullptr when the asset is not embedded.
    const void* EmbeddedData(const char* path, unsigned int* size) const;

private:
    AssetManager() = default;

    struct BundleIndexEntry {
        std::string path;
        uint16_t bundle = 0; // index into bundleFiles_
        uint64_t offset = 0; // absolute file offset inside the bundle
        uint64_t size = 0;
        uint32_t crc = 0;
        uint8_t method = 0;
    };

    // Attempts to open <assetRoot>/<path> from disk.
    SDL_IOStream* OpenFromDisk(const char* path);

    // Attempts to open <path> from runtime-mounted update bundles
    // (newest mount first).
    SDL_IOStream* OpenFromMounts(const char* path);

    // Attempts to open <path> from the base-install `.incoba` bundles.
    SDL_IOStream* OpenFromBundle(const char* path);

    // Locates `index.incobai` (or legacy `game.incoba`) next to the assets
    // on first bundle access; remembers bundle dir + entry table.
    void EnsureBundles() const;

    static bool ParseIndexImage(const uint8_t* data, size_t size, std::vector<std::string>& files, std::vector<BundleIndexEntry>& entries);
    static bool ParseBundleTable(const uint8_t* data, size_t size, const char* name, std::vector<std::string>& files, std::vector<BundleIndexEntry>& entries);
    // Sorted insert-or-replace (later sets override same-path entries).
    static void MergeBundleEntries(std::vector<BundleIndexEntry>& into,
                                   const std::vector<BundleIndexEntry>& from);
    static const BundleIndexEntry* FindEntry(const std::vector<BundleIndexEntry>& entries,
                                             const char* path);
    // Loads dir/index.incobai, else dir/a.incoba, else dir/game.incoba.
    // `dir` may be "" for bare relative names.
    static bool LoadMountSet(const char* dir,
                             std::vector<std::string>& files,
                             std::vector<BundleIndexEntry>& entries);
    static SDL_IOStream* OpenEntry(const char* dir,
                                   const std::vector<std::string>& files,
                                   const BundleIndexEntry& entry,
                                   const char* path);

    // Resolves the platform asset root (executable dir on desktop/iOS,
    // empty on Android/Web where relative paths hit the APK/preloaded FS).
    const char* AssetRoot() const;

    mutable char assetRoot[1024] = {};
    mutable bool bundleReady_ = false;
    mutable bool bundleFound_ = false;
    mutable char bundleDir_[1024] = {};
    mutable std::vector<std::string> bundleFiles_;
    mutable std::vector<BundleIndexEntry> bundleIndex_;

    struct Mount {
        std::string dir; // bundle file names resolve against this
        std::vector<std::string> files;
        std::vector<BundleIndexEntry> entries; // sorted by path, deduped
    };
    std::vector<Mount> mounts_;
};

#endif // ASSETMANAGER_H
