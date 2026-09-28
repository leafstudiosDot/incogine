#include "assetmanager.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_filesystem.h>
#include <algorithm>
#include <cstring>
#include <vector>

#ifdef ICG_EMBED_ASSETS
#include "main_font.h"
#include "jpsup_font.h"
#include "testbgm_audio.h"
#endif

namespace {
    struct EmbeddedAsset {
        const char* path;
        const unsigned char* data;
        unsigned int size;
    };

    struct EmbeddedAssetTable {
        const EmbeddedAsset* items;
        unsigned int count;
    };

    // Canonical asset paths. Only compiled in when the game is built with
    // ICG_EMBED_ASSETS=ON (single-file distribution mode). The table is
    // wrapped in a struct so the non-embedded build has no zero-size array
    // (MSVC rejects empty initializer lists for arrays).
    #ifdef ICG_EMBED_ASSETS
        const EmbeddedAsset s_embeddedAssets[] = {
            { "fonts/main_font.ttf", _mainfont_data, _mainfont_size },
            { "fonts/jpsup_font.ttf", _jpsup_font_data, _jpsup_font_size },
            { "audio/testbgm.ogg", testbgm_audio_data, testbgm_audio_size },
        };
        const EmbeddedAssetTable s_embeddedTable = { s_embeddedAssets, 3 };
    #else
        const EmbeddedAssetTable s_embeddedTable = { nullptr, 0 };
    #endif

    // .incoba magic + versions (must match src/studio/core/incoba.h).
    constexpr char kBundleMagic[6] = {'I', 'N', 'C', 'O', 'B', 'A'};
    constexpr char kIndexMagic[7] = {'I', 'N', 'C', 'O', 'B', 'A', 'I'};
    constexpr uint16_t kBundleVersion = 1;
    constexpr uint16_t kIndexVersion = 1;
    constexpr uint8_t kMethodStored = 0;
    // Upper bound for a single entry read (bundles cap payloads at 128 MiB;
    // anything bigger is almost certainly a corrupt offset/size).
    constexpr uint64_t kMaxEntryBytes = 1024ull * 1024ull * 1024ull;

    uint32_t BundleCrc32(const void* data, size_t size) {
        static uint32_t table[256];
        static bool ready = false;
        if (!ready) {
            for (uint32_t i = 0; i < 256; ++i) {
                uint32_t c = i;
                for (int k = 0; k < 8; ++k) {
                    c = (c & 1) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
                }
                table[i] = c;
            }
            ready = true;
        }
        const uint8_t* bytes = static_cast<const uint8_t*>(data);
        uint32_t crc = 0xFFFFFFFFu;
        for (size_t i = 0; i < size; ++i) {
            crc = table[(crc ^ bytes[i]) & 0xFF] ^ (crc >> 8);
        }
        return crc ^ 0xFFFFFFFFu;
    }

    // Bounds-checked little-endian cursor over a loaded file image.
    struct BundleCursor {
        const uint8_t* p;
        const uint8_t* end;
        bool ok = true;

        uint16_t U16() {
            if (p + 2 > end) {
                ok = false;
                return 0;
            }
            const uint16_t v = static_cast<uint16_t>(p[0]) |
                               (static_cast<uint16_t>(p[1]) << 8);
            p += 2;
            return v;
        }
        uint32_t U32() {
            if (p + 4 > end) {
                ok = false;
                return 0;
            }
            uint32_t v = 0;
            for (int i = 0; i < 4; ++i) {
                v |= static_cast<uint32_t>(p[i]) << (8 * i);
            }
            p += 4;
            return v;
        }
        uint64_t U64() {
            if (p + 8 > end) {
                ok = false;
                return 0;
            }
            uint64_t v = 0;
            for (int i = 0; i < 8; ++i) {
                v |= static_cast<uint64_t>(p[i]) << (8 * i);
            }
            p += 8;
            return v;
        }
    };

} // namespace

// Parses an `index.incobai` image into bundle names + sorted entries.
bool AssetManager::ParseIndexImage(const uint8_t* data, size_t size,
                                       std::vector<std::string>& files,
                                       std::vector<BundleIndexEntry>& entries) {
        if (size < 13) {
            return false;
        }
        BundleCursor c{data, data + size};
        if (std::memcmp(c.p, kIndexMagic, 7) != 0) {
            return false;
        }
        c.p += 7;
        const uint16_t version = c.U16();
        c.U16(); // flags (reserved)
        const uint16_t bundleCount = c.U16();
        if (!c.ok || version != kIndexVersion) {
            return false;
        }
        for (uint16_t i = 0; i < bundleCount; ++i) {
            const uint16_t nameLen = c.U16();
            if (!c.ok || c.p + nameLen > c.end) {
                return false;
            }
            files.emplace_back(reinterpret_cast<const char*>(c.p), nameLen);
            c.p += nameLen;
        }
        const uint32_t entryCount = c.U32();
        if (!c.ok) {
            return false;
        }
        for (uint32_t i = 0; i < entryCount; ++i) {
            const uint16_t pathLen = c.U16();
            if (!c.ok || c.p + pathLen > c.end) {
                return false;
            }
            AssetManager::BundleIndexEntry e;
            e.path.assign(reinterpret_cast<const char*>(c.p), pathLen);
            c.p += pathLen;
            e.bundle = c.U16();
            e.offset = c.U64();
            e.size = c.U64();
            e.crc = c.U32();
            if (!c.ok || c.p + 1 > c.end) {
                return false;
            }
            e.method = *c.p++;
            if (e.bundle >= files.size() || e.method != kMethodStored ||
                e.size > kMaxEntryBytes) {
                return false;
            }
            entries.push_back(e);
        }
        std::sort(entries.begin(), entries.end(),
                  [](const AssetManager::BundleIndexEntry& a,
                     const AssetManager::BundleIndexEntry& b) { return a.path < b.path; });
        return true;
    }

    // Parses a single legacy bundle's entry table (blobs are NOT loaded;
    // reads use the recorded offsets). The bundle becomes bundle index 0.
    bool AssetManager::ParseBundleTable(const uint8_t* data, size_t size, const char* name,
                                          std::vector<std::string>& files,
                                          std::vector<BundleIndexEntry>& entries) {
        if (size < 14) {
            return false;
        }
        BundleCursor c{data, data + size};
        if (std::memcmp(c.p, kBundleMagic, 6) != 0) {
            return false;
        }
        c.p += 6;
        const uint16_t version = c.U16();
        c.U16(); // flags (reserved)
        const uint32_t count = c.U32();
        if (!c.ok || version != kBundleVersion) {
            return false;
        }
        files.emplace_back(name);
        for (uint32_t i = 0; i < count; ++i) {
            const uint16_t pathLen = c.U16();
            if (!c.ok || c.p + pathLen > c.end) {
                return false;
            }
            AssetManager::BundleIndexEntry e;
            e.path.assign(reinterpret_cast<const char*>(c.p), pathLen);
            c.p += pathLen;
            e.bundle = 0;
            e.offset = c.U64();
            e.size = c.U64();
            const uint64_t stored = c.U64();
            if (!c.ok || c.p + 1 + 4 > c.end) {
                return false;
            }
            e.method = *c.p++;
            uint32_t crc = 0;
            for (int k = 0; k < 4; ++k) {
                crc |= static_cast<uint32_t>(c.p[k]) << (8 * k);
            }
            c.p += 4;
            e.crc = crc;
            if (e.method != kMethodStored || stored != e.size ||
                e.size > kMaxEntryBytes || e.offset + e.size > size) {
                return false;
            }
            entries.push_back(e);
        }
        std::sort(entries.begin(), entries.end(),
                  [](const AssetManager::BundleIndexEntry& a,
                     const AssetManager::BundleIndexEntry& b) { return a.path < b.path; });
        return true;
    }

void AssetManager::MergeBundleEntries(std::vector<BundleIndexEntry>& into,
                                      const std::vector<BundleIndexEntry>& from) {
    for (const BundleIndexEntry& e : from) {
        size_t lo = 0, hi = into.size();
        while (lo < hi) {
            const size_t mid = lo + (hi - lo) / 2;
            if (into[mid].path < e.path) {
                lo = mid + 1;
            } else {
                hi = mid;
            }
        }
        if (lo < into.size() && into[lo].path == e.path) {
            into[lo] = e; // later set wins
        } else {
            into.insert(into.begin() +
                            static_cast<std::vector<BundleIndexEntry>::difference_type>(lo),
                        e);
        }
    }
}

const AssetManager::BundleIndexEntry* AssetManager::FindEntry(
    const std::vector<BundleIndexEntry>& entries, const char* path) {
    size_t lo = 0, hi = entries.size();
    while (lo < hi) {
        const size_t mid = lo + (hi - lo) / 2;
        if (entries[mid].path < path) {
            lo = mid + 1;
        } else {
            hi = mid;
        }
    }
    if (lo < entries.size() && entries[lo].path == path) {
        return &entries[lo];
    }
    return nullptr;
}

bool AssetManager::LoadMountSet(const char* dir,
                                std::vector<std::string>& files,
                                std::vector<BundleIndexEntry>& entries) {
    const char* base = (dir && dir[0] != '\0') ? dir : "";
    const char* slash = (dir && dir[0] != '\0') ? "/" : "";
    char candidate[2048];

    // 1. Split set with an index.
    SDL_snprintf(candidate, sizeof(candidate), "%s%sindex.incobai", base, slash);
    size_t size = 0;
    void* data = SDL_LoadFile(candidate, &size);
    if (data) {
        const bool ok = ParseIndexImage(static_cast<const uint8_t*>(data), size, files, entries);
        SDL_free(data);
        if (ok && !files.empty()) {
            return true;
        }
        files.clear();
        entries.clear();
    }

    // 2. Lone bundles (current name first, then legacy).
    for (const char* single : {"a.incoba", "game.incoba"}) {
        SDL_snprintf(candidate, sizeof(candidate), "%s%s%s", base, slash, single);
        size = 0;
        data = SDL_LoadFile(candidate, &size);
        if (!data) {
            continue;
        }
        const bool ok = ParseBundleTable(static_cast<const uint8_t*>(data), size, single,
                                         files, entries);
        SDL_free(data);
        if (ok) {
            return true;
        }
        files.clear();
        entries.clear();
    }
    return false;
}

SDL_IOStream* AssetManager::OpenEntry(const char* dir,
                                      const std::vector<std::string>& files,
                                      const BundleIndexEntry& e,
                                      const char* path) {
    if (e.bundle >= files.size()) {
        return nullptr;
    }

    char bundlePath[2048];
    if (dir && dir[0] != '\0') {
        SDL_snprintf(bundlePath, sizeof(bundlePath), "%s/%s", dir, files[e.bundle].c_str());
    } else {
        SDL_snprintf(bundlePath, sizeof(bundlePath), "%s", files[e.bundle].c_str());
    }

    SDL_IOStream* file = SDL_IOFromFile(bundlePath, "rb");
    if (!file) {
        return nullptr;
    }

    uint8_t* buf = static_cast<uint8_t*>(SDL_malloc(e.size > 0 ? static_cast<size_t>(e.size) : 1));
    if (!buf) {
        SDL_CloseIO(file);
        return nullptr;
    }

    bool readOk = false;
    if (SDL_SeekIO(file, static_cast<Sint64>(e.offset), SDL_IO_SEEK_SET) >= 0) {
        size_t got = 0;
        const size_t want = static_cast<size_t>(e.size);
        while (got < want) {
            const size_t n = SDL_ReadIO(file, buf + got, want - got);
            if (n == 0) {
                break;
            }
            got += n;
        }
        readOk = (got == want);
    }
    SDL_CloseIO(file);

    if (!readOk || BundleCrc32(buf, static_cast<size_t>(e.size)) != e.crc) {
        SDL_free(buf);
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Asset bundle read failed: %s", path);
        return nullptr;
    }

    // SDL owns dynamic-mem streams: no caller-side free needed on close.
    SDL_IOStream* mem = SDL_IOFromDynamicMem();
    if (!mem) {
        SDL_free(buf);
        return nullptr;
    }
    if (e.size > 0 && SDL_WriteIO(mem, buf, static_cast<size_t>(e.size)) != static_cast<size_t>(e.size)) {
        SDL_free(buf);
        SDL_CloseIO(mem);
        return nullptr;
    }
    SDL_free(buf);
    SDL_SeekIO(mem, 0, SDL_IO_SEEK_SET);
    return mem;
}

AssetManager& AssetManager::Instance() {
    static AssetManager instance;
    return instance;
}

SDL_IOStream* AssetManager::Open(const char* path) {
    if (!path || !*path) {
        SDL_InvalidParamError("path");
        return nullptr;
    }

    SDL_IOStream* stream = OpenFromMounts(path);
    if (stream) {
        return stream;
    }

    stream = OpenFromDisk(path);
    if (stream) {
        return stream;
    }

    stream = OpenFromBundle(path);
    if (stream) {
        return stream;
    }

    unsigned int size = 0;
    const void* data = EmbeddedData(path, &size);
    if (data) {
        return SDL_IOFromConstMem(data, static_cast<Sint64>(size));
    }

    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Asset not found: %s", path);
    return nullptr;
}

bool AssetManager::Exists(const char* path) const {
    if (!path || !*path) {
        return false;
    }

    for (size_t i = mounts_.size(); i-- > 0;) {
        if (FindEntry(mounts_[i].entries, path)) {
            return true;
        }
    }

    SDL_IOStream* stream = const_cast<AssetManager*>(this)->OpenFromDisk(path);
    if (stream) {
        SDL_CloseIO(stream);
        return true;
    }

    EnsureBundles();
    if (bundleFound_ && FindEntry(bundleIndex_, path)) {
        return true;
    }

    unsigned int size = 0;
    return EmbeddedData(path, &size) != nullptr;
}

bool AssetManager::HasBundles() const {
    EnsureBundles();
    return bundleFound_;
}

size_t AssetManager::BundleEntryCount() const {
    EnsureBundles();
    return bundleFound_ ? bundleIndex_.size() : 0;
}

const void* AssetManager::EmbeddedData(const char* path, unsigned int* size) const {
    if (!path || !*path) {
        return nullptr;
    }

    for (unsigned int i = 0; i < s_embeddedTable.count; ++i) {
        const EmbeddedAsset& asset = s_embeddedTable.items[i];
        if (std::strcmp(asset.path, path) == 0) {
            if (size) {
                *size = asset.size;
            }
            return asset.data;
        }
    }
    return nullptr;
}

SDL_IOStream* AssetManager::OpenFromDisk(const char* path) {
#if defined(SDL_PLATFORM_ANDROID) || defined(SDL_PLATFORM_EMSCRIPTEN)
    // Android: SDL_IOFromFile falls back to the APK's assets/ directory via
    // the AAssetManager. Web: relative paths read from the preloaded
    // emscripten virtual filesystem.
    SDL_IOStream* stream = SDL_IOFromFile(path, "rb");
    if (stream) {
        return stream;
    }
#else
    // Desktop/iOS: <assetRoot>/<path>, where the asset root is either the
    // assets/ directory next to the executable/bundle, or ./assets/ when
    // running from the source tree (dev mode).
    const char* root = AssetRoot();
    if (root && *root) {
        char fullPath[2048];
        SDL_snprintf(fullPath, sizeof(fullPath), "%s/%s", root, path);
        SDL_IOStream* stream = SDL_IOFromFile(fullPath, "rb");
        if (stream) {
            return stream;
        }
    }
#endif
    return nullptr;
}

SDL_IOStream* AssetManager::OpenFromMounts(const char* path) {
    for (size_t i = mounts_.size(); i-- > 0;) {
        const BundleIndexEntry* e = FindEntry(mounts_[i].entries, path);
        if (!e) {
            continue;
        }
        SDL_IOStream* stream = OpenEntry(mounts_[i].dir.c_str(), mounts_[i].files, *e, path);
        if (stream) {
            return stream;
        }
        // Corrupt entry in a newer mount: fall through to older mounts/base.
    }
    return nullptr;
}

SDL_IOStream* AssetManager::OpenFromBundle(const char* path) {
    EnsureBundles();
    if (!bundleFound_) {
        return nullptr;
    }
    const BundleIndexEntry* e = FindEntry(bundleIndex_, path);
    if (!e) {
        return nullptr;
    }
    return OpenEntry(bundleDir_, bundleFiles_, *e, path);
}

void AssetManager::EnsureBundles() const {
    if (bundleReady_) {
        return;
    }
    bundleReady_ = true;

    // Candidate base dirs, in priority order: platform asset root, a
    // CWD-relative assets/ dir, then bare relative names (Android/Web).
    char bases[3][1024];
    int baseCount = 0;
    const char* root = AssetRoot();
    if (root && *root) {
        SDL_snprintf(bases[baseCount], sizeof(bases[baseCount]), "%s", root);
        ++baseCount;
    }
    SDL_snprintf(bases[baseCount], sizeof(bases[baseCount]), "assets");
    ++baseCount;
    bases[baseCount][0] = '\0';
    ++baseCount;

    for (int b = 0; b < baseCount && !bundleFound_; ++b) {
        std::vector<std::string> files;
        std::vector<BundleIndexEntry> entries;
        if (LoadMountSet(bases[b], files, entries)) {
            SDL_snprintf(bundleDir_, sizeof(bundleDir_), "%s", bases[b]);
            bundleFiles_ = std::move(files);
            bundleIndex_ = std::move(entries);
            bundleFound_ = true;
        }
    }
}

bool AssetManager::MountBundleDir(const char* dir) {
    if (!dir || !*dir) {
        return false;
    }
    std::vector<std::string> files;
    std::vector<BundleIndexEntry> entries;
    if (!LoadMountSet(dir, files, entries)) {
        return false;
    }
    Mount mount;
    mount.dir = dir;
    mount.files = std::move(files);
    mount.entries = std::move(entries);
    mounts_.push_back(std::move(mount));
    return true;
}

bool AssetManager::MountBundleFile(const char* path) {
    if (!path || !*path) {
        return false;
    }
    const size_t len = std::strlen(path);
    const bool isIndex = len > 8 && std::strcmp(path + len - 8, ".incobai") == 0;

    size_t size = 0;
    void* data = SDL_LoadFile(path, &size);
    if (!data) {
        return false;
    }
    std::vector<std::string> files;
    std::vector<BundleIndexEntry> entries;
    bool ok = false;
    if (isIndex) {
        ok = ParseIndexImage(static_cast<const uint8_t*>(data), size, files, entries);
    } else {
        const char* slash = std::strrchr(path, '/');
        const char* backslash = std::strrchr(path, '\\');
        const char* name = path;
        if (slash && (!backslash || slash > backslash)) {
            name = slash + 1;
        } else if (backslash) {
            name = backslash + 1;
        }
        ok = ParseBundleTable(static_cast<const uint8_t*>(data), size, name, files, entries);
    }
    SDL_free(data);
    if (!ok || files.empty()) {
        return false;
    }
    Mount mount;
    mount.dir = path;
    const size_t sep = mount.dir.find_last_of("/\\");
    mount.dir = (sep == std::string::npos) ? std::string() : mount.dir.substr(0, sep);
    mount.files = std::move(files);
    mount.entries = std::move(entries);
    mounts_.push_back(std::move(mount));
    return true;
}

size_t AssetManager::MountedBundleCount() const {
    return mounts_.size();
}

void AssetManager::ClearMounts() {
    mounts_.clear();
}

const char* AssetManager::AssetRoot() const {
    if (assetRoot[0] != '\0') {
        return assetRoot;
    }

    assetRoot[0] = '\0';

    const char* base = SDL_GetBasePath();
    if (base && *base) {
        SDL_snprintf(assetRoot, sizeof(assetRoot), "%sassets", base);
        return assetRoot;
    }

    SDL_snprintf(assetRoot, sizeof(assetRoot), "assets");
    return assetRoot;
}
