---
title: Assets
description: The AssetManager, asset subsystems, and code generation for embedded assets.
sidebar_position: 12
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

Resolution order: **mounted update bundles first, then disk, bundles,
embedded last** (embedded only when compiled with `ICG_EMBED_ASSETS=ON`).

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

## Bundled builds (`.incoba`)

Every game build splits `src/assets/` into payload-capped bundles
(`ICG_INCOBA_MAX_MB`, default 128 MiB each) plus an `index.incobai` lookup
table, shipped inside the executable's `assets/` folder:

```text
<exe>/assets/
├── index.incobai        # path -> (bundle, offset, size, crc) for every asset
├── a.incoba             # ...or a_00.incoba, a_01.incoba, ... past the cap
└── ...                  # loose files may sit alongside as dev overrides
```

Two ship modes, one CMake switch (`ICG_USE_INCOBA`, default `ON`) — Dev,
Debug, Staging, and Release all behave the same:

- `ON`: `<exe>/assets/` holds **only** `index.incobai` + `*.incoba`. The
  folder is cleared first, so no stale loose files survive.
- `OFF` (`-DICG_USE_INCOBA=OFF`): loose `src/assets/` files are copied
  instead — direct-disk flow, no bundles involved.

At runtime `AssetManager` probes `<assetRoot>/index.incobai` (falling back
to a lone `a.incoba`/`game.incoba`, then `assets/`-relative and bare names
for Android/Web layouts). Index entries are binary-searched by path and read via
direct offset seeks — bundles that cannot contain the asset are never
opened. `HasBundles()` / `BundleEntryCount()` report mount state. Reads are
CRC-checked; corrupt entries fall through to the embedded fallback.

## Downloadable update sets (live games)

Bundles double as versioned content updates (gacha banners, events,
balance patches) without re-shipping the game:

```text
server:  patch_2026-10-01/ { index.incobai, a_00.incoba, ... } + version note
            |
            v  (game downloads into a writable dir, e.g. SDL_GetPrefPath)
client:  <prefpath>/patches/2026-10-01/ { index.incobai, a_00.incoba, ... }
            |
            v  AssetManager::MountBundleDir("<prefpath>/patches/2026-10-01")
runtime: patch entries override same-path base assets, live, no restart
```

- Produce a patch set with the packer (`--split` over just the new/changed
  files); a patch dir may also be a single `.incoba` mounted with
  `MountBundleFile()`. Multi-bundle patch sets must ship their index.
- Mounts are explicit and ordered: the newest mount wins, and mounts beat
  loose/base content. `MountedBundleCount()` / `ClearMounts()` manage them.
- Delivery (version check, download, resume, per-entry CRC already covers
  integrity) is game code — the engine only mounts. Keep a game-side
  `version.txt` next to each patch dir so the client knows what is applied.

Packaging, compression headroom, and casual modification resistance are the
goals — explicitly **not DRM**. See [Incogine Studio](./studio.md) for the
format and packer.

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