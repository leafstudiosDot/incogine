---
title: Build on Windows
description: Building Incogine on Windows with Visual Studio or a terminal compiler.
sidebar_position: 3
tags: [build, windows]
---

# Build on Windows

> Recommended: Visual Studio Code with the CMake Tools extension.

## Visual Studio IDE

1. Run the following commands in the project root directory:

```bash
mkdir build
cd build
cmake ..          # uses CMakeSettings.json (x64-Debug, Ninja)
```

2. Open `build/Incogine.sln` with the Visual Studio IDE (or Visual Studio 2022), then build.

Alternatively, from the terminal:

```bash
cmake --build .
```

Or use the VS Code task `CMake: build` (runs `cmake --build build --target Incogine`).

## Terminal compiler (MinGW / Ninja)

```bash
mkdir build
cd build
cmake ..          # Ninja is the default
make              # or: ninja
```

## Notes

- The MSVC build forces `/utf-8`; MinGW/Clang add `-fexec-charset=UTF-8`.
- The Windows target name is the value of `<name>` in `src/project.xml` (currently `Incogine`).
- The Windows build copies the static `Puroko.lib` next to the executable as a post-build step — don't be surprised by its presence.
- Every game build ships `src/assets/` as `.incoba` bundles + `index.incobai` **only** (executable's `assets/` folder) via the headless packer — no Studio/Qt needed. Use `-DICG_USE_INCOBA=OFF` for the direct-disk flow (loose files only). See [Incogine Studio](./studio.md).
