---
title: Objects and Components
description: The Object / Component model in Incogine.
sidebar_position: 10
tags: [objects, components]
---

# Objects and Components

The object/component model lives in `src/core/objects/` and `src/core/components/`.

## `Object`

`Object` carries `Position`/`Scale`/`Rotation`/`Color` POD structs and a `std::vector<Component>`. Concrete subtypes:

| Subtype | Behavior |
|---------|----------|
| `Cube` | No rendering yet. |
| `Square` | Renders with its `Sprite` component. |

## `Component`

`Component` is a thin base. Existing components:

- `Transform`
- `Sprite` (color only)

## Notes

- The component system is intentionally minimal — most scenes don't use `Object` directly; they compose `Square`/`Font`/`Audio` manually.
- `Object::Render()` is currently a no-op; rendering is implemented per concrete subclass (e.g. `Square::Render`).
