---
title: Continuous Integration
description: How Incogine is built on GitHub Actions for Windows, macOS and Linux.
sidebar_position: 7
tags: [build, ci, github-actions, windows, macos, linux]
---

# Continuous Integration

`.github/workflows/ci.yml` configures and builds the engine on **Windows
(MSVC)**, **macOS** and **Linux (Ubuntu)** for every push and pull request, and
uploads the resulting runtime output as a workflow artifact.

The workflow runs the exact same top-level `CMakeLists.txt` as a local build —
no CI-only code paths — with the default options: Kodo and C# scripting enabled,
`.incoba` asset bundling enabled, and the Studio sources compiled with only its
optional Qt shell skipped.

## What each platform does

| Step | Linux | macOS | Windows |
|---|---|---|---|
| Dependencies | `build-essential`, `cmake`, `git`, `python3`, `ccache`, `ca-certificates` + the SDL3 X11/GL/audio dev packages | `ccache` (via Homebrew) | Visual Studio 2022 (preinstalled) |
| SDL3 | built from the `reqs/*_source` trees | built from the `reqs/*_source` trees | prebuilt import libraries in `reqs/SDL3*` |
| .NET | `actions/setup-dotnet` with `10.0.x` | same | same |
| Architecture | x86-64 | **arm64 only** (`-DCMAKE_OSX_ARCHITECTURES=arm64`) | x86-64 |
| Generator | default (Unix Makefiles) | default (Unix Makefiles) | default (Visual Studio) |

SDL3 and the addons are compiled from source on Linux/macOS, which is a few
thousand translation units on 3-4 CI cores, so `ccache` is installed and its
cache directory (`~/.cache/ccache`, pinned via `CCACHE_DIR`) is persisted between
runs.

:::caution
macOS is built **arm64-only**, because x64 macOS binaries are being
discontinued and only an arm64 build keeps running on current and future macOS.
The architecture is passed explicitly rather than inherited from the runner, and
a `Verify macOS architecture` step asserts it on the produced binary with
`lipo -archs` (and `file`) after the build — so a silently universal or x64
binary fails the job instead of shipping. To build for Intel locally, configure
with an explicit `-DCMAKE_OSX_ARCHITECTURES=x86_64`; `CMakeLists.txt` follows
that override when picking the .NET hosting pack.
:::

:::note
The engine requires **CMake >= 3.26.3** (see `cmake_minimum_required` in the
top-level `CMakeLists.txt`) and **Python 3** (`find_package(Python3 REQUIRED)`,
used by the asset/font/audio parsers). The workflow checks the CMake version up
front so a too-old runner fails with a readable message.
:::

:::caution
SDL3 hard-errors at configure time on Linux without X11 **or** Wayland
development libraries ("SDL could not find X11 or Wayland development
libraries on your system"). Unlike the SDL3 addons — whose codecs are vendored
in the `reqs/*_source` trees and therefore need nothing — the SDL3 core itself
needs those dev packages. The workflow installs the X11/GLX/EGL/audio set listed
in the SDL3 README for Linux (`libx11-dev`, `libxext-dev`, `libxrandr-dev`,
`libxcursor-dev`, `libxi-dev`, `libxfixes-dev`, `libxss-dev`, `libxtst-dev`,
`libxkbcommon-dev`, `libxkbcommon-x11-dev`, `libgl1-mesa-dev`,
`libegl1-mesa-dev`, `libdrm-dev`, `libgbm-dev`, `libasound2-dev`,
`libpulse-dev`, `libudev-dev`, `libdbus-1-dev`), plus `ca-certificates` so the
HTTPS clones of the SDL repositories verify. Each missing X11 extension is its
own `FATAL_ERROR` from SDL3 (`Couldn't find dependency package for XTEST ...`),
so the list has to be complete rather than "the obvious few".
:::

## Dependency fetching

The SDL3 sources are listed in `.gitmodules` but are **not committed as
gitlinks**, so `actions/checkout` with `submodules: true` would fetch nothing
useful while also pulling in the large, unused `emsdk` checkout. The workflow
therefore checks out without submodules and fetches dependencies itself:

- `cmake/ci/fetch-sources.sh` (Linux/macOS) reads `.gitmodules`, clones every
  `reqs/SDL3*_source` entry at the branch recorded there, and then runs
  `git submodule update --init --recursive` inside each clone. That recursive step
  is what pulls in the vendored freetype/harfbuzz/plutosvg (SDL3_ttf),
  libpng/jpeg/webp/tiff/jxl (SDL3_image) and ogg/vorbis/flac/opus/... (SDL3_mixer)
  sources — the reason no system dev packages are needed. `emsdk` is skipped
  unless `ICG_CI_FETCH_EMSDK=1` (Web builds only).
- `cmake/ci/fetch-prebuilts.ps1` (Windows) downloads the official
  `<lib>-devel-<version>-VC.zip` release archives from the upstream GitHub
  releases and unpacks them into `reqs/SDL3`, `reqs/SDL3_ttf`, `reqs/SDL3_image`
  and `reqs/SDL3_mixer`. Windows links against prebuilt import libraries rather
  than compiling SDL from source.

Both scripts read their versions out of `.gitmodules`, so bumping a branch there
(e.g. `branch = release-3.4.14`) bumps the dependency CI builds against — no
version numbers are duplicated in the workflow.

:::note
Once the SDL3 submodules are committed as gitlinks, the fetch scripts keep
working unchanged: they skip directories that are already present and only
refresh the vendored submodules.
:::

## Artifacts

Each run uploads the runtime output as `incogine-<platform>`:

- Linux: the `Incogine` binary, its `assets/` folder (`.incoba` bundles), the
  `Incogine.dll` managed assembly, `Incogine.runtimeconfig.json` and the
  `Incogine.sha256` sidecar Incogine Studio uses to bind a dev build.
- macOS: the `Incogine.app` bundle.
- Windows: the whole `build/Release` folder (executable, SDL3 DLLs,
  `nethost.dll`, `assets/`, static libraries).

Artifacts are retained for 14 days. They are build outputs for testing, not
releases — the project is distributed under MPL-2.0, and see
[License](./license.md) for what that means for redistribution.

## Running the same build locally

```bash
# Linux / macOS
bash cmake/ci/fetch-sources.sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel

# Windows (PowerShell)
./cmake/ci/fetch-prebuilts.ps1
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel
```

## Known limitations

- The Qt shell of Incogine Studio is never built in CI (no Qt on the runners);
  `IncogineStudioCore` and the `IncogineIncoba` packer are.
- Android (Gradle), iOS (Xcode) and WebAssembly (Emscripten) need their own
  SDKs and are not part of this workflow — see [Android](./build-android.md),
  [iOS](./build-ios.md) and [WebAssembly](./build-web.md) for the local flows.
- The macOS leg is the only one not reproducible on Linux/Windows hardware; it
  was validated by construction (explicit `-DCMAKE_OSX_ARCHITECTURES=arm64` plus
  the `lipo` assertion) rather than by a local build, since no macOS host was
  available.
