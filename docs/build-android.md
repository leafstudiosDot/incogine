---
title: Build for Android
description: Building Incogine for Android with Gradle and the NDK.
sidebar_position: 7
tags: [build, android, gradle]
---

# Build for Android

Packaging is driven by the vendored SDL3 `android-project/` Gradle template.

## Requirements

- Android SDK
- NDK **28.2.13676358** (pinned in `android-project/app/build.gradle`)
- `APP_PLATFORM` is android-21, ABI arm64-v8a

## Build

```bash
cd android-project
./gradlew assembleDebug          # APK in app/build/outputs/apk/debug/
```

## How it fits together

- `app/jni/CMakeLists.txt` is a thin adapter that points Gradle's CMake invocation at the repository root `CMakeLists.txt`.
- That root CMake builds SDL3 + addons as shared libraries (`libSDL3.so`, `libSDL3_ttf.so`, ...) and the game as the `main` shared library loaded by `SDLActivity`.
- `SDLActivity.getLibraries()` (in `android-project/app/src/main/java/org/libsdl/app/SDLActivity.java`) lists the `.so` files to load.
