---
title: Scenes
description: The scene system and the game scenes that ship with Incogine.
sidebar_position: 10
tags: [scenes, scene-manager]
---

# Scenes

The scene system lives in `src/core/scenes/scenes.h/.cpp`.

## The `Scene` base class

`Scene` is a polymorphic base with these virtual methods:

- `Prestart()` — optional
- `Start()`
- `Update()`
- `Render()`
- `Events(const SDL_Event&)`

Scenes are owned exclusively by the `SceneManager`. `SetScene` deletes the previous scene and calls `Prestart → Start`.

:::note
A scene **constructor runs before `Start()`**. Most of the codebase does font/audio/scene-owned-resource initialization in the constructor and leaves `Start()` empty — `Start()` is the "everything is now wired up" callback.
:::

## Shipping scenes — `src/scenes/`

| Scene | Description |
|-------|-------------|
| `splash/` | Animated "Powered by Incogine" intro using cubic-bezier easing, then `SetScene(new MainScene())`. |
| `MainScene` | Top-level menu (New Game / Load Game / Settings / Credits / Exit), arrow/WASD navigation, Enter to select. |
| `game/GameScene` | Example play scene; instantiates a `PauseMenu` and plays a background audio track. Currently no real game objects. |
| `settings/SettingsScene` | Menu with sub-menu navigation backed by a static `SettingsMenu` vector of `MenuItem { name, subItems }`. Each sub-item carries a `std::function<void()>` action. |
| `credits/Credits` | Credits scene (currently a stub). |

## The `Puroko` static library

`CMakeLists.txt` builds `src/project/*.cpp|h` and `src/scenes/**` (except `splash/` and `settings/`) into a **static** library named `Puroko`. The final executable (`Incogine`, from `<name>`) links `Puroko` plus the rest of `src/`.

:::note
**Puroko is the game-project layer**, not part of the engine core. It exists as a reference example of any project someone can make on Incogine — your own game would replace/parallel `Puroko` with your own `src/project/` and `src/scenes/` code, while the engine core (`src/core/`) stays untouched.
:::

:::warning
The CMake glob **`src/scenes/splash/*` and `src/scenes/settings/*` are explicitly removed** from the `Puroko` static library, so they only link into the executable. If you add a new subfolder that should be part of `Puroko`, update the `list(REMOVE_ITEM ...)` in `CMakeLists.txt`.
:::

In practice, `src/project/` currently contains a placeholder `Script` class (empty `Start`/`Update`) and `pausemenu/` (a real `PauseMenu` overlay used by `GameScene`). `pausemenu/` lives under `src/project/` but is included via the `src/scenes/**` glob.

## Adding a new scene

Add your scene to `src/scenes/` and instantiate it via:

```cpp
Engine::Instance(0, nullptr)->SetScene(new MyScene());
```

The first scene shown at startup is selected in `Engine::Init()` in `src/core/engine/engine.cpp`.
