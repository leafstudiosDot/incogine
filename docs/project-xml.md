---
title: Project XML
description: The project identity fields in src/project.xml and how CMake consumes them.
sidebar_position: 13
tags: [project.xml, configuration]
---

# Project XML

Project identity is stored in `src/project.xml`. `CMakeLists.txt` regex-extracts the following fields and exports them as compile-time defines:

| Field | Defines |
|-------|---------|
| `<name>` | `PROJECT_NAME` (also the executable name) |
| Window title | `WINDOW_NAME` |
| Bundle ID | `BUNDLE_IDENTIFIER` |
| Version | `PROJECT_VERSION` |
| Author | `PROJECT_AUTHOR` |
| Copyright | `PROJECT_COPYRIGHT` |
| Description | `PROJECT_DESCRIPTION` |
| `<incogine_version>` | `INCOGINE_VERSION` (mirrored from `src/core/engine/version.h` — do not edit manually) |

## Rules

- The `name` key must be a **single token with no spaces** — it becomes the executable filename:

```xml
<name>Incogine</name>       <!-- OK -->
<name>Incogine Engine</name> <!-- Not OK -->
```

- The `incogine_version` key should **not** be modified by you — updating the base source code updates that key's value.
- Per [Contributing](./contributing.md), contributors must not remove `leafstudiosDot` or `Incogine` from derivatives.