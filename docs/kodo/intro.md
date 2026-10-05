---
title: Kodo Language
description: Incogine's custom scripting language.
sidebar_position: 13
tags: [scripting, kodo]
---

# Kodo

**Kodo** is a custom scripting language for the Incogine game engine by leafstudiosDot. Each `.kodo` file is attached to an `Object` as a `ScriptComponent`.

- Dynamically typed (`var`), no semicolons, comments `//` / `/* */`
- Spec: `src/scripts/kodo/syntax-reference.md`

## What's Implemented (MVP)

- `ScriptLanguage::Kodo` enum value
- `KodoScriptHandler` — parses each `.kodo` file once, runs top-level code, then dispatches `start()` / `update()` / `onDestroy()` with `this.owner` bound
- `Kodo::Lexer` / `Kodo::Parser` / `Kodo::Interpreter` (`kodolexer.*`, `kodoparser.*`, `kodointerpreter.*`, `kodoeval.cpp`, `kodobindings.cpp`, `kodoengine.h`, `kodovalue.h`, `kodoast.h`)
- Language: `var`, named/anonymous functions, closures, recursion, `if/else`, `while`, `for`, `foreach`, `switch`, `try/catch`, `import`, all spec operators including `++`/`--`, named call args
- Bindings: `log/logf`, `lerp`, `cubicBezier`, `Random`, `Engine.setScene` / `Scene.change`, `Object.find/findAll/create/destroy`, `Sprite(Color)` (+ texture path recorded), `Audio` play/stop, `Save`, `Time`, `Input` (incl. pressed/released edges), `this.owner.transform/sprite/name`
- Safety: error messages carry file:line, call-depth limit (256), per-dispatch step budget, failing `update()` disables the script

## What's Not Implemented Yet

- `Sprite` image rendering (texture path is stored; engine draws colored quads)
- `Audio.seek/forward/reverse/setVolume/setLoop` (accepted, ignored)
- `Save.load()` return shape (currently returns success bool)
- Physics / collision bindings (`Object.collidesWith`, `onCollision`)
- `Object.create` ownership (created objects are Kodo-owned until `destroy()`)

## Vectors Are Values

`owner.transform.position` returns a copy — `pos.x = 5` on it fails loudly.
Assign whole Vectors back:

```
this.owner.transform.position = this.owner.transform.position + Vector(step, 0, 0)
```

## Attaching Kodo Scripts

```cpp
#include "core/components/script/scriptcomponent.h"

auto obj = new Object("Enemy", Position(100,0,0), Scale(1,1,1), Rotation(0,0,0))
obj->addComponent(std::make_unique<ScriptComponent>(
    obj, "scripts/kodo/enemy.kodo", ScriptLanguage::Kodo))
```

No semicolons in Kodo — but C++ still needs them.

## Writing Kodo Scripts

See `src/scripts/kodo/syntax-reference.md` and `docs/kodo/syntax.md`.

## Files

| File | Purpose |
|------|---------|
| `src/core/scripting/kodo/kodohandler.h:1` | Lifecycle wiring (`start`/`update`/`onDestroy`) |
| `src/core/scripting/kodo/kodoparser.h:1` | Recursive-descent parser → AST |
| `src/core/scripting/kodo/kodointerpreter.h:1` | Tree-walk runtime + engine bindings |
| `src/scripts/kodo/` | Script files directory |
| `src/scripts/kodo/syntax-reference.md:1` | Language reference (draft by you) |
