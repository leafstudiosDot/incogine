---
title: Save Data
description: How Incogine resolves the save file location.
sidebar_position: 12
tags: [save-data, persistence]
---

# Save Data

The save system lives in `src/core/engine/savedata/`.

`SaveData` resolves a per-platform writable path via `SDL_GetPrefPath(PROJECT_AUTHOR, PROJECT_NAME)`.

:::warning
`Save()` / `Load()` are currently stubbed (`return false`) — fill these in when implementing persistence.
:::

To change the save file location, look at `src/core/engine/savedata/savedata.cpp`.