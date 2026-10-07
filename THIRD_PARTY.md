# Third-Party Software

Incogine is built on the work of other projects. Full license texts ship with each dependency in its own source tree or install directory.

Incogine's own source is licensed under **MPL-2.0** (see `LICENSE`). The components below are used under their own terms; none of them are relicensed by Incogine, and none of them change Incogine's MPL-2.0 terms for the engine or for softwares built on it.

---

## SDL3

Made **Incogine possible.** Provides the window, input, audio device, and the cross-platform foundation for the desktop, mobile, and Web builds. Incogine is built directly on SDL3 — there is no abstraction layer between them.

Used by: the `Incogine` runtime and all Studio tools.

Website: https://www.libsdl.org  
License: zlib License  
Copyright (C) 1997-2025 Sam Lantinga

This software is provided 'as-is', without any express or implied warranty of any kind. Use at your own risk.

### SDL3_ttf, SDL3_image, SDL3_mixer

The official SDL3 add-ons, vendored as submodules under `reqs/`:

| Add-on | Used for |
|---|---|
| **SDL3_ttf** | Font rendering (every menu and text in the engine) |
| **SDL3_image** | Image decoding, with its own vendored libpng/libjpeg/libwebp/libtiff/libjxl |
| **SDL3_mixer** | Audio playback, with its own vendored ogg/vorbis/flac/opus codecs |

Website: https://www.libsdl.org  
License: zlib License

---

## Qt 6

Made **Incogine Studio and Incogine Animator possible.** Provides the widget toolkit, file dialogs, settings persistence, image handling, local sockets, and the multimedia stack behind Studio's audio preview.

Used by: `IncogineStudio`, `IncogineAnimator`, and `IncogineIncoba` (partly).

Qt is **developer tooling only**. It is linked into the Studio and Animator executables and is never linked into the `Incogine` game runtime, so shipped games do not carry Qt or any of its licensing obligations. `src/studio/` is explicitly removed from the game executable's source globs to guarantee this.

Qt 6 is licensed under the **GNU Lesser General Public License v3** (or commercially). Incogine Studio and Incogine Animator are developer tools that are not distributed with games, which is what keeps the LGPLv3 terms satisfied without a commercial Qt license.

Website: https://www.qt.io  
License: https://www.qt.io/licensing/

---

## glad

Generated OpenGL function loader. Incogine resolves all GL entry points at
runtime through it, which is how one code path serves desktop OpenGL 3.3 core
and OpenGL ES 3.0 (Android, iOS, Web) without a compatibility layer.

Used by: the `Incogine` runtime.

Website: https://github.com/Dav1dde/glad  
License: `(WTFPL OR CC0-1.0) AND Apache-2.0` (as generated in `gl.c`)

---

## miniz

Single-purpose DEFLATE compressor/decompressor (zlib-compatible streams).
Used for exactly one thing: compressing `.incoanim` v2 container chunks, so
animation files stay small without a heavyweight dependency. The game runtime
decompresses through the same code, which is why this lives in `src/core`
rather than Studio: it must stay Qt-free.

Vendored at `src/core/thirdparty/miniz/` (deflate subset only: `miniz.h`,
`miniz.c`, `miniz_common.h`, `miniz_tdef.*`, `miniz_tinfl.*`, plus a local
`miniz_export.h` stub standing in for the file upstream's CMake generates).
Only `mz_compress2` / `mz_uncompress` are used.

Used by: `IncogineAnim` (container read/write, runtime included).

Website: https://github.com/richgel999/miniz  
License: MIT (Copyright 2013-2014 RAD Game Tools and Valve Software, Copyright
2010-2014 Rich Geldreich and Tenacious Software LLC; full text in
`src/core/thirdparty/miniz/LICENSE`)

---

## FFmpeg

Vendored as a source submodule at `reqs/ffmpeg` for **video export** from Incogine Animator (image/video sequences, MP4/MOV/AVI).

> **Status: not built or linked yet.** The export pipeline pipes rendered frames
> to a user-configured `ffmpeg` binary rather than linking `libav`. That choice
> is deliberate: FFmpeg is **LGPL-2.1-or-later** by default, but its
> `--enable-gpl` components (x264, x265, vpx) make it **GPL-2.0-or-later**,
> and linking that into the engine would place GPL obligations on every shipped
> Incogine game — which would break the promise that **game code built on
> Incogine may stay closed source**. Invoking the binary as a separate process
> keeps Incogine and its games free of FFmpeg's terms entirely.
>
> Building it also needs yasm/nasm on Windows and FFmpeg's own `configure`
> system rather than CMake, so the CI cost across Windows, macOS, and Linux is
> still an open decision.

License: **LGPL v2.1 or later** (see `reqs/ffmpeg/COPYING.LGPLv2.1`); optional
components under **GPL v2 or later** (see `reqs/ffmpeg/COPYING.GPLv2`)  
Website: https://ffmpeg.org/

---

## .NET (CoreCLR hosting)

`nethost` / `hostfxr` headers and libraries for hosting the .NET 10 runtime, used
by the C# scripting system. Incogine uses the hosting *APIs* to load
`Incogine.dll`; the managed assembly is built separately with `dotnet build`.

Used by: the `Incogine` runtime when `ICG_SCRIPTING_CSHARP=ON` (default).

Website: https://dotnet.microsoft.com/  
License: MIT  
Copyright (c) .NET Foundation and Contributors

---

## Kodo

Incogine's own placeholder scripting language — not a third-party dependency,
listed here only because it sits alongside the items above. The parser,
interpreter, and language notes live in `src/core/scripting/kodo/` and
`src/scripts/kodo/`, and are part of Incogine under MPL-2.0.

---

## Vendored build-time dependencies

`reqs/SDL3_source`, `reqs/SDL3_ttf_source`, `reqs/SDL3_image_source`, and
`reqs/SDL3_mixer_source` are submodules of the SDL projects above. Building them
from source also compiles their own vendored third-party copies of
freetype/harfbuzz/plutosvg, libpng/libjpeg/libwebp/libtiff/libjxl, and
ogg/vorbis/flac/opus — each under its own permissive license, documented in the
corresponding SDL3 add-on's repository.
