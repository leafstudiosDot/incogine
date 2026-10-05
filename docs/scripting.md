---
title: Scripting
description: How to use C# and Kodo scripts with Incogine's ScriptComponent system.
sidebar_position: 11
tags: [scripting, csharp, kodo, scriptcomponent]
---

# Scripting

Incogine supports two scripting languages through the `ScriptComponent` system:

- **C#** — full .NET 10 runtime integration via CoreCLR
- **Kodo** — Incogine's custom scripting language (MVP interpreter: lexer, parser, tree-walk runtime)

Scripts are attached to `Object` instances via C++ code, similar to Unity's `MonoBehaviour`.

## How It Works

```
┌─────────────┐
│   Object     │
├─────────────┤
│ Transform    │
│ Sprite       │
│ ScriptComponent ──→ ScriptHandler (C# or Kodo)
└─────────────┘
```

Each `ScriptComponent` holds a `ScriptHandler` — an abstract interface with two implementations:

| Handler | Language | Status |
|---------|----------|--------|
| `CSriptHandler` | C# | Functional (requires .NET 10 SDK at build time) |
| `KodoScriptHandler` | Kodo | MVP interpreter (errors logged with file:line; failing `update()` disables the script) |

## Lifecycle

Scripts receive these callbacks from the engine:

| Method | When | Frequency |
|--------|------|-----------|
| `Start()` | First frame after the script is attached | Once |
| `Update()` | Every frame | Every frame |
| `OnDestroy()` | When the script or its owner Object is destroyed | Once |

## Attaching Scripts in C++

```cpp
#include "core/components/script/scriptcomponent.h"
#include "core/components/script/scripthandler.h"

// C# script
auto obj = new Object("Player", Position(0,0,0), Scale(1,1,1), Rotation(0,0,0));
obj->addComponent(std::make_unique<ScriptComponent>(
    obj, "scripts/csharp/PlayerController.cs", ScriptLanguage::CSharp));

// Kodo script
auto obj2 = new Object("Enemy", Position(100,0,0), Scale(1,1,1), Rotation(0,0,0));
obj2->addComponent(std::make_unique<ScriptComponent>(
    obj2, "scripts/kodo/enemy.kodo", ScriptLanguage::Kodo));
```

## Scene Integration

Scenes are responsible for managing their Objects and calling script lifecycle methods:

```cpp
void MyScene::Start() {
    player = new Object("Player", Position(0,0,0), Scale(1,1,1), Rotation(0,0,0));
    player->addComponent(std::make_unique<ScriptComponent>(
        player, "scripts/csharp/PlayerController.cs", ScriptLanguage::CSharp));
    player->startScripts();  // calls Start() on all ScriptComponents
}

void MyScene::Update() {
    player->updateScripts();  // calls Update() on all ScriptComponents
}

MyScene::~MyScene() {
    delete player;  // Object destructor calls destroyScripts() → OnDestroy()
}
```

## Script File Locations

| Language | Directory | Extension |
|----------|-----------|-----------|
| C# | `src/scripts/csharp/` | `.cs` |
| Kodo | `src/scripts/kodo/` | `.kodo` |

## Build Configuration

| CMake Option | Default | Description |
|-------------|---------|-------------|
| `ICG_SCRIPTING_CSHARP` | ON | Enable C# scripting (requires .NET 10 SDK) |
| `ICG_SCRIPTING_KODO` | ON | Enable Kodo scripting (MVP interpreter) |

When `ICG_SCRIPTING_CSHARP=ON`, the build compiles the managed assembly `Incogine.dll`
from `src/scripts/csharp/Incogine/` via `dotnet build` and copies it next to the
executable along with `Incogine.runtimeconfig.json`.

## Visual Studio Solution Structure

The scripting system appears in the VS Solution Explorer as:

```
Incogine (exe)
├── src/core/components/script/    ← ScriptComponent, ScriptHandler
├── src/core/scripting/csharp/     ← C# host + handler
├── src/core/scripting/kodo/       ← Kodo interpreter (lexer/parser/runtime)
Kodo (lib)                         ← Kodo static library
IncogineScripting (custom)         ← C# managed assembly build
Scripts/
├── CSharp/Incogine/               ← C# managed assembly source
├── CSharp/Examples/               ← Example C# scripts
├── Kodo/                          ← Kodo scripts + docs
└── Kodo/Examples/                 ← Example Kodo scripts
```

## See Also

- [C# Scripting](./csharp-scripting.md) — writing C# scripts
- [Kodo](./kodo/intro.md) — the Kodo language (MVP interpreter)
