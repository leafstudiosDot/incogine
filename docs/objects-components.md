---
title: Objects and Components
description: The Object / Component model in Incogine.
sidebar_position: 10
tags: [objects, components]
---

# Objects and Components

The object/component model lives in `src/core/objects/` and `src/core/components/`.

## `Object`

`Object` carries `Position`/`Scale`/`Rotation`/`Color` POD structs and a `std::vector<std::unique_ptr<Component>>`. Concrete subtypes:

| Subtype | Behavior |
|---------|----------|
| `Cube` | No rendering yet. |
| `Square` | Renders with its `Sprite` component. |

## Identity and hierarchy

Every `Object` carries a runtime-unique `uint64_t` id (`getId()`, stable
across renames) and non-owning hierarchy links (`setParent` / `addChild` /
`getChildren`). The id generator starts at 1 and auto-assigns on
construction; loaders override ids with `setId()`, which pushes the
generator past any loaded id so later objects never collide. `FindById()`
joins `FindByName()` / `FindAllByName()` for lookups.

Lifetimes are unchanged: whoever `new`'d an object still owns it.
Destroying an object detaches it from its parent and orphans its children
— there is no cascading delete. Link attempts that would create a cycle
are rejected (`setParent` / `addChild` return `false`). Stored transforms
stay in world space; the hierarchy is organizational, and editors
translate subtrees explicitly when moving a parent.

## `Component`

`Component` is a polymorphic base with virtual lifecycle methods (`Start`, `Update`, `OnDestroy`). Existing components:

- `Transform` — position, scale, rotation
- `Sprite` — color only
- `ScriptComponent` — holds a `ScriptHandler` (C# or Kodo) for gameplay scripting

## `ScriptComponent`

`ScriptComponent` attaches a script to an Object. The script can be written in C# or Kodo. See [Scripting](./scripting.md) for details.

```cpp
#include "core/components/script/scriptcomponent.h"

obj->addComponent(std::make_unique<ScriptComponent>(
    obj, "scripts/csharp/PlayerController.cs", ScriptLanguage::CSharp));
```

## Notes

- Components are stored as `std::unique_ptr<Component>` for polymorphic behaviour.
- `Object::startScripts()` / `updateScripts()` / `destroyScripts()` iterate components
  and call lifecycle methods on `ScriptComponent` instances.
- `Object::Render()` is currently a no-op; rendering is implemented per concrete subclass
  (e.g. `Square::Render`).
