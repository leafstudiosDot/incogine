---
title: "`.incoanim` (2D vector animation)"
description: The .incoanim animation format, its data model, and the Incogine Animate editor.
sidebar_position: 18
tags: [animation, vector, studio, assets]
---

# `.incoanim` — 2D vector animation

**Incogine Animate** is Incogine's Flash/Animate-style 2D vector animation
editor. You draw vector art, animate it on a timeline, save it as an
`.incoanim` asset, and play it in-game as an animated 2D sprite.

Everything described here is **2D only**: 2D coordinates, 2D affine transforms
(translate, rotate, scale, skew), and a flat layer stack. There is no Z-depth,
no perspective, and no 3D pipeline. The module is self-contained so a future 3D
animation editor can sit beside it rather than inside it.

## Where the code lives

| Layer | Location |
|---|---|
| Data model, geometry, `.incoanim` IO, importer | `src/core/animation/` (`IncogineAnim`, Qt-free) |
| Importer registry seam | `src/core/assets/assetimport.h` (`IncogineAssets`, Qt-free) |
| Editor window, canvas, tools, timeline | `src/studio/animator/` (Qt) |
| Editor window | `src/studio/animator/window.*` |
| Document controller (document + stack + dirty + autosave) | `src/studio/animator/document.*` |
| Stage canvas and view transform | `src/studio/animator/canvas.*` |
| Tool interface (`ITool`) + registry | `src/studio/animator/tools/tools.*` |
| Hand / Cursor / Brush / Pen tools | `src/studio/animator/tools/hand.cpp`, `cursor.cpp`, `brush.cpp`, `pen.cpp` |
| Tool options strip (size/smoothing/colors/swatches) | `src/studio/animator/widgets/options_bar.*` |
| Single-instance channel (server side) | `src/studio/animator/channel.*` |
| Entry point (`animator_main.cpp` keeps its prefix: `main.cpp` would collide with `src/main.cpp`) | `src/studio/animator/animator_main.cpp` |

The model and all rasterization live in the **engine**, with no Qt dependency,
so the SDL3 runtime can load `.incoanim` files itself. Studio's window is a thin
editor on top of it.

The shared drawing state is resolved **once, in the engine**, by
`ResolveShape()` (`anim_document.h`): it composes the keyframe and shape
transforms, multiplies the color transforms and alphas, scales the stroke width,
and flattens the path. Both the editor canvas and the runtime rasterizer call
that one function, which is what makes "what you see is what bakes" true rather
than aspirational.

## Incogine Animator

`Incogine Animator` is a **`QMainWindow` in its own executable**, not a tab or a
child window of Incogine Studio. That is deliberate: closing Studio must not
close an animation you are working on.

```
Animate → New Animation…      (Ctrl+Shift+N)   creates the asset, then opens it
Asset Browser → double-click *.incoanim        opens it in the Animator
IncogineAnimator <file.incoanim>              launch it directly
IncogineAnimator --self-test [file]           headless smoke test
```

On Windows the Animator runs straight from `build/` — CMake stages the Qt
runtime next to it (see [Incogine Studio](./studio.md#windows-the-qt-runtime-is-staged-next-to-the-tools)).

- **Separate process.** Studio spawns it with `QProcess::startDetached` and
  passes the file path. Nothing in Studio holds a reference to the window, so
  the animation outlives the IDE.
- **Single instance per document.** The Animator listens on a local socket named
  after the open file (`QLocalServer`); a second launch of the same file sends
  the path to the running window, which raises itself and shows the document,
  then the new process exits. Without this, two editors would race to save one
  file. Studio performs the same check before spawning, and the Animator checks
  at startup too, so a file-association double-click behaves the same way.
  The socket name comes from `core/anim_channel.h` (a deterministic FNV-1a hash
  of the canonical path), shared by both processes so they always agree.
- **Geometry is remembered** via `QSettings` under `animator/geometry` and
  `animator/windowState`, separate keys from Studio's.
- **One window per document.** `File → Open` replaces the current document after
  confirming any unsaved changes; a second `IncogineAnimator` process is what you
  use to work on two animations at once.
- **Save-changes prompt** on close, on New, and on Open — Save / Discard /
  Cancel, where cancelling a Save As correctly aborts the whole operation.
- **Autosave** writes `<name>.autosave.incoanim` beside the document every
  60 seconds of inactivity. It is a recovery copy and deliberately does *not*
  clear the dirty flag: closing the window still prompts.
- **Document dock** (stage size, frame rate, length, loop, bake scale) drives
  everything through the command stack, so undo/redo, dirty tracking, and
  autosave apply without special cases.

### Milestone 1–2 scope

**M1:** the window, document lifecycle (new / open / save / save-as), geometry
memory, dirty tracking with the close prompt, autosave, undo/redo over the
command stack, and the document-properties dock.

**M2:** the real canvas replaces the placeholder.

- **Coordinates: y-down, origin at the stage's top-left corner.** `(0, 0)` is
  the stage's top-left pixel. That matches the engine's world space
  (`camera_math.h`, `QuadRenderer`), Qt's `QPainter` orientation, and the
  animated-sprite draw path, so nothing needs a Y flip anywhere. The only cost is
  that y increases downward, which is normal for a game engine.
- **Rendering: shapes are drawn from `anim_geometry::Flatten()`** — the same
  flattening the runtime rasterizer will use, so the preview and the baked
  sprite sheet come from identical geometry. Qt's own `QPainterPath` bezier
  flattening is deliberately avoided: it is a different algorithm, so the preview
  could then differ subtly from the bake. Qt still antialiases the resulting
  polygons, so the canvas stays smooth.
- **Stage rectangle** with a dashed outline, a transparency checkerboard, and
  off-stage content dimmed but still **visible** while editing (it is clipped
  only on export and in-game).
- **Pan / zoom**: wheel zooms *at the cursor*, `Ctrl+0` fits the stage,
  `Ctrl+=` / `Ctrl+-` step, a live zoom readout sits in the status bar, and
  `Space`+drag pans temporarily with *any* tool. Middle-drag always pans.
- **Tools** go through an `ITool` interface (`tools/tools.h`). The canvas owns
  painting and the view transform and forwards input to the active tool, so
  adding Line, Rect, Ellipse, Paint Bucket, Eraser, or Transform later means
  writing one subclass and adding one line to `ToolSet`'s constructor — no change
  to the canvas or the window. Shortcuts live on the tool (`V`/`H`/`B`/`P`) so a
  new tool brings its own key.
- **Selection** is a set of shape ids on the active layer's active keyframe,
  pruned automatically when the document changes so ids cannot outlive their
  shapes. Click selects, `Shift`+click adds, `Ctrl`+click toggles, dragging empty
  space marquees, and marquee selects only **fully enclosed** shapes (Flash
  behaviour). `Delete` removes the selection as one undo step.
- **Hidden layers are not drawn; locked layers are not editable** but stay
  selectable for inspection, and deleting there reports "Layer is locked"
  instead of silently doing nothing.
- **A drag is one undo step.** The shape transforms are mutated live for
  immediate feedback and the pre-drag transforms are captured, so release pushes a
  single `MoveShapesCommand`; a click that never moved pushes nothing.
- **Repaints never re-subdivide.** Flattened paths are cached per shape id
  (cleared on any command); a repaint only re-resolves matrices and colors,
  so paint cost is O(shapes), not O(segments). Live move drags only touch
  transforms, so the cache stays valid through them.
- **Brush (`B`)**: drag to paint a stroked path. Input is throttled to ~2 screen
  px, then fitted to error-bounded Beziers on release (least-squares cubics,
  longest-within-tolerance wins), so a shaky hand produces a clean selectable
  stroke. The smoothing slider IS the fit tolerance, so the knob is honest:
  curves stay within it of the drawn input (a 3000-point torture scribble
  lands ~215 segments / ~13kB at the 1.0 default, vs ~980 segments before).
  A bare click makes a filled dot in the stroke color. Size (stage units),
  smoothing, color, and opacity come from the tool options strip; `Esc`
  cancels a stroke.
- **Pen (`P`)**: Flash-style — click places corner points, click-drag pulls
  symmetric Bezier handles for smooth points, clicking the start point closes,
  double-click or `Enter` finishes an open path, `Esc` cancels, `Backspace`
  drops the last point. Strokes only in M3; closed paths stay open strokes.
- **Tool options strip** under the main toolbar, visible only for Brush/Pen:
  size, smoothing, opacity, stroke + fill pickers, and swatches (click = stroke,
  `Alt`+click = fill). Settings persist via `QSettings`; fill is stored for
  future shape tools.
- **Drawing auto-creates the keyframe** when the current frame has none,
  copying the nearest earlier keyframe so the span stays continuous; undo
  removes the whole promoted keyframe, restoring the exact prior state.

Headless verification:

```
QT_QPA_PLATFORM=offscreen IncogineAnimator --self-test
```

runs 70+ checks: the command stack (edit/dirty/undo/redo, refused no-ops, layer
add+undo, stage resize+undo), a save → reset → reload round trip, then the
canvas — stage outline and fill pixels actually painted, view-transform
round-trip, hit testing, selection, a drag with exact undo restoration, marquee
enclosure, delete with undo, locked-layer refusal, tool dispatch (all four tools
with shortcuts), drawing options, a committed stroke with hit-test + undo/redo,
and locked-layer draw refusal leaving no history. It plants a
fixture keyframe and shape when the document has none, so it works both bare and
with a file argument. It exits non-zero on any failure.

:::note
A self-test must never open a modal dialog — with `QT_QPA_PLATFORM=offscreen`
nothing can dismiss it and the process hangs. The self-test therefore loads
through `AnimatorDocument` rather than the window's `openPath()`, and a missing
file argument is reported on stderr instead of in a message box.
:::

### Phase 2 (future, not implemented)

3D animation — importing FBX/OBJ/glTF/`.blend`, skeletal and mesh animation, and
a 3D workflow in Studio — is a **separate future task**. The design keeps room
for it:

- The animation module is its own target with its own data model; a 3D
  animation editor sits beside it, not inside it.
- No 2D-only assumption was baked into engine-wide core types. The engine's
  `Position`/`Scale`/`Rotation` (3D doubles) and `icg::Vec3`/`icg::Mat4` are
  **untouched**; the module has its own 2D `Vec2` and `Mat2x3`.
- Importers register through the generic `IAssetImporter` interface, so FBX /
  OBJ / glTF / `.blend` register the same way `.incoanim` does — a future 3D
  importer links `IncogineAssets` and never touches the 2D model.

## File format (v1)

A single **UTF-8 JSON document** with a `formatVersion` field. Human-readable
and diff-friendly by choice: animation files show up in pull requests, and the
existing `.incoba` packer already handles shipping, so a zip container would
buy nothing today. Embedded resources can be added behind a version bump later.

```json
{
  "formatVersion": 1,
  "generator": "Incogine Animate 0.0.0.3",
  "document": {
    "stage": {
      "width": 1280,
      "height": 720,
      "fps": 24,
      "background": "#FFFFFF00",
      "transparentBackground": true
    },
    "timeline": { "lengthFrames": 48, "loop": true },
    "bakeScale": 1
  },
  "layers": [
    {
      "id": 1,
      "name": "Background",
      "visible": true,
      "locked": false,
      "keyframes": [
        {
          "frame": 1,
          "kind": "key",
          "transform": {
            "x": 0, "y": 0, "rotation": 0,
            "scaleX": 1, "scaleY": 1,
            "skewX": 0, "skewY": 0,
            "alpha": 1, "color": "#FFFFFFFF"
          },
          "tweenIn": {
            "type": "none",
            "easing": { "kind": 0, "p1x": 0, "p1y": 0, "p2x": 1, "p2y": 1 },
            "motionFlags": ["position", "scale", "rotation", "alpha", "color"],
            "shapeHints": true
          },
          "shapes": [
            {
              "id": 2,
              "name": "blob",
              "path": [
                { "k": "M", "p": [100, 120] },
                { "k": "C", "p": [133.1371, 120, 160, 102.0914, 160, 80] },
                { "k": "Z" }
              ],
              "style": {
                "fill": true, "fillColor": "#FF8000C8",
                "stroke": true, "strokeColor": "#000000FF",
                "strokeWidth": 3.5, "cap": 1, "join": 1
              },
              "transform": { "x": 10, "y": 20, "rotation": 0.5 }
            }
          ]
        }
      ]
    }
  ]
}
```

### Field reference

| Field | Meaning |
|---|---|
| `formatVersion` | Schema version. Required. Older-than-supported and newer-than-current both fail with a specific message rather than mis-reading. |
| `generator` | Human-readable provenance string. Ignored on load. |
| `document.stage` | `width`/`height` (stage rect, pixels), `fps`, `background` (`#RRGGBBAA`), `transparentBackground`. |
| `document.timeline` | `lengthFrames` (1-based, inclusive), `loop`. |
| `document.bakeScale` | Default rasterization scale for the runtime bake (see trade-offs below). |
| `layers[]` | **Index 0 is the topmost layer** and draws last, like Flash's layer stack. |
| `layer.visible` / `locked` | Visibility gates drawing; `locked` is an editing guard only. |
| `layer.keyframes[]` | **Sparse** — only keyframes are stored. Frame spans fall out of "nearest keyframe at or before N". Sorted by `frame`, unique. |
| `keyframe.kind` | `"key"` (has artwork) or `"blank"` (holds timing, no artwork). |
| `keyframe.tweenIn` | The span from the **previous** keyframe to this one (Flash-style). |
| `shape.path[]` | Segments: `M` (move, 1 point), `L` (line, 1 point), `C` (cubic, 3 points = c1, c2, end), `Z` (close). |
| `shape.style` | `fill`/`fillColor`, `stroke`/`strokeColor`/`strokeWidth`, `cap`, `join`. Colors are `#RRGGBBAA`; `cap`/`join` are enums. |
| `shape.transform` | Per-shape 2D affine, decomposed so the inspector can edit fields individually. Same shape as the keyframe transform. |

### Formatting guarantees

The writer emits **stable, diff-friendly output**:

- 2-space indent, one field per line, trailing newline.
- Object keys in **insertion order** (not sorted alphabetically), so the layout
  is fixed by the schema rather than by hashing.
- Arrays whose elements are all scalars stay **inline** — a coordinate array
  reads `[100, 120]`, not six lines. Arrays containing objects go multi-line.
- Floats are rounded to **4 decimals in double precision**, so a save/load
  round-trip is byte-identical and diffs stay free of noise like
  `0.10000000149011612`.
- `Serialize()` is **idempotent**: `Deserialize(Serialize(d))` re-serializes to
  the identical bytes.

### Migration path

Every load runs `Migrate()` on the parsed JSON before binding, so a document of
any supported version is upgraded to the current one in exactly one place. v1 is
the first release, so `Migrate()` currently validates the version and passes
through; **adding v2 means one `case 1:` there**. A future document saved by a
newer Incogine is rejected with "update Incogine" rather than silently
mis-read.

## Data model

### Keyframes are sparse

A layer stores keyframes **only**; there is no separate span list to keep in
sync with the frames. The active keyframe for frame *N* is the nearest
keyframe at or before *N*, and the span end is the next keyframe after that.

A **blank keyframe** is an explicit keyframe with `kind: "blank"`: it holds the
timing but no artwork, so deleting an object's keyframe leaves a hole instead
of freezing the previous artwork.

### A keyframe holds a shape snapshot — deliberately

Each keyframe stores a copy of its layer's shape list. That duplication is
intentional: it makes the two planned tween types fall out for free later.

- **Motion Tween** — interpolate position / scale / rotation / alpha / color
  transform between two keyframes.
- **Shape Tween** — morph between two 2D vector shapes. `anim_geometry.h` already
  provides winding (`Winding()`) and point counts, and `shapeHints` records that
  a pair was prepared for morphing.

The cost is repeated identical shape data per keyframe. A shared symbol/instance
layer is the escape hatch if files grow too large; it is deliberately **not**
built speculatively.

### Tween spans are reserved

`keyframe.tweenIn` describes the span from the previous keyframe to this one:

| Field | Meaning |
|---|---|
| `type` | `"none"` (hold), `"motion"`, or `"shape"`. Motion and Shape Tween are **reserved** — parsed and round-tripped, not yet interpolated. |
| `easing` | Cubic-bezier control points. Sampling reuses the engine's own `cubicBezier()` from `src/core/engine/math.h`, so easing curves match splash/menu motion. |
| `motionFlags` | Named list of which properties a Motion Tween would interpolate. |
| `shapeHints` | Records that a span was prepared for shape morphing (point-count / winding matching). |

Until Milestone 7 every span is effectively `none`: the timeline model reserves
the span bar between keyframes, and nothing else changes.

## Runtime strategy: baked sprite sheet

**Decided:** at load time the runtime rasterizes each frame into a sprite sheet
/ texture atlas (configurable bake scale) and plays it back as a normal 2D
animated sprite.

- **Why:** keeps the runtime simple and fast, and reuses the engine's existing 2D
  quad rendering path rather than shipping a live vector rasterizer to every
  platform.
- **Trade-off:** this is **resolution-dependent**. Scaling a baked sprite up
  blurs it, unlike true vector rendering. The vector data is always retained in
  the file, so re-baking at a different `bakeScale` is possible — bake once per
  target resolution rather than per draw.
- **Live vector rendering at runtime is out of scope.**

The editor canvas renders via QPainter from the **same** `AnimPath` /
`AnimTransform` / `AnimStyle` geometry the rasterizer consumes — one shared 2D
representation for both — so the canvas and the baked output cannot drift apart.

Content outside the stage rect stays visible while editing but is **clipped on
export and in-game**.

## Asset system integration

`.incoanim` plugs into the asset pipeline through the generic importer interface,
not through a hardcoded suffix check:

```cpp
// src/core/assets/assetimport.h
enum class AssetKind { Unknown, Texture, Audio, Font, Animation2D, Text, Source };

class IAssetImporter {
    virtual AssetKind kind() const = 0;
    virtual const char* extension() const = 0;     // "incoanim"
    virtual const char* displayName() const = 0;   // asset-browser label
    virtual bool ImportBytes(const void* bytes, size_t size, std::string& error) = 0;
};
```

`ImportBytes` takes **bytes rather than a path** on purpose: Studio reads
authoring files from `src/assets/`, while the engine reads through
`AssetManager::Open()` (disk → `.incoba` bundle → embedded). The importer must
not care where the bytes came from, so mounted and bundled content works with
no special-casing.

`AssetImporterRegistry::RegisterBuiltins()` is the single switch point for
registering formats. Lookup normalizes extensions (lowercase, leading dot
stripped), so `.INCOANIM`, `incoanim`, and `IncoAnim` all match, and a dot in a
directory name is never mistaken for an extension.

## JSON is hand-rolled

`.incoanim` must be readable by the SDL3 runtime, which cannot depend on Qt, and
the repo carries no JSON library. `src/core/animation/anim_json.*` is a
stdlib-only value/parser/writer following the existing codec approach
(`incoba_detail.h`, `xml_util.h`) — no third-party dependency added.

It is deliberately small: it reads back what its writer produces. Object keys
keep insertion order rather than sorting, which is what makes the output
stable and diff-friendly.

## See also

- [Architecture](./architecture.md) — the `IncogineAssets` / `IncogineAnim` targets
- [Incogine Studio](./studio.md) — the IDE
- [Assets](./assets.md) — the AssetManager and `.incoba` bundles

## Acknowledgements

Incogine Animate stands on **SDL3** (the engine) and **Qt 6** (the editor), plus
[glad](../THIRD_PARTY.md) for OpenGL loading. Full details in
`THIRD_PARTY.md`.