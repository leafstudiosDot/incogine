---
title: Build on iOS
description: Building Incogine for iOS with Xcode.
sidebar_position: 5
tags: [build, ios]
---

# Build on iOS

Incogine is built for iOS as a static-SDL3 app bundle via the Xcode generator:

```bash
mkdir build
cd build
cmake -G Xcode -DCMAKE_SYSTEM_NAME=iOS ..
```

`bundle/ios/` supplies the asset catalog, launch screen, and a generated `Info.plist`.

The iOS `Info.plist` is generated from `project.xml` fields by `src/parser/ios_infoplist_gen.py` — see [Code Generation](./assets.md#code-generation) in the Assets docs.
