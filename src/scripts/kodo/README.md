# Kodo Scripts

This directory is where Kodo (`.kodo`) script files live.

## Structure

```
src/scripts/kodo/
    README.md                 ← this file
    syntax-reference.md       ← Kodo syntax reference
    examples/
        hello.kodo            ← minimal lifecycle example
        mover.kodo            ← MVP feature demo (input, Time, Save, Object.find)
```

## How It Works

Kodo scripts are attached to Objects via the `ScriptComponent`:

```cpp
#include "core/components/script/scriptcomponent.h"

auto obj = new Object("MyObject", Position(0,0,0), Scale(1,1,1), Rotation(0,0,0));
obj->addComponent(std::make_unique<ScriptComponent>(
    obj, "scripts/kodo/examples/hello.kodo", ScriptLanguage::Kodo));
```

## Status

Kodo has an **MVP interpreter** (`src/core/scripting/kodo/`: lexer, parser,
tree-walk runtime). `start()` / `update()` / `onDestroy()` run with `this.owner`
bound; script errors are logged with file and line, and a failing `update()`
disables that script (deterministic scripts would otherwise spam every frame).

Implemented: `var`, functions (named/anonymous, recursion, closures), `if/else`,
`while`, `for`, `foreach`, `switch`, `try/catch`, `import`, all operators,
`Vector` math, and the engine modules (`log/logf`, `Random`, `Engine`/`Scene`,
`Object`, `Sprite`, `Audio`, `Save`, `Time`, `Input`).

Known limits: `Sprite("path")` records the texture path but the engine renders
colored quads only; `Audio.seek/forward/reverse/setVolume/setLoop` are accepted
but ignored (engine `Audio` has `play/stop`); `Save.load()` reports success
instead of returning data; no physics/collision bindings yet.

## Writing Kodo Scripts

See `syntax-reference.md` for the language reference and `examples/mover.kodo`
for a runnable demo.
