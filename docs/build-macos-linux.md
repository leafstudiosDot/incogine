---
title: Build on macOS / Linux
description: Building Incogine on macOS or Linux with CMake and make.
sidebar_position: 4
tags: [build, macos, linux]
---

# Build on macOS / Linux

```bash
mkdir build
cd build
cmake ..
make
```

SDL3 and the addons compile from the `reqs/` submodules as static libraries. The vendored freetype/harfbuzz/plutosvg (SDL3_ttf), libpng/jpeg/webp/tiff/jxl (SDL3_image), and ogg/vorbis/flac/opus/etc. (SDL3_mixer) are all compiled in, so no system dev packages are required.

:::note
`SDLIMAGE_AVIF` is disabled (the `dav1d` dependency needs `nasm`).
:::

## First-time setup

If you haven't fetched the submodules yet, run:

```bash
git submodule update --init --recursive
```

See [Getting Started](./getting-started.md) for the full prerequisites.
