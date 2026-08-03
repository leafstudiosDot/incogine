# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project: Incogine

A 2D/3D-capable C++ game engine (and reference game) by **leafstudiosDot**, distributed under **Incogine License**. The repo contains the engine, a sample `Puroko` game built on it, and a CMake superbuild that glues everything together.

**Engine version** is in `src/core/engine/version.h` and mirrored into `src/project.xml` (`<incogine_version>`). The CMake `VERSION_STRING` macro is what `main.cpp` prints at startup.

**Project identity** (window name, executable name, bundle ID, copyright) is read by `CMakeLists.txt` regex from `src/project.xml` and exported as `PROJECT_NAME`, `WINDOW_NAME`, `BUNDLE_IDENTIFIER`, `PROJECT_VERSION`, `PROJECT_AUTHOR`, `PROJECT_COPYRIGHT`, `PROJECT_DESCRIPTION`, `INCOGINE_VERSION` defines. Per `CONTRIBUTING.md`, contributors must not remove `leafstudiosDot` or `Incogine` from derivatives; the `name` key in `project.xml` must be a single token (no spaces) because it becomes the executable filename.

## Build Commands

All builds are driven by the top-level `CMakeLists.txt`. Standard patterns:

### Windows (Visual Studio / MSVC + Ninja)
```
mkdir build
cd build
cmake ..          # uses CMakeSettings.json (x64-Debug, Ninja)
cmake --build .
```
Or via the VS Code task: `CMake: build` (runs `cmake --build build --target Incogine`).

The MSVC build forces `/utf-8`; MinGW/Clang add `-fexec-charset=UTF-8`. The Windows target name is the value of `<name>` in `src/project.xml` (currently `Incogine`).

### macOS / Linux
```
mkdir build && cd build && cmake .. && make
```
On macOS, `CMakeLists.txt` pins `gcc`/`g++` to `/usr/bin/...` when `APPLE`.

### iOS (Xcode)
```
mkdir build && cd build && cmake -G Xcode -DCMAKE_SYSTEM_NAME=iOS ..
```

### WebAssembly (Emscripten)
The `emsdk` submodule is required. First-time setup:
```
cd emsdk
./emsdk update
./emsdk install latest
./emsdk activate latest
source ./emsdk_env.sh
```
Then:
```
mkdir build && cd build
emcmake cmake ..
emmake make
```
Web builds use `src/web/init.html` as the shell file and link with `-s USE_SDL=3`.

### Android
NDK path expected at `reqs/android_ndk`; the build sets `ANDROID_ABI=arm64-v8a`, `ANDROID_PLATFORM_LEVEL=android-26`, and uses `clang`. SDL2 shared libs come from `${SDL2_PATH}/lib/armeabi-v7a/libSDL2.so` (despite targeting arm64).

## Code-Generation Steps

The CMake build invokes three Python scripts before compiling — these run automatically but are worth knowing about:

- `src/parser/ttfparse_main.py` and `src/parser/ttfparse.py` convert TTF files in `src/fonts/` to C headers (`main_font.h`, `jpsup_font.h`). Source `.ttf` files live in `src/fonts/`.
- `src/parser/svgparse.py` converts any root-level `.svg` into a `_svgdata.h` header.
- `src/parser/ios_infoplist_gen.py` generates the iOS `Info.plist` from `project.xml` fields.
- `src/parser/requirements.txt` lists Python deps for the parsers.

Generated `*.c` / `*.h` font files are gitignored (see `.gitignore`).

## High-Level Architecture

### Main loop (`src/main.cpp`)
Creates a singleton `Engine`, runs `Init()` → `Events/Update/Render` while `running()` → `Cleanup()`. Recognized CLI flags: `-dev`, `-debug`, `--skipSplash`.

### `Engine` (singleton) — `src/core/engine/engine.h/.cpp`
Owns: SDL window, OpenGL context (via SDL3 GL), TTF + mixer init, the `SceneManager`, FPS/dev-mode overlay, and window-size bookkeeping. The default window is 1280×720 (`SCREEN_WIDTH/HEIGHT`), clamped to 16:9 on resize with a 1280×720 minimum. F11 toggles fullscreen. Returns the SDL event of the current frame via `GetEventProvider()`.

Singleton access pattern: `Engine::Instance(argc, argv)` (caches and returns the same pointer). Many subsystems grab it with `Engine::Instance(0, nullptr)` for the logger and to query `inDevMode()`.

### Scene system — `src/core/scenes/scenes.h/.cpp`
- `Scene` is a polymorphic base with `Prestart()` (optional), `Start()`, `Update()`, `Render()`, and `Events(const SDL_Event&)`. Scenes are owned exclusively by `SceneManager`; `SetScene` deletes the previous scene and calls `Prestart → Start`.
- A scene **constructor runs before `Start()`**. Most of the codebase does font/audio/scene-owned-resource initialization in the constructor and leaves `Start()` empty — `Start()` is the "everything is now wired up" callback.
- The first scene is `Splash` (skippable via `--skipSplash` → `MainScene`).

### Game / project scenes — `src/scenes/`
- `splash/` — animated "Powered by Incogine" intro using cubic-bezier easing, then `SetScene(new MainScene())`.
- `MainScene` — top-level menu (New Game / Load Game / Settings / Credits / Exit), arrow/WASD navigation, Enter to select.
- `game/GameScene` — example play scene; instantiates a `PauseMenu` and plays a background audio track. Currently no real game objects.
- `settings/SettingsScene` — menu with sub-menu navigation backed by a static `SettingsMenu` vector of `MenuItem { name, subItems }`. Each sub-item carries a `std::function<void()>` action.
- `credits/Credits` — credits scene (currently a stub).

### Project (Puroko) library — `src/project/`
`CMakeLists.txt` builds `src/project/*.cpp|h` and `src/scenes/**` (except `splash/` and `settings/`, which are deliberately excluded from this library) into a **static** library named `Puroko`. The final executable is then `Incogine` (from `<name>`) which links `Puroko` plus the rest of `src/`.

In practice, `src/project/` currently only contains a placeholder `Script` class (empty `Start`/`Update`) and `pausemenu/` (a real `PauseMenu` overlay used by `GameScene`). `pausemenu/` lives under `src/project/` but is included via the `src/scenes/**` glob — it pairs the `Square` scene-object primitive with menu font rendering.

### Object / Component model — `src/core/objects/`, `src/core/components/`
- `Object` carries `Position`/`Scale`/`Rotation`/`Color` POD structs and a `std::vector<Component>`. Subtypes: `Cube` (no rendering yet), `Square` (renders with its `Sprite` component).
- `Component` is a thin base; existing components are `Transform` and `Sprite` (color only). The component system is intentionally minimal — most scenes don't use `Object` directly; they compose `Square`/`Font`/`Audio` manually.
- `Object::Render()` is currently a no-op; rendering is implemented per concrete subclass (e.g. `Square::Render`).

### Assets / subsystems — `src/core/assets/`, `src/core/fonts/`
- `image/Image` — SDL_texture-backed image loader (loads from memory).
- `audio/Audio` — `SDL3_mixer` (`MIX_*`) wrapper. Files are resolved relative to `<exe-dir>/data/audio/<filename>`. A post-build step on Windows copies `src/assets/audio/` to `<build>/Incogine/data/audio/`. `play(loop)` accepts `-1` for infinite looping.
- `fonts/Font` — OpenGL-texture-rendered text via SDL3_ttf. Each `Font` instance owns a `TTF_Font` and a GL texture that is rebuilt on text/color/scale changes. Used everywhere menus appear.

### Save data — `src/core/engine/savedata/`
`SaveData` resolves a per-platform writable path via `SDL_GetPrefPath(PROJECT_AUTHOR, PROJECT_NAME)`. `Save()`/`Load()` are stubbed (`return false`) — fill these in when implementing persistence.

### Platform layer — `src/core/platforms/platforms.h`
`printPlatform()` selects a human-readable name from `__APPLE__`/`__ANDROID__`/`__ORBIS__`/`__PROSPERO__`/`_DURANGO`/`__XBOXONE__`/`__NX__`/`__Mira__` etc. (PlayStation/Xbox/Switch/Mira are detected by macro but not actually supported in the current CMake build.)

### Web layer — `src/web/init.html`
Sets up the Emscripten `Module` shim and appends a `<canvas>`. The final Emscripten build emits `Incogine.html`/`Incogine.js`.

## Common Pitfalls

- The CMake glob **`src/scenes/splash/*` and `src/scenes/settings/*` are explicitly removed** from the `Puroko` static library, so they only link into the executable. If you add new scenes in those subfolders, that's intentional — but if you add a new subfolder that should be part of `Puroko`, update the `list(REMOVE_ITEM ...)` in `CMakeLists.txt`.
- TTF files at `src/fonts/*.ttf` are source; `*.c`/`*.h` are generated. Don't check in generated headers.
- The Windows build copies the static `Puroko.lib` next to the executable as a post-build step — don't be surprised by its presence.
- Audio files for development live in `src/assets/audio/`; in production, the executable looks for them at `<exe-dir>/data/audio/`.
- Resize is forced to 16:9 (`winHeight = winWidth * 9 / 16`); menu layouts in scenes use `windowHeight / 720` as a base scale factor — assume 720p is the design resolution.
- Many scenes log through `std::cerr` only when `Engine::Instance(0, nullptr)->inDevMode()` is true; you won't see those messages without `-dev`.

## Where to Look for What

| To change… | Look at… |
|---|---|
| Engine version / `VERSION_STRING` | `src/core/engine/version.h` |
| App name, window title, bundle ID, copyright | `src/project.xml` (regex-extracted by CMake) |
| The first scene shown at startup | `Engine::Init()` in `src/core/engine/engine.cpp` |
| A new game scene | Add to `src/scenes/`, instantiate via `Engine::Instance(0, nullptr)->SetScene(new ...)` |
| Settings menu entries | `SettingsMenu` static vector in `src/scenes/settings/SettingsScene.h` |
| Audio files | Drop in `src/assets/audio/`; reference by basename (e.g. `"testbgm.ogg"`) |
| Fonts | Drop TTF in `src/fonts/`, add a `add_custom_command` block in `CMakeLists.txt` mirroring the existing `main_font` / `jpsup_font` rules |
| Save file location | `src/core/engine/savedata/savedata.cpp` (`SDL_GetPrefPath` based) |
| Platform-specific code | `src/core/platforms/platforms.h` and the per-`PLATFORM STREQUAL` blocks in `CMakeLists.txt` |
