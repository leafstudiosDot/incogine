---
title: Assets
description: The AssetManager, asset subsystems, and code generation for embedded assets.
sidebar_position: 11
tags: [assets, asset-manager, fonts, audio]
---

# Assets

Assets are loaded from disk at runtime by default through the **AssetManager**. Only when the build is configured with `-DICG_EMBED_ASSETS=ON` (single-file distribution) do the code generators below produce C arrays compiled into the binary.

## AssetManager — `src/core/assets/assetmanager.h/.cpp`

The single entry point for all asset IO:

```cpp
Open(path) -> SDL_IOStream*
Exists(path)
```

Resolution order: **disk first, embedded fallback** (the latter only when compiled with `ICG_EMBED_ASSETS=ON`).

Per-platform disk roots:

| Platform | Root |
|----------|------|
| Desktop / iOS | `<exe-or-bundle>/assets/` (copied by CMake post-build) |
| Android | The APK's `assets/` directory (via `SDL_IOFromFile` + `AAssetManager`, wired in `android-project/app/build.gradle` through `assets.srcDirs`) |
| Web | The Emscripten virtual filesystem (`--preload-file src/assets@assets` → `Incogine.data`) |

Asset paths are canonical, e.g. `"fonts/main_font.ttf"`, `"audio/testbgm.ogg"`.

:::tip
Adding a new asset only requires placing the file in `src/assets/` (plus a registry entry in `assetmanager.cpp` when embedded mode should cover it).
:::

## Subsystems

### Audio — `src/core/assets/audio/`

An SDL3_mixer (`MIX_*`) wrapper. `Audio(path)` resolves through the AssetManager (disk first, embedded fallback) and loads via `MIX_LoadAudio_IO`. `play(loop)` accepts `-1` for infinite looping.

### Fonts — `src/core/fonts/`

OpenGL-texture-rendered text via SDL3_ttf. Each `Font` instance owns a `TTF_Font` and a GL texture that is rebuilt on text/color/scale changes. Two loaders:

- `setFontFile(path, pts)` — AssetManager-resolved, the normal path
- `setFont(data, size, pts)` — raw memory, used by the embedded fallback

Fonts are used everywhere menus appear.

### Image — `src/core/assets/image/`

An SDL_texture-backed image loader. `load(path)` resolves through the AssetManager (still unused by scenes).

:::note
`Image` renders via `SDL_Renderer`, not the GL pipeline.
:::

## Code generation

Only when the build is configured with `-DICG_EMBED_ASSETS=ON` do these generators run (all under `src/parser/`):

| Generator | Input | Output |
|-----------|-------|--------|
| `ttfparse_main.py` / `ttfparse.py` | TTFs in `src/assets/fonts/` | C headers in `<build>/generated_fonts/` |
| `audioparse.py` | Audio in `src/assets/audio/` | C arrays in `<build>/generated_audio/` |
| `svgparse.py` | Any root-level `.svg` | `_svgdata.h` (demo code, in the build dir) |
| `ios_infoplist_gen.py` | `project.xml` fields | The iOS `Info.plist` |
| `requirements.txt` | — | Lists Python deps for the parsers |

:::warning
All generated files land in the build dir — nothing is written into the source tree. Don't check in generated headers (`.c`/`.h` under `src/fonts/` are generated; only the `.ttf` files are source).
:::