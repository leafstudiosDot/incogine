---
title: Incogine Studio
description: The developer IDE for an Incogine project.
sidebar_position: 16
tags: [studio, ide, incoba, tooling]
---

# Incogine Studio

**Incogine Studio is the IDE for an Incogine project.** It manages the whole
project — source code, scenes, scripts, assets, project settings, and credits —
from one environment.

## Actual project layout

The Studio proposal described root-level `credits.xml`, `projects.xml`,
`assets/`, and `studio/` paths. The repository as built keeps project files
under `src/` (singular `project.xml`):

```text
<repo root>/
├── src/
│   ├── assets/        # project/game assets (fonts, audio, ...)
│   ├── project/       # project-layer source (Puroko game code)
│   ├── scenes/        # scene .cpp/.h (source of truth for scenes, for now)
│   ├── scripts/       # C# (csharp/) + Kodo (kodo/) scripts
│   ├── studio/        # Studio application source (this IDE)
│   ├── core/          # engine core (never closed; MPL file-level copyleft)
│   ├── credits.xml    # project credits (NOT root-level)
│   └── project.xml    # project settings (singular; NOT projects.xml)
└── CMakeLists.txt
```

Studio resolves all of this through `src/studio/core/project_paths.h`
(`ProjectPaths::FindRoot` walks up to the directory containing
`src/project.xml`), so the IDE works when opened from any subdirectory.

Key distinction:

```text
src/assets/    = project/game assets on disk
src/core/…     = engine source implementing the asset/scene/script systems
src/project/   = project-layer source code
src/scenes/    = scene implementations (.cpp/.h = source of truth, Phase 1)
src/scripts/   = script sources, organized by the developer
src/studio/    = Studio application source (dev-only, never in the runtime)
```

## CMake targets

CMake is the source of truth. Studio adds three targets alongside the existing
`Incogine` game and `Puroko` library (see the top-level `CMakeLists.txt`):

| Target | What | Qt? |
|---|---|---|
| `IncogineStudioCore` | Qt-free models: `project.xml` / `credits.xml` parse+save, scene discovery, `.incoba` reader/writer | No |
| `IncogineIncoba` | Headless `incoba_packer` CLI (pack/list) | No |
| `IncogineStudio` | Qt Widgets IDE shell (main window, docks, forms) | Yes — only if Qt6 Widgets is found |

Rules that keep the runtime clean (§17):

- `src/studio/*` is explicitly removed from the game executable globs, so the
  `Incogine` runtime never links Studio or Qt.
- The Qt shell is optional: without Qt6 Widgets, configure logs
  `IDE shell skipped` and still builds Core + the packer.
- Studio targets are excluded from Android/Web game packaging.
- `ICG_BUILD_STUDIO=OFF` disables all Studio targets:
  `cmake -DICG_BUILD_STUDIO=OFF ..`
- On Windows, `src/studio/CMakeLists.txt` hints
  `C:/Qt/6.10.2/msvc2022_64` automatically when present.

Build:

```text
mkdir build && cd build && cmake .. && cmake --build . --target IncogineStudio
cmake --build . --target IncogineIncoba   # packer only
```

## What the IDE shell does (Phases 1–3)

- **Project dock** — `QFileSystemModel` over `src/` with right-click
  **New file / New folder / Rename / Delete**, drag-drop move, and refresh,
  plus a **Loading...** indicator while folders populate (dismissed when
  content lists). Delete/rename confirm and warn that `#includes` and
  references need hand-updating. No language folders are ever reserved:
  `src/scripts/` stays developer-organized. Project source is editable,
  not read-only.
- **Code editor** — syntax highlighting for C/C++, C#, Kodo (keywords from
  the engine lexer), Python, XML, and JSON, with light- and dark-theme
  palettes chosen from the application theme (rebuilt live on theme
  change), plus an inline **find/replace** bar (`Ctrl+F` / `Ctrl+H`) with
  wrap-around and back-to-front replace-all. No custom abstractions;
  completion comes later.
- **Editor tabs (VS Code-style)** — every open file gets its own closable
  tab titled by project-relative path (`src/main.cpp`, last 120 chars).
  Tabs keep a readable minimum width and scroll instead of squeezing, with
  Notepad++-style `‹` `›` pager buttons and `Ctrl+Tab` / `Ctrl+Shift+Tab`
  to step through them; tabs are also drag-reorderable. Edited files
  show a `*` marker until saved; closing a dirty tab asks Save/Discard/
  Cancel. Credits, Project Settings, and Scene tabs stay pinned and
  unclosable. `File → Save all files` saves every dirty tab.
  `Edit → Undo/Redo` (`Ctrl+Z` / `Ctrl+Shift+Z`) target the front code
  tab; each code tab owns its document, so histories stay per-tab.
  Saves are byte-verbatim: files are read raw and the original EOL
  (CRLF vs LF) and UTF-8 BOM ride along as page properties, so
  untouched files round-trip byte-identically.
- **Manual game compile** — the main toolbar's **Compile game** button
  next to Save/Refresh/Rescan configures (`cmake -S/-B`, first run
  only) and builds the CMake game target (`CMAKE_PROJECT_NAME` from
  the configured tree, `Debug`), never Studio itself and never
  automatically; VS2026/CMake CLI builds keep working untouched. The
  button disables while the build runs. Output streams to the Output
  dock color-coded: errors red, warnings yellow, info in the theme's
  default text color (palette-driven, correct in dark/light mode).
  Switching scenes with unsaved Scene-tab edits prompts
  Save / Don't Save / Cancel first (the editor re-parses from disk, so
  unsaved work would otherwise be lost).
- **Audio preview tabs** (`Audio - <name>`) — Audition-style stereo
  waveform lanes decoded in the background (progressive draw, played region
  highlighted, red playhead), with play/pause (or `Space`), `mm:ss / mm:ss`
  duration readout, volume slider, mute, zoom (−/+/Fit buttons and wheel,
  centered on cursor), and a loop toggle underneath. Opening a file shows a
  centered **Loading...** state and playback stays disabled until the
  waveform is ready (decode errors still unlock playback, since the player
  backend is independent). Clicking or dragging the waveform seeks
  immediately. Peaks are shrunk once into
  fixed-resolution columns, so repaints (playhead ticks) are cheap lookups
  instead of rescans, and decode draws are throttled to ~8 Hz. An info line
  shows size, sample rate, channels, bit depth, and duration; decode
  problems report in a status label instead of failing silently
  (Qt Multimedia; without it audio files open as text with a log note).
- **Font preview tabs** (`Font - <name>`) — pangram + digits
  ("The quick brown fox jumps over a lazy dog... 1 2 3 4 5 6 7 8 9 0")
  rendered in the loaded file with an adjustable preview size (6–144 pt).
- **Window layout memory** — size, position, and dock layout persist across
  runs via `QSettings` (default 1440×900 on first run, roomier than the
  engine's 1280×720).
- **Search in files dock** (`Ctrl+Shift+F`) — recursive project search with
  binary/build-dir pruning (`reqs/`, `emsdk/`, `build*/`, `.git`, …);
  double-click jumps to file + line.
- **Credits editor** — form over `src/credits.xml` (add/remove people and
  contributors, headers, subheaders, grids) with a raw-XML preview. Unknown
  elements are preserved verbatim on save.
- **Project Settings editor** — form over `src/project.xml` (name, window
  name, identifier, version, description, author, copyright) plus the raw
  `<settings>` payload, which is extensible and preserved verbatim. The
  `<incogine_version>` field stays read-only (mirrored from
  `src/core/engine/version.h`); `<name>` must remain a single token because it
  becomes the executable filename.
- **Scene editor tab (Unity-style, Phase 3).** The Scene tab edits
  directly from the parsed `.cpp/.h` — **no game process needed** (live
  pixels belong to the Preview tab; this canvas never switches to them).
  Offline it draws the layout itself: parser-known boxes (exact `Square`
  rects) and font labels (real TTFs) in the 1280×720 design space, framed
  by a **1px border showing exactly what the game sees**. Sizes scale with
  the viewport from the 1280×720 base (like the game's `windowHeight/720`
  factor), so layout matches at any canvas size and on any renderer
  backend (SDL3/OpenGL today — DirectX/Metal/Vulkan must preserve the
  same design-pixel mapping). Labels draw single-line, never wrapped
  (the engine blits one text quad), in device pixel sizes matching the
  engine's `setFontScale` re-rasterization. The **Window** combo
  (640×360 … 2560×1440, always 16:9 like the game enforces on resize)
  simulates the engine window: `GetWindowSize()` re-evaluates at that
  size (positions, `Square` scale factors, glyph sizes), the camera
  remaps 1:1 onto the simulated window, and unsaved edits survive the
  switch — so resize behavior is checkable without running the game.
  Toolbar: cursor tools **Select / Move /
  Rotate / Scale / Hand** (keys 1–5; rotate/scale drags arrive later,
  spins work now), 2D/3D toggle, Perspective/Orthographic/Isometric
  camera selector, 10px snap, Save to source. Hierarchy tree (objects
  nest, font labels listed with dynamic ones tagged) with drag-reparent
  (rewrites `setParent`, cycle-guarded; texts can't parent), Inspector
  (Position/Rotation/Scale + RGBA for Squares, file id, Apply live,
  Add/Delete), Anchor presets, Source view. Click/drag moves boxes and
  constant-positioned text with realtime viewport feedback (the model
  follows the cursor during the drag; source spans rewrite once on
  release, and a refused drop snaps back to the press-time position).
  The Inspector's Anchor presets snap a placed label to the 9-point
  window grid (Left/Center/Right × Top/Middle/Bottom over the simulated
  window size) by writing constants through the normal text-position
  path, so anchored labels stay draggable; labels sharing one loop
  `renderUI` call can't drag or anchor (one edit would move siblings)
  and say so instead. The **Stick** toggles (Left/Right/Top/Bottom)
  constrain instead of placing: checked edges rewrite `renderUI` args
  to `GetWindowSize()`-relative forms measured via `getSize()` (the
  `versionFont` idiom), so the label holds its edge at any window size;
  opposing pairs on one axis center the label, and unchecking bakes
  constants back. Constrained labels keep drawing (Studio measures
  glyphs with the real TTFs) but refuse drags/anchors/spins that would
  bake their constraints — Hierarchy tags them `(constrained)`. Dropping an asset creates a placeholder
  `Square_N`. In 3D mode an orbitable grid previews camera math until
  Cube rendering lands. Edits apply to the in-memory model (dirty `*`)
  and flush on Save. Every committed edit pushes the pre-edit source
  onto a per-tab undo stack (toolbar Undo/Redo, `Ctrl+Z` /
  `Ctrl+Shift+Z`, 50 entries; no-op commits are dropped), and Save to
  source writes the serializer output byte-verbatim, so only rewritten
  spans change — a dragged font moves exactly one line.
- **Scene panel.** Discovery scans `src/scenes/` for
  `class X : public Scene`, shows the declared `Scene("…")` name,
  header/source locations, and whether the scene ships in the `Puroko`
  lib or is exe-only (`splash/`, `settings/`). Clicking anything in the
  panel loads the scene into the editor tab (retitled `Scene - <name>`)
  and targets Preview launches at it; no Code tab is opened.
- **Round-trip parser** (`src/studio/core/scene/scene_cpp.h`): constructor
  patterns (`new Square/Cube/Object`, `setName`/`setId`, transform/color
  calls, `addComponent`/`setParent`) plus `Font` member declarations
  (`Font f;`, `Font f[N];`, and `std::vector<Font>` sized by a constant
  `resize(...)`), `setFontFile`, `setTextContent` (literals plus
  data-driven `Data[i]` / `Data[i].name` inside counted loops), numeric
  `setColor`, and `renderUI` call sites. Constant args place the label
  directly; counted `for` loops over static data (menu tables, `sizeof`
  idioms, `Data.size()`) evaluate per iteration with C++ int/float
  semantics at the simulated window size (`GetWindowSize()` reports it;
  default 1280×720), so menu lists render with real content and
  positions at any simulated size. `getSize()` in layout math resolves
  through an optional measurer (Studio supplies the TTF metrics), so
  window-constrained labels place too — tracked with edge pins
  (left/right/center, top/bottom/center) and a no-bake guard instead of
  going dynamic. Loop-placed labels share one call site: drawn
  and selectable, but position rewrites refuse (`sharedSite`) instead of
  moving siblings. Truly computed layouts (measured text sizes, runtime
  selection state) stay dynamic and refuse rewrites instead of freezing
  expressions. All ops rewrite exact spans — rename, transform, id,
  parent, add, remove, text position — with byte-identical no-op round
  trips, while `Update()`/`Render()` code, control blocks, and
  non-Object allocations stay verbatim.
- **Preview tab (passive monitor).** The former Viewport tab now only runs
  and watches: game picker with dev-build binding, Launch/Stop, live
  frames, status. One shared session object feeds both tabs, so Scene and
  Preview can never fight over the single-instance channel. All transform
  editing moved to the Scene tab.
- **Asset Browser** — `QFileSystemModel` over `src/assets/` (browse, open,
  import by copying in) with a **Loading...** indicator while folders
  populate (dismissed when content lists). Import settings and dependency
  tracking come later.
- **Inspector (placeholder)** — documents the real `Object` model
  (`Position`/`Scale`/`Rotation`/`Color`, components `Transform`/`Sprite`/
  `ScriptComponent`); live-object property binding lands with the scene format.
- **Output dock** — Studio/build log.
- **Preview Console dock** — raw stdout/stderr of the running preview game
  process (`[err]`-prefixed for stderr), so `cout`/`cerr` and engine logs
  from the game appear in the IDE while it runs.
- **Headless self-test** — `IncogineStudio --self-test <root>` with
  `QT_QPA_PLATFORM=offscreen` constructs the full window (scene discovery +
  XML loads) and exits; used to smoke-test the shell without a display.
- On Windows the Studio executable carries the same `bundle/windows/icon.ico`
  as the game (via `src/studio/studio.rc`).

## Scripts

`src/scripts/` stays developer-organized. Studio never imposes
`src/scripts/kodo/` or `src/scripts/csharp/` as mandatory roots and never
auto-creates language folders — those directories exist in this repo because
the project uses both languages, not because Studio requires them. Language is
determined per file/project configuration, and any `src/scripts/newfolder/`
the developer creates is preserved.

## `.incoba` asset bundles (v1)

Development loads loose files through `AssetManager`. For distribution,
`src/assets/` is split into payload-capped bundles sharing one folder with
a searchable index:

```text
Development:  src/assets/{fonts,audio,...}  ->  AssetManager::Open(path)
Release:      assets/{index.incobai, a.incoba | a_00.incoba, ...}
                                             ->  index lookup -> blob slice
                                             ->  AssetManager
Updates:      downloaded patch dir(s)       ->  MountBundleDir (override, live)
```

Bundle format (little-endian, no third-party deps —
`src/studio/core/incoba/incoba.h`, codec helpers in `incoba/incoba_detail.h`):

```text
magic[6]="INCOBA", version u16=1, flags u16=0, entryCount u32
per entry: pathLen u16, path (UTF-8, '/' separators, relative to asset root),
           offset u64, size u64, storedSize u64, method u8, crc32 u32
then concatenated entry blobs
```

Index format (`index.incobai`, entries sorted by path for binary search):

```text
magic[7]="INCOBAI", version u16=1, flags u16=0, bundleCount u16
bundleCount x { nameLen u16, name bytes }   (relative to the index dir)
entryCount u32
per entry: pathLen u16, path, bundleIdx u16,
           offset u64 (absolute bundle file offset), size u64, crc32 u32, method u8
```

- Splitting is greedy in sorted-path order at `ICG_INCOBA_MAX_MB` MiB of
  payload per bundle (default 128; approximate — table overhead excluded).
  One bundle keeps the `<stem>.incoba` name (`a.incoba`), otherwise
  `<stem>_<NN>.incoba` (`a_00.incoba`, `a_01.incoba`, …). An oversized
  single file gets a bundle of its own.
- Patch/update sets for live games are ordinary split outputs: pack just
  the new/changed files, ship the set, and mount it at runtime with
  `AssetManager::MountBundleDir()` — mounted entries override the base
  install live, newest mount wins. See [Assets](./assets.md).
- v1 writes method 0 (stored/uncompressed) only; methods 1+ are reserved for
  future compression. Readers accept 0 and reject the rest.
- CRC-32 is a corruption check, not security. The bundle gives packaging,
  fewer files, and casual modification resistance — explicitly **not DRM**.
- Studio-side code is split by concern: `incoba/incoba.cpp` (CRC, single-bundle
  writer, directory packer, `Reader`) and `incoba/incoba_index.cpp` (split
  packing, index, `IndexedReader`). The split packer builds its index
  from the writer's reported table rows instead of re-reading each
  bundle back.
- The engine runtime reads bundles itself (`src/core/assets/`:
  disk → bundle → embedded). Studio Core's `Reader`/`IndexedReader` are the
  reference implementations; the engine parser mirrors the format and must
  stay in sync. The runtime never links Studio code.

Packer usage:

```text
incoba_packer <assetDir> <out.incoba>   # single bundle (legacy)
incoba_packer --split <assetDir> <outDir> [--stem a] [--max-mb 128]
incoba_packer --list <bundle.incoba>
incoba_packer --list-index <index.incobai>
```

## Automatic bundling on every game build

You do **not** need to open Studio to get bundles. Normal root-project
builds pack automatically via `ICG_USE_INCOBA` (default `ON`) — one switch,
one layout, in every configuration:

- The `IncogineIncobaBundle` target (built with `ALL`) runs the headless
  `IncogineIncoba` packer with `--split` over `src/assets/` — no Qt, no IDE
  involved — into `<build>/incoba_staging/` (`index.incobai` + bundles).
- The executable's `assets/` folder is cleared and filled with **only** that
  bundle set (`$<TARGET_FILE_DIR>`), so every configuration ships
  bundle-only and the runtime finds it with zero configuration.
- `-DICG_USE_INCOBA=OFF` switches to the direct-disk flow instead: loose
  `src/assets/` files only, no bundles.
- Asset tracking uses `CONFIGURE_DEPENDS`, so added/removed/edited assets
  trigger a repack.
- This is generator-agnostic: Ninja, Make, and the Visual Studio generator
  (VS 2026 `.slnx` — the bundle target shows up as a project the game
  depends on) all behave the same.
- Skipped on Android/Web, which package assets through Gradle/Emscripten.

## Roadmap / open questions for the maintainer

Per the Studio brief, major systems need discussion before implementation:
prefab/template system, scene serialization format + C++ codegen,
hot reload, play mode, breakpoints, code completion, asset database + import
pipeline, `.incoba` compression choice, resource IDs, build profiles,
packaging, project templates, and plugin/shader/audio/input/localization
editors. Phase 1 deliberately stops at the shell + models + packer.

## Engine roadmap (what to add next)

Phase 3 order, biggest unlocks first:

1. **`Cube` rendering + camera adoption** — the renderer still uses a fixed
   ortho path and `Cube::Render` is empty. Render boxes through the new
   `Camera` (2D already matches pixel-for-pixel; verified), then 3D scenes
   become editable instead of view-only.
2. **Relative transforms** — hierarchy is organizational today (world-space
   storage, editors translate subtrees). Compose parent→child transforms
   in `Object` when ready for true nested prefabs-in-spirit.
3. **Sprite textures** — `Sprite` is color-only; texture assignment is the
   top asset-editor unlock (Asset Browser drag already stages the drop).
4. **Script attach patterns** — `ScriptComponent` construction is parsed
   but not yet written; adding the writer op unlocks drag-a-script-onto-
   an-object.
5. **Physics/collision stubs** — Kodo notes physics as unimplemented;
   decide 2D-first (AABB) scope before any editor.
6. **Prefab/templates, play mode, hot reload** — in that order;
   each builds on the round-trip + preview channel already in place.
   (Undo/redo already landed: per-tab document history in code tabs,
   snapshot history in the Scene tab.)
