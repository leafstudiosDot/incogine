---
title: Kodo Built-in Functions
description: Built-in functions available in Kodo scripts.
sidebar_position: 15
tags: [scripting, kodo, builtins]
---

# Kodo Built-in Functions

> Canonical reference is `src/scripts/kodo/syntax-reference.md:141`. This page summarizes the builtins.

---

## Logging

```
// level constants: INFO (default), WARN, ERROR, DEBUG
log("message")
log("message", INFO)
log("message", WARN)
log("message", ERROR)
log("message", DEBUG)
logf("score: {score}", score=42)
logf("score: {score}", score=42, level=INFO)
logf("score: {score}", score=42, level=INFO, timestamp=true)
```

## Math

```
// mirrors src/core/engine/math.h:1
lerp(a, b, t)
cubicBezier(t, x1, y1, x2, y2)
```

## Random

```
Random.range(0, 100)        // int in [min, max)
Random.rangeFloat(0.0, 1.0) // float
Random.pick([1, 2, 3])      // random element
```

## Scene

```
// both work; Engine.setScene is canonical
Engine.setScene("GameScene") // src/core/engine/engine.h:52
Scene.change("GameScene")    // alias
```

## Object

```
// src/core/objects/objects.h:97
var player = Object.find("Player")
var enemies = Object.findAll("Enemy")
var go = Object.create("Bullet", Vector(0, 0, 0))
this.owner.destroy()
```

## Sprite

```
// Sprite is Color OR image from assets/* (png, jpg, jpeg, gif, bmp, tiff, etc.)
// NOTE: image paths are recorded (read back via .texture) but the engine
// renders colored quads — no texture upload yet.
var spr1 = Sprite(Color(1, 0, 0, 1))
var spr2 = Sprite("assets/images/hero.png")
this.owner.sprite = Sprite("assets/images/hero.png")
this.owner.sprite.texture = "assets/images/hero.png"
```

## Audio

```
// Audio(path) loads from src/assets/* — methods use parentheses
var bgm = Audio("assets/audio/testbgm.ogg")
bgm.play()
bgm.stop()
bgm.play(loop=true)
```

`seek` / `forward` / `reverse` / `setVolume` / `setLoop` are accepted but
ignored — engine `Audio` (`src/core/assets/audio/audio.h:12`) only backs
`play`/`stop` so far.

## Save / Load

```
// game-defined keys; one shared store (scriptsave.dat) for C# and Kodo
// — see docs/save-data.md
Save.set("coins", 100)
var coins = Save.get("coins") // "" when the key is missing
Save.has("coins") // -> bool
Save.remove("coins") // -> bool
Save.clear()
Save.save() // -> bool
Save.load() // -> bool (success; use get()/has() afterwards to read values)
```

## Time

```
var dt = Time.deltaTime // src/core/engine/engine.h:67
var t = Time.time
```

## Input

```
// src/scripts/kodo/syntax-reference.md:Input / docs/kodo/builtins.md:Input
Input.isKeyDown("W")
Input.isKeyPressed("Space")
Input.isKeyReleased("Escape")
Input.getMousePosition() // -> Vector
Input.isMouseButtonDown("Left")
Input.isGamepadButtonDown(0, "A")
Input.getGamepadAxis(0, "LeftStickX")
```

## Transform

```
// Access via this.owner.transform — see src/scripts/kodo/syntax-reference.md:82
var pos = this.owner.transform.position
this.owner.transform.position = Vector(1, 2, 3)
var scale = this.owner.transform.scale
var rot = this.owner.transform.rotation
```

## Physics

```
// TODO — not yet implemented (src/core/objects/objects.h:97)
// Proposed: this.owner.collidesWith(other), onCollision callback
```
