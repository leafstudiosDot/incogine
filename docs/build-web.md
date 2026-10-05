---
title: Build for WebAssembly
description: Building Incogine for WebAssembly with Emscripten.
sidebar_position: 6
tags: [build, web, wasm, emscripten]
---

# Build for WebAssembly

The `emsdk` submodule is required. Before your first WebAssembly build, set up the SDK:

```bash
cd emsdk
./emsdk update          # or: git pull if you have local changes within emsdk
./emsdk install latest
./emsdk activate latest
source ./emsdk_env.sh
```

Then build:

```bash
mkdir build
cd build
emcmake cmake ..
emmake make
```

## Notes

- Web builds use `src/web/init.html` as the shell file.
- SDL3 and the addons compile from the same `reqs/` submodules as the desktop builds (Emscripten ships no SDL3 ports, so no `-sUSE_SDL` flags are used).
- The final Emscripten build emits `Incogine.html` / `Incogine.js`.
