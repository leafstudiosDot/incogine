---
title: C# Scripting
description: How to write C# scripts for Incogine using the .NET 10 runtime.
sidebar_position: 12
tags: [scripting, csharp, dotnet]
---

# C# Scripting

Incogine embeds the .NET 10 runtime (CoreCLR) to run C# scripts on game Objects.
Scripts extend `ScriptBehaviour` and override lifecycle methods.

## Prerequisites

- [.NET 10 SDK](https://dotnet.microsoft.com/download) installed
- CMake build configured with `-DICG_SCRIPTING_CSHARP=ON` (default)

## Writing a Script

Create a `.cs` file in `src/scripts/csharp/`:

```csharp
using Incogine;

public class PlayerController : ScriptBehaviour
{
    private float speed = 200.0f;

    public override void Start()
    {
        Object.SetPosition(100.0f, 300.0f, 0.0f);
    }

    public override void Update()
    {
        var pos = Object.GetPosition();
        pos.X += speed * 0.016f;
        Object.SetPosition(pos);
    }
}
```

## ScriptBehaviour

The base class for all Incogine C# scripts:

```csharp
namespace Incogine
{
    public abstract class ScriptBehaviour
    {
        public ObjectHandle Object { get; internal set; }

        public virtual void Start() { }
        public virtual void Update() { }
        public virtual void OnDestroy() { }
    }
}
```

## ObjectHandle API

`ObjectHandle` provides access to the native Object's properties:

| Method | Description |
|--------|-------------|
| `SetPosition(float x, float y, float z)` | Set world position |
| `GetPosition()` → `Vector3` | Get world position |
| `SetScale(float x, float y, float z)` | Set scale |
| `GetScale()` → `Vector3` | Get scale |
| `SetRotation(float x, float y, float z)` | Set rotation |
| `GetRotation()` → `Vector3` | Get rotation |
| `GetName()` → `string` | Get the Object's name |

## Vector3

Simple struct used by the scripting API:

```csharp
public struct Vector3
{
    public float X, Y, Z;
    public Vector3(float x, float y, float z) { ... }
}
```

## Attaching a C# Script

From C++ code (typically in a Scene's `Start()` method):

```cpp
#include "core/components/script/scriptcomponent.h"

auto obj = new Object("MyObject", Position(0,0,0), Scale(1,1,1), Rotation(0,0,0));
obj->addComponent(std::make_unique<ScriptComponent>(
    obj, "scripts/csharp/PlayerController.cs", ScriptLanguage::CSharp));
```

## How It Works

1. At build time, `dotnet build` compiles `src/scripts/csharp/Incogine/Incogine.csproj`
   into `Incogine.dll`, placed next to the executable.
2. At runtime, when a `ScriptComponent` with `ScriptLanguage::CSharp` is first started,
   the engine's `CSriptHost` initializes CoreCLR via `hostfxr`.
3. The `CSriptHandler` loads the user's script assembly and resolves the `Start`,
   `Update`, and `OnDestroy` method pointers.
4. Each frame, `Object::updateScripts()` calls `CSriptHandler::Update()` which
   invokes the managed `Update()` method.

## Managed Assembly Structure

```
src/scripts/csharp/Incogine/
    Incogine.csproj              ← .NET 10 class library
    Incogine.runtimeconfig.json  ← runtime configuration
    ScriptBehaviour.cs           ← base class for user scripts
    ObjectHandle.cs              ← managed wrapper for native Object
    ObjectInterop.cs             ← P/Invoke bindings to engine
```

## Files

| File | Purpose |
|------|---------|
| `src/core/scripting/csharp/csharphost.h/.cpp` | CoreCLR host initialization |
| `src/core/scripting/csharp/cshandlerruntime.h/.cpp` | Per-script handler |
| `src/scripts/csharp/Incogine/` | Managed assembly source |
| `src/scripts/csharp/examples/` | Example scripts |
