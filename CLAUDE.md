# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project: Incogine

A 2D/3D-capable C++ game engine (and reference game) by **leafstudiosDot**, distributed under **MPL-2.0**. The repo contains the engine, a sample `Puroko` game built on it, and a CMake superbuild that glues everything together.

**Two builds in one repo:** `Incogine` is the **engine core** build; `Puroko` is a **game-project** build — a reference example of any project someone can make on Incogine. Think of it as your own game's project layer: your game would replace/parallel `Puroko` (i.e. `src/project/`, `src/scenes/`, `src/project.xml`), while the engine core (`src/core/`, `src/main.cpp`, `CMakeLists.txt`) stays the same. Keep this distinction in mind when changing or documenting either.

## Documentation Policy

**`docs/` must stay in sync with the code.** Any change made that touches something not already documented in `docs/` — new features, new files, changed behavior, new build steps, renamed subsystems — must also update `docs/` (Docusaurus-compatible markdown, see the pages in `docs/`). Add or revise the relevant page(s); never leave undocumented changes behind.

**Engine version** is in `src/core/engine/version.h` and mirrored into `src/project.xml` (`<incogine_version>`). The CMake `VERSION_STRING` macro is what `main.cpp` prints at startup.

**Project identity** (window name, executable name, bundle ID, copyright) is read by `CMakeLists.txt` regex from `src/project.xml` and exported as `PROJECT_NAME`, `WINDOW_NAME`, `BUNDLE_IDENTIFIER`, `PROJECT_VERSION`, `PROJECT_AUTHOR`, `PROJECT_COPYRIGHT`, `PROJECT_DESCRIPTION`, `INCOGINE_VERSION` defines. Per `CONTRIBUTING.md`, contributors must not remove `leafstudiosDot` or `Incogine` from derivatives; the `name` key in `project.xml` must be a single token (no spaces) because it becomes the executable filename.

## Licensing & Project Direction

**License: MPL-2.0** (Mozilla Public License 2.0). The official text is in `LICENSE`. The old custom "Incogine License" and `COMMERCIAL_LICENSE.md` were removed. **Keep this section updated whenever licensing is discussed — it records decisions made with the maintainer.**

Decisions (recorded 2026-08-16):

- **Community over revenue.** No commercial license fees, no royalties, no EULA. Donations are optional (a GitHub Sponsors link can be added to the README once set up).
- **Games built with Incogine may be closed source.** Game/project code (e.g. `src/project/`, `src/scenes/`) is a "Larger Work" under MPL — it can stay proprietary. Only the engine itself (Covered Software) carries obligations.
- **The engine core can never be closed.** MPL file-level copyleft: anyone who modifies engine files and distributes them must publish those modifications under MPL. This is the maintainer's core concern — nobody can take the engine proprietary.
- **Attribution.** All files must retain `leafstudiosDot`/`Incogine` (see `CONTRIBUTING.md`). A "Powered by Incogine" startup splash is desired but is NOT license-mandated.
- **Trademark plan (not yet registered).** Once the business is registered, file an "Incogine" trademark and publish a short brand policy page (name/logo usage) separate from the license. Until then, no trademark enforcement.
- **Rejected alternatives — do not revisit without a new discussion with the maintainer:** MIT/Apache (modified core could be closed), GPL/AGPL (would force games open), custom source-available EULA with contribution-back-by-email clauses (unenforceable, kills community, rejected by corporate legal), upfront commercial fees (the $100/mo model was removed as "greed").

## Git Policy

**Never run git commands that write to the repository or alter history** — no commits, pushes, amends, rebases, resets, branch operations, stashes, cleans, or checkouts that modify files. Read-only commands (`git status`, `git diff`, `git log`) are fine. All repository changes stay in the working tree until the maintainer commits them. The only exception is moving a submodule working tree to a newer upstream release tag (see below) — still without committing or pushing.

## Submodules / Third-Party Deps (`reqs/`, `emsdk`)

Submodule checkouts (everything listed in `.gitmodules`: `emsdk/`, `reqs/SDL3_source`, `reqs/SDL3_ttf_source`, `reqs/SDL3_image_source`, `reqs/SDL3_mixer_source`, and any future `reqs/*` entry) plus the gitignored prebuilt drops in `reqs/` (`SDL3`, `SDL3_ttf`, `SDL3_image`, `SDL3_mixer`, per `reqs/.gitignore`) are **read-only**:

- Never create, edit, or delete files inside them. Read them for reference; build against them via the existing `add_subdirectory(reqs/SDL3_*_source ...)` / prebuilt paths in `CMakeLists.txt`. Fix integration issues in our own code or CMake, never by patching upstream sources, headers, or build scripts.
- **Allowed: bump a submodule to a newer upstream release tag.** Run `git fetch --tags` then `git submodule update --remote --checkout <path>` (or `git checkout <release-tag>` inside that submodule) to test the latest stable tag on its tracked `release-*` branch from `.gitmodules` (e.g. SDL `release-3.4.x`, SDL_ttf `release-3.2.x`). Rebuild the affected platform(s) to verify. Do not retarget submodule URLs/branches, do not add/remove submodules, and do not commit or push — leave the moved gitlink in the working tree for the maintainer to review and commit.
- Windows prebuilt refreshes work the same way: re-extract the upstream release archive into `reqs/` instead of hand-editing files. If the fresh `SDL3_mixer` package still ships the header at the legacy `include/SDL_mixer.h` location, recreate the one-line `reqs/SDL3_mixer/include/SDL3_mixer/SDL_mixer.h` shim (see the Windows prebuilts note under Build Commands).

## Build Commands

All builds are driven by the top-level `CMakeLists.txt`. Standard patterns:

> **Prerequisite:** SDL3 + SDL3_ttf/SDL3_image/SDL3_mixer are vendored as git submodules under `reqs/` (Windows uses prebuilt libraries there instead). First clone:
> `git submodule update --init --recursive` (fetches SDL3 and the addons' vendored third-party sources, ~100 MB).

Assets are read from `src/assets/` at runtime (copied next to the binary by CMake; packaged by gradle/emcc on Android/Web). Add `-DICG_EMBED_ASSETS=ON` to compile fonts/audio into the binary for a single-file distribution.

### Windows (Visual Studio / MSVC + Ninja)
```
mkdir build
cd build
cmake ..          # uses CMakeSettings.json (x64-Debug, Ninja)
cmake --build .
```
Or via the VS Code task: `CMake: build` (runs `cmake --build build --target Incogine`).

The MSVC build forces `/utf-8`; MinGW/Clang add `-fexec-charset=UTF-8`. The Windows target name is the value of `<name>` in `src/project.xml` (currently `Incogine`).

> **Windows prebuilts note:** the current `reqs/SDL3_mixer` package ships its header at the legacy `include/SDL_mixer.h` location. The shim `reqs/SDL3_mixer/include/SDL3_mixer/SDL_mixer.h` (a one-line `#include <SDL_mixer.h>`) maps the canonical include; recreate it after re-extracting fresh prebuilts, or upgrade to a package with the `SDL3_mixer/` header layout.

### macOS / Linux
```
mkdir build && cd build && cmake .. && make
```
SDL3 and the addons compile from the `reqs/` submodules as static libraries; vendored freetype/harfbuzz/plutosvg (SDL3_ttf), libpng/jpeg/webp/tiff/jxl (SDL3_image) and ogg/vorbis/flac/opus/etc. (SDL3_mixer) are compiled in, so no system dev packages are required. `SDLIMAGE_AVIF` is disabled (the dav1d dependency needs nasm).

### iOS (Xcode)
```
mkdir build && cd build && cmake -G Xcode -DCMAKE_SYSTEM_NAME=iOS ..
```
Built as a static-SDL3 app bundle; `bundle/ios/` supplies the asset catalog, launch screen, and a generated `Info.plist`.

### WebAssembly (Emscripten)
The `emsdk` submodule is required. First-time setup:
```
cd emsdk
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
Web builds use `src/web/init.html` as the shell file and compile SDL3 + the addons from the same `reqs/` submodules as the desktop builds (Emscripten ships no SDL3 ports, so no `-sUSE_SDL` flags are used).

### Android (Gradle)
Packaging is driven by the vendored SDL3 `android-project/` gradle template. Requirements: Android SDK + NDK 28.2.13676358 (as pinned in `android-project/app/build.gradle`).
```
cd android-project
./gradlew assembleDebug          # APK in app/build/outputs/apk/debug/
```
`app/jni/CMakeLists.txt` is a thin adapter that points Gradle's CMake invocation at the repository root `CMakeLists.txt`, which builds SDL3 + addons as shared libraries (`libSDL3.so`, `libSDL3_ttf.so`, ...) and the game as the `main` shared library loaded by SDLActivity. `SDLActivity.getLibraries()` (in `android-project/app/src/main/java/org/libsdl/app/SDLActivity.java`) lists the `.so` files to load. `APP_PLATFORM` is android-21, ABI arm64-v8a.

## Code-Generation Steps

Assets are loaded from disk at runtime by default (see **AssetManager** below). Only when the build is configured with `-DICG_EMBED_ASSETS=ON` (single-file distribution) do the following generators run, producing C arrays compiled into the binary:

- `src/parser/ttfparse_main.py` and `src/parser/ttfparse.py` convert TTF files in `src/assets/fonts/` to C headers generated into `<build>/generated_fonts/`.
- `src/parser/audioparse.py` converts audio files in `src/assets/audio/` into C arrays generated into `<build>/generated_audio/`.
- `src/parser/svgparse.py` converts any root-level `.svg` into a `_svgdata.h` header (demo code, generated into the build dir).
- `src/parser/ios_infoplist_gen.py` generates the iOS `Info.plist` from `project.xml` fields.
- `src/parser/requirements.txt` lists Python deps for the parsers.

All generated files land in the build dir — nothing is written into the source tree.

## High-Level Architecture

### Main loop (`src/main.cpp`)
Creates a singleton `Engine`, runs `Init()` → `Events/Update/Render` while `running()` → `Cleanup()`. Recognized CLI flags: `-dev`, `-debug`, `--skipSplash`.

### `Engine` (singleton) — `src/core/engine/engine.h/.cpp`
Owns: SDL window, OpenGL context (via SDL3 GL), TTF + mixer init, the `SceneManager`, FPS/dev-mode overlay, and window-size bookkeeping. The default window is 1280×720 (`SCREEN_WIDTH/HEIGHT`), clamped to 16:9 on resize with a 1280×720 minimum. F11 toggles fullscreen. Returns the SDL event of the current frame via `GetEventProvider()`. Mobile (Android/iOS) and Web windows are always fullscreen and OS-controlled: resize events are accepted as-is (no 16:9 clamp / `SDL_SetWindowSize`), and `ToggleFullscreen()` is a no-op there.

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

`src/project/` contains a redirect `Script` class (now points to the scripting system in `src/core/components/script/`) and `pausemenu/` (a real `PauseMenu` overlay used by `GameScene`).

### Object / Component model — `src/core/objects/`, `src/core/components/`
- `Object` carries `Position`/`Scale`/`Rotation`/`Color` POD structs and a `std::vector<std::unique_ptr<Component>>`. Subtypes: `Cube` (no rendering yet), `Square` (renders with its `Sprite` component).
- `Component` is a polymorphic base with virtual lifecycle methods (`Start`, `Update`, `OnDestroy`). Existing components are `Transform`, `Sprite` (color only), and `ScriptComponent` (gameplay scripting).
- `Object::Render()` is currently a no-op; rendering is implemented per concrete subclass (e.g. `Square::Render`).
- `Object::startScripts()` / `updateScripts()` / `destroyScripts()` iterate components and call lifecycle on `ScriptComponent` instances.

### Scripting — `src/core/components/script/`, `src/core/scripting/`, `src/scripts/`
- `ScriptComponent` attaches a script (C# or Kodo) to an `Object`. Holds a `ScriptHandler` — abstract interface with `Start`/`Update`/`OnDestroy`.
- `CSriptHandler` (in `src/core/scripting/csharp/`) wraps the .NET 10 CoreCLR runtime via `nethost`/`hostfxr`. The C# managed assembly (`Incogine.dll`) is built by `dotnet build` from `src/scripts/csharp/Incogine/`.
- `KodoScriptHandler` (in `src/core/scripting/kodo/`) is a placeholder — logs a warning, no-op.
- Script files: C# in `src/scripts/csharp/`, Kodo in `src/scripts/kodo/`.
- CMake options: `ICG_SCRIPTING_CSHARP` (ON by default, requires .NET 10 SDK), `ICG_SCRIPTING_KODO` (ON by default, stub).
- The Kodo language syntax is being designed in `src/scripts/kodo/syntax-reference.md`.

### Assets / subsystems — `src/core/assets/`, `src/core/fonts/`
- **AssetManager** (`assetmanager.h/.cpp`) — the single entry point for all asset IO: `Open(path) -> SDL_IOStream*` and `Exists(path)`. Resolution order: disk first, embedded fallback (only when compiled with `ICG_EMBED_ASSETS=ON`). On desktop/iOS the disk root is `<exe-or-bundle>/assets/` (copied by CMake post-build); on Android it's the APK's `assets/` directory (via `SDL_IOFromFile` + AAssetManager, wired in `android-project/app/build.gradle` through `assets.srcDirs`); on Web it's the emscripten virtual filesystem (`--preload-file src/assets@assets` → `Incogine.data`). Asset paths are canonical, e.g. `"fonts/main_font.ttf"`, `"audio/testbgm.ogg"`. Adding a new asset only requires placing the file in `src/assets/` (plus a registry entry in `assetmanager.cpp` when embedded mode should cover it).
- `image/Image` — SDL_texture-backed image loader; `load(path)` resolves through the AssetManager (still unused by scenes; note it renders via `SDL_Renderer`, not the GL pipeline).
- `audio/Audio` — `SDL3_mixer` (`MIX_*`) wrapper. `Audio(path)` resolves through the AssetManager (disk first, embedded fallback) and loads via `MIX_LoadAudio_IO`. `play(loop)` accepts `-1` for infinite looping.
- `fonts/Font` — OpenGL-texture-rendered text via SDL3_ttf. Each `Font` instance owns a `TTF_Font` and a GL texture that is rebuilt on text/color/scale changes. Two loaders: `setFontFile(path, pts)` (AssetManager-resolved, the normal path) and `setFont(data, size, pts)` (raw memory, used by the embedded fallback). Used everywhere menus appear.

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
| C# scripts | `src/scripts/csharp/`, base class in `src/scripts/csharp/Incogine/ScriptBehaviour.cs` |
| Kodo scripts | `src/scripts/kodo/`, syntax reference in `src/scripts/kodo/syntax-reference.md` |
| Scripting engine core | `src/core/components/script/` (ScriptComponent) and `src/core/scripting/` (handlers) |
| Save file location | `src/core/engine/savedata/savedata.cpp` (`SDL_GetPrefPath` based) |
| Platform-specific code | `src/core/platforms/platforms.h` and the per-`PLATFORM STREQUAL` blocks in `CMakeLists.txt` |
| Licensing questions / project direction | The "Licensing & Project Direction" section above |
