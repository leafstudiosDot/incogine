#include "assetmanager.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_filesystem.h>
#include <cstring>

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

    SDL_IOStream* stream = OpenFromDisk(path);
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
    unsigned int size = 0;
    if (EmbeddedData(path, &size)) {
        return true;
    }

    SDL_IOStream* stream = const_cast<AssetManager*>(this)->OpenFromDisk(path);
    if (stream) {
        SDL_CloseIO(stream);
        return true;
    }
    return false;
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

const char* AssetManager::AssetRoot() {
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