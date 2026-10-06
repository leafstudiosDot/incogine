---
title: Architecture
description: High-level architecture of Incogine, including the main loop and the Engine singleton.
sidebar_position: 9
tags: [architecture, engine]
---

# Architecture

## Main loop — `src/main.cpp`

Creates a singleton `Engine`, then runs `Init()` → `Events/Update/Render` while `running()` → `Cleanup()`.

Recognized CLI flags:

| Flag | Effect |
|------|--------|
| `-dev` | Enables developer mode (extra `std::cerr` logging, dev-mode overlay) |
| `-debug` | Enables debug mode |
| `--skipSplash` | Skips the splash scene and goes straight to `MainScene` |

## The `Engine` singleton — `src/core/engine/engine.h/.cpp`

`Engine` owns:

- The SDL window
- The OpenGL context (via SDL3 GL)
- TTF + mixer initialization
- The `SceneManager`
- The FPS / dev-mode overlay
- Window-size bookkeeping

Key behaviors:

- The default window is 1280×720 (`SCREEN_WIDTH`/`SCREEN_HEIGHT`), clamped to 16:9 on resize with a 1280×720 minimum. `F11` toggles fullscreen.
- On mobile (Android/iOS) and Web, windows are always fullscreen and OS-controlled: resize events are accepted as-is (no 16:9 clamp / `SDL_SetWindowSize`), and `ToggleFullscreen()` is a no-op.
- Returns the current frame's SDL event via `GetEventProvider()`.

### Singleton access pattern

```cpp
Engine::Instance(argc, argv)   // caches and returns the same pointer
```

Many subsystems grab it with `Engine::Instance(0, nullptr)` for the logger and to query `inDevMode()`.

## Where the first scene comes from

`Engine::Init()` in `src/core/engine/engine.cpp` selects the first scene. See [Scenes](./scenes.md).

## Platform detection

`src/core/platforms/platforms.h` — `printPlatform()` selects a human-readable name from `__APPLE__`/`__ANDROID__`/`__ORBIS__`/`__PROSPERO__`/`_DURANGO`/`__XBOXONE__`/`__NX__`/`__Mira__` etc.

:::warning
PlayStation/Xbox/Switch/Mira are detected by macro but not actually supported in the current CMake build.
:::

## Directory map

| Area | Location |
|------|----------|
| Main loop | `src/main.cpp` |
| Engine singleton | `src/core/engine/` |
| Scene system | `src/core/scenes/` |
| Object/component model | `src/core/objects/`, `src/core/components/` |
| Scripting system | `src/core/components/script/`, `src/core/scripting/` |
| C# runtime host | `src/core/scripting/csharp/` |
| Kodo interpreter | `src/core/scripting/kodo/` |
| Script files (C#) | `src/scripts/csharp/` |
| Script files (Kodo) | `src/scripts/kodo/` |
| Assets / fonts / audio | `src/core/assets/`, `src/core/fonts/` |
| Asset importer registry | `src/core/assets/assetimport.h` |
| 2D vector animation (`.incoanim`) | `src/core/animation/` |
| Studio | `src/studio/` (see [Incogine Studio](./studio.md)) |
| Incogine Animator (animation editor) | `src/studio/animator/` (see [`.incoanim`](./incoanim.md)) |
| Save data | `src/core/engine/savedata/` |
| Platform layer | `src/core/platforms/` |
| Web shell | `src/web/init.html` |

## Engine modules — `IncogineAssets`, `IncogineAnim`

Two static libraries sit between `src/core/` and the game executable. Both are
plain **C++17 with no SDL and no Qt**, so the SDL3 runtime, the Qt-free test
harness, and Incogine Studio can all link the exact same code.

| Target | Sources | What |
|---|---|---|
| `IncogineAssets` | `src/core/assets/assetimport.*` | `IAssetImporter` + `AssetImporterRegistry`: the generic extension → importer seam. Knows no specific format. |
| `IncogineAnim` | `src/core/animation/*` | 2D vector animation data model, geometry, `.incoanim` IO, and the `.incoanim` importer. Links `IncogineAssets`. |

Their sources are removed from the executable globs (`list(REMOVE_ITEM ...)`,
next to the Studio and Puroko exclusions), so they are **linked once**, not
compiled twice.

Inside `IncogineAnim` the headers form a one-directional chain — a cycle here
does not fail at the first include, it silently leaves types incomplete:

```
anim_types.h    scalars, color, 2D affine, easing, tween spans
anim_path.h     the segments a path is made of
anim_geometry.h flattening, bounds, hit tests        (needs anim_path.h)
anim_document.h layers, keyframes, shapes, drawing  (needs anim_geometry.h)
anim_commands.h undo/redo commands                  (needs anim_document.h)
anim_io.h       .incoanim save/load + migration
```

The editor's stage canvas and the future runtime rasterizer both consume
`ResolveShape()`, so a preview cannot drift from the baked sprite sheet.

:::tip
Splitting the importer seam from the animation model is what lets a future 3D
importer (FBX, OBJ, glTF, `.blend`) link `IncogineAssets` on its own instead of
dragging in the 2D model. Register a new format in
`AssetImporterRegistry::RegisterBuiltins()`.
:::

Both are `POSITION_INDEPENDENT_CODE ON` because static library code is linked
into a shared library on Android. Neither needs the SDL platform blocks — they
compile unchanged for every platform the engine targets.

## Staged runtimes

Three targets stage their native runtime next to the executable so they run
from the build directory without any `PATH` setup:

| Target | Mechanism | What |
|---|---|---|
| `Incogine` (game) | `POST_BUILD` copy, top-level `CMakeLists.txt` | SDL3, SDL3_ttf, SDL3_image, SDL3_mixer DLLs |
| `IncogineStudio` | `POST_BUILD` **windeployqt**, `src/studio/CMakeLists.txt` | Qt6 DLLs **and** required plugins |
| `IncogineAnimator` | same `icg_deploy_qt_runtime()` helper | same |

Qt needs its `platforms/qwindows.dll` plugin to start at all, so a manual DLL
copy would still fail; `windeployqt` resolves the whole dependency set and picks
the debug or release flavor per configuration. Linux and macOS stage nothing —
Qt resolves through the normal loader paths there.

## Test suites

`tests/` holds headless suites registered with CTest (`BUILD_TESTING`, on by
default, skipped on Android/Web):

| Test | Covers |
|---|---|
| `IncogineAnimTests` | 2D affine math, color/easing, geometry, keyframe and frame-span logic, `.incoanim` round-trip and file IO, the undo/redo command stack |
| `IncogineAssetsTests` | Importer registry lookup rules and the `.incoanim` importer |

```
cmake --build . --target IncogineAnimTests IncogineAssetsTests
ctest --output-on-failure
```

`tests/test_check.h` is a small stdlib harness rather than a framework
dependency; each suite is its own executable with its own `main()`, so adding a
module is a few lines of CMake.
