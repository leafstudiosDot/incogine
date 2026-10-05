#ifndef ASSETMANAGER_H
#define ASSETMANAGER_H

#include <SDL3/SDL.h>

// Central asset access. Assets are resolved from disk first (platform
// asset root), falling back to embedded data when the game was built with
// embedded assets (ICG_EMBED_ASSETS=ON).
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

    // Returns true if the asset can be resolved (disk or embedded).
    bool Exists(const char* path) const;

    // Embedded fallback lookup (only populated when ICG_EMBED_ASSETS=ON).
    // Returns nullptr when the asset is not embedded.
    const void* EmbeddedData(const char* path, unsigned int* size) const;

private:
    AssetManager() = default;

    // Attempts to open <assetRoot>/<path> from disk.
    SDL_IOStream* OpenFromDisk(const char* path);

    // Resolves the platform asset root (executable dir on desktop/iOS,
    // empty on Android/Web where relative paths hit the APK/preloaded FS).
    const char* AssetRoot();

    char assetRoot[1024];
};

#endif // ASSETMANAGER_H