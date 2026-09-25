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

## What's Implemented

- `ScriptLanguage::Kodo` enum value
- `KodoScriptHandler` — loads `.kodo` files (currently logs placeholder)
- `Kodo::Parser` — stub in `src/core/scripting/kodo/kodoparser.h:13` (to be implemented from the spec)

## What's Not Implemented Yet

- Lexer / tokenizer / AST / interpreter (spec is ready — see `src/scripts/kodo/syntax-reference.md:1`)
- Bindings: `Object.find/create/destroy`, `Sprite` image (`assets/*`), `Audio.seek/forward/reverse`, `Save`, `Random`, `Engine.setScene:52`, `++`/`--`

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
| `src/core/scripting/kodo/kodohandler.h:1` | Script handler (placeholder) |
| `src/core/scripting/kodo/kodoparser.h:13` | Parser stub — implement here |
| `src/scripts/kodo/` | Script files directory |
| `src/scripts/kodo/syntax-reference.md:1` | Language reference (draft by you) |
