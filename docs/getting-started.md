---
title: Getting Started
description: Prerequisites and repository setup for building Incogine.
sidebar_position: 2
tags: [setup, prerequisites]
---

# Getting Started

This page covers the prerequisites and initial repository setup. Choose a platform-specific guide for the actual build:

- [Windows](./build-windows.md)
- [macOS / Linux](./build-macos-linux.md)
- [iOS](./build-ios.md)
- [WebAssembly](./build-web.md)
- [Android](./build-android.md)

## Dependencies

- **CMake**
- **Python 3** (3.11+)
- **C++17**
- **SDL2** (legacy) / **SDL3** (current)

### SDL3 submodules

SDL3, SDL3_ttf, SDL3_image, and SDL3_mixer are vendored as git submodules under `reqs/` (Windows uses prebuilt libraries there instead). On first clone, fetch them:

```bash
git submodule update --init --recursive
```

This downloads SDL3 and the addons' vendored third-party sources (~100 MB). SDL3 and the addons compile from these submodules as static libraries on macOS/Linux; the vendored freetype/harfbuzz/plutosvg (SDL3_ttf), libpng/jpeg/webp/tiff/jxl (SDL3_image), and ogg/vorbis/flac/opus/etc. (SDL3_mixer) are all compiled in, so no system dev packages are required.

:::note
`SDLIMAGE_AVIF` is disabled because the `dav1d` dependency requires `nasm`.
:::

## Project identity

The engine reads your project's identity (window name, executable name, bundle ID, copyright) from `src/project.xml`. The `name` key becomes the executable filename, so it must be a **single token with no spaces**:

```xml
<name>Incogine</name>       <!-- OK -->
<name>Incogine Engine</name> <!-- Not OK -->
```

See [Project XML](./project-xml.md) for details.

## Assets

Assets are read from `src/assets/` at runtime (copied next to the binary by CMake; packaged by Gradle/emcc on Android/Web). See [Assets](./assets.md) for the full picture.
