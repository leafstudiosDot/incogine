---
title: Introduction
description: What Incogine is, its capabilities, and how it is licensed.
sidebar_position: 1
tags: [overview]
---

# Introduction

**Incogine** is a 2D/3D-capable C++ game engine (and reference game) by **leafstudiosDot**. It is distributed under the **Mozilla Public License 2.0 (MPL-2.0)**.

The repository contains:

- The **engine core** — a C++ engine with 2D/3D rendering, a scene system, an asset manager, audio/font support, and **scripting** (C# via .NET 10, Kodo scripting language).
- A **sample game** (`Puroko`) built on top of the engine, plus the example play scene (`GameScene`).
- A **CMake superbuild** that glues the engine, the game, and the vendored third-party libraries together.

## Two builds in one repo

- **`Incogine`** is the **engine core** build.
- **`Puroko`** is a **game-project** build — a reference example of any project someone can make on Incogine.

In other words, `Puroko` plays the role your own game's project layer would play: `src/project/`, `src/scenes/`, and `src/project.xml` are the "game" parts, while `src/core/` and `CMakeLists.txt` are the engine. To start your own project, model it on the Puroko layer (see [Project XML](./project-xml.md) and [Scenes](./scenes.md)).

Incogine can target multiple platforms from a single codebase:

- Windows
- macOS / Linux
- iOS
- WebAssembly (Emscripten)
- Android (Gradle)

## Versioning

The engine version lives in `src/core/engine/version.h` and is mirrored into `src/project.xml` (the `<incogine_version>` key). The CMake `VERSION_STRING` macro is what `main.cpp` prints at startup.

## License at a glance

- Games and projects built with Incogine may be **closed source** — game code is a "Larger Work" under MPL and can stay proprietary.
- **Modifications to the engine core must be shared under MPL-2.0.**
- Attribution: `leafstudiosDot` and `Incogine` must be retained in derivatives.
- Donations are optional and always welcome.

See the [License](./license.md) page for the full record.

## Next steps

- Read [Getting Started](./getting-started.md) to install dependencies and prepare the repository.
- Pick your target from the [Build](./build-windows.md) guides.
