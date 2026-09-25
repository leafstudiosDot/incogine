# Kodo Scripts

This directory is where Kodo (`.kodo`) script files live.

## Structure

```
src/scripts/kodo/
    README.md                 ← this file
    syntax-reference.md       ← Kodo syntax reference (to be filled in)
    examples/
        hello.kodo            ← placeholder example
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

Kodo is currently a **placeholder**. The interpreter has not been implemented yet.
When you run a game with Kodo scripts attached, you will see a log message:

```
[Incogine] Kodo scripting not yet implemented. Script: scripts/kodo/examples/hello.kodo
```

This is expected. Once the Kodo language syntax is defined in `syntax-reference.md`,
the parser and interpreter will be implemented in `src/core/scripting/kodo/`.

## Writing Kodo Scripts

See `syntax-reference.md` for the language reference (to be filled in by the maintainer).
