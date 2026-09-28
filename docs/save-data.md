---
title: Save Data
description: Key-value persistence via SaveData, shared by C++ game code and scripts.
sidebar_position: 12
tags: [save-data, persistence]
---

# Save Data

The save system lives in `src/core/engine/savedata/` (`savedata.h:14`).

`SaveData` is a string key-value store with file persistence:

```cpp
#include "core/engine/savedata/savedata.h"

SaveData save("mysave");   // -> <pref-path>/mysave.dat
save.Set("coins", "100");
save.Save();               // flush to disk -> bool

SaveData loaded("mysave");
loaded.Load();             // replace memory from disk -> bool (false: no file yet)
loaded.Get("coins");       // "100" ("" when the key is missing)
loaded.Has("coins");       // -> bool
loaded.Remove("coins");    // -> bool (true when the key existed)
loaded.Clear();
```

`SaveData` resolves a per-platform writable path via `SDL_GetPrefPath(PROJECT_AUTHOR, PROJECT_NAME)`.

## Shared script store

Both scripting languages delegate to one `SaveData` instance —
`SharedScriptSave()` (file `scriptsave.dat`) — so a value written from C#
(`SaveApi.Set`) is visible to Kodo (`Save.get`) and vice versa. The C ABI is
`Incogine_Save_*` (`src/core/scripting/csharp/csharpinterop.h`).

To change the save file location, look at `src/core/engine/savedata/savedata.cpp`.
