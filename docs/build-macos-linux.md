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

SDL3 and the addons compile from the `reqs/` submodules as static libraries. The vendored freetype/harfbuzz/plutosvg (SDL3_ttf), libpng/jpeg/webp/tiff/jxl (SDL3_image), and ogg/vorbis/flac/opus/etc. (SDL3_mixer) are all compiled in, so the **addons** need no system dev packages.

:::note
macOS builds **arm64-only** by default, since x64 macOS binaries are being
discontinued. For an Intel build, configure with
`cmake -DCMAKE_OSX_ARCHITECTURES=x86_64 ..`.
:::

:::caution
The SDL3 **core** does need development libraries on Linux — it refuses to
configure without X11 or Wayland. Install the list from the
[SDL3 README for Linux](https://wiki.libsdl.org/SDL3/README-linux) (see
[Getting Started](./getting-started.md#sdl3-submodules) for the Debian/Ubuntu
one-liner). macOS needs none of this.
:::

:::note
`SDLIMAGE_AVIF` is disabled (the `dav1d` dependency needs `nasm`).
:::

## First-time setup

If you haven't fetched the submodules yet, run:

```bash
git submodule update --init --recursive
```

If that leaves `reqs/SDL3_source` empty, the SDL3 entries are declared in
`.gitmodules` but not yet committed as gitlinks — use
`bash cmake/ci/fetch-sources.sh` instead (same repositories, same branches).

See [Getting Started](./getting-started.md) for the full prerequisites.
