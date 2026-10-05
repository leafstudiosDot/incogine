---
title: C# Scripting
description: How to write C# scripts for Incogine using the .NET 10 runtime.
sidebar_position: 13
tags: [scripting, csharp, dotnet]
---

# C# Scripting

Incogine embeds the .NET 10 runtime (CoreCLR) to run C# scripts on game Objects.

> **Binding model:** the C++ `CSriptHandler` resolves **static**
> `[UnmanagedCallersOnly]` methods `Start`/`Update`/`OnDestroy` by type name
> (script file stem → type name), passing the owner `Object*` as `IntPtr`.
> `ScriptBehaviour`/`ObjectHandle` below describe the intended instance model
> (future work); for scripts that run today, use the static pattern.

## Prerequisites

- [.NET 10 SDK](https://dotnet.microsoft.com/download) installed
  (including the `Microsoft.NETCore.App.Host` pack that provides `nethost.h`).
  If the SDK is missing — or the hosting headers can't be located — the C++
  host compiles as a disabled stub so the engine still builds; C# scripts
  just won't run until the pack is installed.
- CMake build configured with `-DICG_SCRIPTING_CSHARP=ON` (default)

## Hosting Pack Lookup

`CMakeLists.txt` locates the native hosting bits by probing, per platform RID
(`win-x64`, `osx-arm64`, `linux-x64`), both the dotnet directory next to
`dotnet.exe` and the well-known install locations:

```
packs/Microsoft.NETCore.App.Host.<rid>/<version>/runtimes/<rid>/native/
```

On macOS the RID defaults to **`osx-arm64`** — x64 macOS binaries are being
discontinued, so desktop builds are arm64-only. Passing
`-DCMAKE_OSX_ARCHITECTURES=x86_64` switches to the `osx-x64` pack for a local
Intel cross-build; a universal `arm64;x86_64` still prefers arm64.

That directory holds **both** the headers (`nethost.h`, `hostfxr.h`,
`coreclr_delegates.h`) and the `nethost` import library. Several versions can
be installed side by side; candidates are sorted newest-first with a natural
compare (so `10.x` wins over `8.x`) to match the `net10.0` TFM in
`Incogine.runtimeconfig.json`. A `<version>/include` + `<version>/lib` split
layout is also probed as a fallback.

The library file is spelled differently per platform, and all three names are
recognised: `nethost.lib` (Windows), `libnethost.lib` (older Unix layouts) and
`libnethost.a` (current Unix packs). On Linux/macOS the static
`libnethost.a` is preferred over the shipped `libnethost.so`, so the game does
not gain a runtime `.so` it would have to ship — the same reasoning that forces
SDL3 and the addons to static. A successful configure reports:

```
-- C# scripting: nethost include dir: .../Microsoft.NETCore.App.Host.win-x64/10.0.12/runtimes/win-x64/native
-- C# scripting: nethost library: .../10.0.12/runtimes/win-x64/native/nethost.lib
-- C# scripting: staging nethost.dll: .../10.0.12/runtimes/win-x64/native/nethost.dll
```

On Windows, `nethost.lib` is an import library, so the executable gains a
load-time dependency on `nethost.dll`. The .NET install directory is not on
`PATH` by default, so CMake copies `nethost.dll` next to the game executable.

## Wide vs. narrow strings

`hostfxr` types every string parameter as `char_t`, which is `wchar_t` on Windows
and plain `char` on Linux/macOS. `csharphost.h` cannot include `hostfxr.h` (it
has to compile in stub mode), so it declares the platform choice itself as
`icg_hostfxr_char`, and `cshandlerruntime.cpp` builds its
assembly/type/method-name strings as `std::wstring` or `std::string` to match.
Getting this wrong compiles fine on Windows and fails elsewhere with
`cannot convert 'const char*' to 'const wchar_t*'`, which is why the desktop CI
workflow builds this path on all three platforms.

If the pack lives somewhere unusual, point CMake at it directly instead of
relying on the probe:

```bash
cmake .. -DNETHOST_INCLUDE_DIR=/path/to/native -DNETHOST_LIBRARY=/path/to/nethost.lib
```

:::note
Only object files that are actually reachable get linked in. If no
`ScriptComponent` with `ScriptLanguage::CSharp` is attached anywhere, the C#
host is dead-stripped from the executable and `nethost.dll` will not appear in
its import table — that is normal and unrelated to whether the pack was found.
:::


## Writing a Script (static — supported today)

Create a `.cs` file in `src/scripts/csharp/` (see `examples/StaticPlayer.cs`):

```csharp
using System;
using System.Runtime.InteropServices;
using Incogine;

public static class StaticPlayer
{
    [UnmanagedCallersOnly]
    public static void Start(IntPtr owner)
    {
        ObjectInterop.SetPosition(owner, 100.0f, 200.0f, 0.0f);
    }

    [UnmanagedCallersOnly]
    public static void Update(IntPtr owner)
    {
        float dt = (float)TimeApi.DeltaTime();
        var pos = ObjectInterop.GetPosition(owner);
        pos.X += 100.0f * dt;
        ObjectInterop.SetPosition(owner, pos.X, pos.Y, pos.Z);
    }

    [UnmanagedCallersOnly]
    public static void OnDestroy(IntPtr owner) { }
}
```

## ScriptBehaviour (instance model — future)

The intended base class for Incogine C# scripts (`ScriptBehaviour.cs`):

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

## Engine APIs (static scripts)

`EngineInterop.cs` wraps the native exports in
`src/core/scripting/csharp/csharpinterop.h`:

| Class | Members | Notes |
|-------|---------|-------|
| `TimeApi` | `DeltaTime()`, `Time()`, `FPS()` | Seconds (engine tracks ms internally) |
| `LogApi` | `Info/Warn/Error/Debug(string)` | → `cout`/`cerr`/`SDL_Log` |
| `SceneApi` | `SetScene(name)` | `"MainScene"`, `"GameScene"`, `"Splash"`, `"Settings"`, `"Credits"` |
| `InputApi` | `IsKeyDown`, `GetMousePosition`, `IsMouseButtonDown`, `IsGamepadButtonDown`, `GetGamepadAxis` | Key names are SDL scancodes (`"W"`, `"Space"`, `"Escape"`); mouse button 1/2/3 = left/middle/right |
| `SaveApi` | `Set/Get/Has/Remove/Clear/Save/Load` | One shared `SaveData` store (`scriptsave.dat`) for C# and Kodo — see `save-data.md` |
| `AudioHandle` | `Load(path)`, `Play(loop)`, `Stop()` | Asset-root-relative (e.g. `"audio/testbgm.ogg"`); loop -1 = infinite; no volume/seek yet |
| `ObjectApi` | `SetName`, `SetColor/GetColor`, `Find`, `Create`, `Destroy` | Color via `Sprite` component (or `Square`); `Find` returns `IntPtr.Zero` when absent |

`ObjectInterop` (`ObjectInterop.cs`) covers transform + name via `IntPtr` owner handles.

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
   into `Incogine.dll`, placed next to the executable. Managed debug info is
   embedded in the DLL (`DebugType=embedded`, so C# debugging still works)
   instead of a sidecar `Incogine.pdb` — that filename belongs to the native
   linker (`Incogine.exe`), and sharing it broke every link with LNK1207.
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
    ScriptBehaviour.cs           ← base class for user scripts (instance model, future)
    ObjectHandle.cs              ← managed wrapper for native Object (instance model, future)
    ObjectInterop.cs             ← P/Invoke bindings to engine (transform + name)
    InteropLib.cs                ← resolves DllImport("Incogine") to the host executable
    EngineInterop.cs             ← Time/Log/Scene/Input/Save/Audio/Object extras
```

## Files

| File | Purpose |
|------|---------|
| `src/core/scripting/csharp/csharphost.h/.cpp` | CoreCLR host initialization |
| `src/core/scripting/csharp/cshandlerruntime.h/.cpp` | Per-script handler (static entry points) |
| `src/core/scripting/csharp/csharpinterop.h/.cpp` | Exported C ABI for P/Invoke |
| `src/core/objects/objects.h/.cpp` | `Object` + live-object registry (`FindByName`) |
| `src/scripts/csharp/Incogine/` | Managed assembly source |
| `src/scripts/csharp/examples/` | Example scripts (`StaticPlayer.cs` runs today) |
