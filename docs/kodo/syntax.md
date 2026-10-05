---
title: Kodo Syntax Reference
description: Language reference for the Kodo scripting language.
sidebar_position: 14
tags: [scripting, kodo, syntax]
---

# Kodo Syntax Reference

> Canonical spec is `src/scripts/kodo/syntax-reference.md:1`. This page is the docs-site mirror — update both when you change syntax.

- Dynamically typed — `var` for every variable
- No semicolons (newline-terminated; `;` only as separator in `for` headers)
- Comments `//` and `/* */`

---

## Overview

Each `.kodo` file is a script attached to an `Object` via `ScriptComponent` (`src/core/components/script/scriptcomponent.h:16`).

## Variables

```
var s = "string"
var n = 1
var b = true
var arr = [1, 2, 3]
var obj = { key: "value" }
var vec = Vector(1, 2, 3)
var col = Color(1, 0, 0, 1)
var audio = Audio("assets/audio/testbgm.ogg")
var spr = Sprite("assets/images/hero.png")
```

## Functions

```
function myFunction(a, b) {
    return a + b
}
var add = function(a, b) { return a + b }
```

## Lifecycle

- `start()` — once on activation
- `update()` — every frame
- `onDestroy()` — on destroy

## Accessing the Owner Object

```
var owner = this.owner // this.object is alias; this.owner is canonical
owner.transform.position = Vector(1, 2, 3)
this.owner.sprite = Sprite("assets/images/hero.png")
this.owner.sprite.color = Color(1, 1, 1, 1)
```

## Control Flow

`if/else`, `while`, `for (var i = 0; i < 10; i++)`, `foreach (var item in array)` (`in` is contextual, not reserved), `switch/case/default/break`.

See `src/scripts/kodo/syntax-reference.md:102` for full examples. `i++`/`i--` and `i += 1` both work.

## Built-in Functions

- `log`/`logf` (`INFO/WARN/ERROR/DEBUG`), `lerp`, `cubicBezier` (`src/core/engine/math.h:1`)
- `Random.range`/`rangeFloat`/`pick`
- `Engine.setScene("GameScene")` / alias `Scene.change("GameScene")` (`src/core/engine/engine.h:52`)
- `Object.find`/`findAll`/`create`/`destroy` (`src/core/objects/objects.h:97`)
- `Sprite(Color(...))` or `Sprite("assets/images/*.png|jpg|jpeg|gif|bmp|tiff")`
- `Audio("assets/audio/*.ogg").play()/stop()/seek()/forward()/reverse()/setVolume()/setLoop()`
- `Save.set/get/save/load` (`src/core/engine/savedata/savedata.h:1`)
- `Time.deltaTime` / `Time.time`

See `src/scripts/kodo/syntax-reference.md:141` and `builtins.md`.

## Types

All `var`; `null` for absence.

```
var i = 42
var f = 3.14
var t = "hi"
var flag = true
var nothing = null
```

## Operators

`+ - * / %`, `== != < <= > >=`, `&& || !`, `= += -= *= /=`, `++ --`, `. []`, string `+`.

See `src/scripts/kodo/syntax-reference.md:182`.

## Error Handling

```
try {
} catch (error) {
    log("Caught an error: {error}", error=error, level=ERROR)
}
```

## Physics

TODO — `src/scripts/kodo/syntax-reference.md:Physics / Collision` placeholder (`src/core/objects/objects.h:97`).

## Import

```
import "scripts/kodo/utils.kodo"
from "scripts/kodo/utils.kodo" import { helper }
import "scripts/kodo/other.kodo" as other
```
