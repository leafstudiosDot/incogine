---
title: Architecture
description: High-level architecture of Incogine, including the main loop and the Engine singleton.
sidebar_position: 8
tags: [architecture, engine]
---

# Architecture

## Main loop — `src/main.cpp`

Creates a singleton `Engine`, then runs `Init()` → `Events/Update/Render` while `running()` → `Cleanup()`.

Recognized CLI flags:

| Flag | Effect |
|------|--------|
| `-dev` | Enables developer mode (extra `std::cerr` logging, dev-mode overlay) |
| `-debug` | Enables debug mode |
| `--skipSplash` | Skips the splash scene and goes straight to `MainScene` |

## The `Engine` singleton — `src/core/engine/engine.h/.cpp`

`Engine` owns:

- The SDL window
- The OpenGL context (via SDL3 GL)
- TTF + mixer initialization
- The `SceneManager`
- The FPS / dev-mode overlay
- Window-size bookkeeping

Key behaviors:

- The default window is 1280×720 (`SCREEN_WIDTH`/`SCREEN_HEIGHT`), clamped to 16:9 on resize with a 1280×720 minimum. `F11` toggles fullscreen.
- On mobile (Android/iOS) and Web, windows are always fullscreen and OS-controlled: resize events are accepted as-is (no 16:9 clamp / `SDL_SetWindowSize`), and `ToggleFullscreen()` is a no-op.
- Returns the current frame's SDL event via `GetEventProvider()`.

### Singleton access pattern

```cpp
Engine::Instance(argc, argv)   // caches and returns the same pointer
```

Many subsystems grab it with `Engine::Instance(0, nullptr)` for the logger and to query `inDevMode()`.

## Where the first scene comes from

`Engine::Init()` in `src/core/engine/engine.cpp` selects the first scene. See [Scenes](./scenes.md).

## Platform detection

`src/core/platforms/platforms.h` — `printPlatform()` selects a human-readable name from `__APPLE__`/`__ANDROID__`/`__ORBIS__`/`__PROSPERO__`/`_DURANGO`/`__XBOXONE__`/`__NX__`/`__Mira__` etc.

:::warning
PlayStation/Xbox/Switch/Mira are detected by macro but not actually supported in the current CMake build.
:::

## Directory map

| Area | Location |
|------|----------|
| Main loop | `src/main.cpp` |
| Engine singleton | `src/core/engine/` |
| Scene system | `src/core/scenes/` |
| Object/component model | `src/core/objects/`, `src/core/components/` |
| Scripting system | `src/core/components/script/`, `src/core/scripting/` |
| C# runtime host | `src/core/scripting/csharp/` |
| Kodo interpreter | `src/core/scripting/kodo/` |
| Script files (C#) | `src/scripts/csharp/` |
| Script files (Kodo) | `src/scripts/kodo/` |
| Assets / fonts / audio | `src/core/assets/`, `src/core/fonts/` |
| Save data | `src/core/engine/savedata/` |
| Platform layer | `src/core/platforms/` |
| Web shell | `src/web/init.html` |
