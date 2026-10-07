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

**M4:** the timeline — layers panel, frame grid, playhead, playback. The
editing model (frame/insert/remove/keyframe/clear/paste commands) is done and
tested; see [Timeline](./timeline.md) for the UI design. Frame-step resolve
cost measured at 0.008 ms (no preload cache needed).

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
- **Viewport culling.** Shapes whose stroke-inflated bounds miss the visible
  stage rect skip path AND raster work entirely. The pad covers the stroke
  halo plus antialiasing, so culling can never clip a visible pixel -
  artwork is untouched, only fully-offscreen shapes are skipped. Marquee
  selection and selection handles use the same stroke-inflated box, so wide
  strokes select by their rendered pixels, not their centerline.
- **Bounded checkerboard.** The transparency grid draws in fixed 8px widget
  cells clipped to the stage, so its cost is bounded by the viewport at any
  zoom (stage-space cells exploded into hundreds of thousands of rects
  zoomed in).
- **One draw list per paint.** The culled draw list is built once and shared by
  every painter in a paint pass, instead of each rebuilding its own deep copy.

### Known cost: Qt's raster stroker

Painting a dense stroke is dominated by Qt's raster `QPainterPathStroker`, at
roughly **20us per vertex**, scaling with the stroke band's **device-space**
width - so it gets worse as you zoom in. Measured on a fitted brush stroke
(12 strokes, 3648 flattened vertices, pen width 4, 1200x800):

| Operation | Cost |
|---|---|
| `NoPen` fill of the same geometry | ~0.1 ms |
| Stroked, antialiasing off | no faster than AA on |
| Stroked, 3648 verts | 43 ms (zoom 1) → 173 ms (zoom 8) |

Rasterization is essentially free; the stroker is the whole cost. Turning off
antialiasing, switching join styles, and culling offscreen shapes do not
address it.

Decimating the centerline before the stroker was implemented, measured, and
**rejected** (`DecimateForStroke` keeps the geometry helper, unused, with the
numbers recorded). There is no useful operating point:

| Tolerance | Verts kept | Speedup | Worst-case pixel error |
|---|---|---|---|
| 0.004 | 92.5% | 1.0x | max 90, 22 visibly wrong px |
| 0.008 | 87.5% | 1.0x | max 132, 188 bad px |
| 0.020 | 73.0% | 1.0–1.2x | max 333, 1906 bad px |
| 0.080 | 34.9% | 1.4–1.7x | max 504, 8548 bad px |

Tolerances small enough to preserve the artwork buy no speedup; the ones that
buy speedup damage it. The vertices decimation removes are the same ones the
stroker needs to resolve sub-pixel detail into smooth edges.

GPU path (in progress): the raster canvas stays until the GPU canvas proves
itself. The seam is landed: `anim_scene::BuildSceneMesh()` turns the same
resolved shapes the canvas paints into one stage-space triangle soup (fills
triangulated directly, strokes via the same union-correct pieces, per-vertex
colors), and `src/studio/animator/renderer.h` defines the `IVectorRenderer`
contract both backends implement (scene mesh + view + overlays; timeline
frames and onion-skinning plug in as extra meshes). Pan/zoom stay pure view
uniforms with zero geometry work. Backend choice: **QRhi** (`QRhiWidget`,
Vulkan/Metal/D3D/GL, MSAA) over `QOpenGLWidget` (QPainter vectors stay
CPU-side there, and macOS GL is deprecated) and custom Vulkan (duplicates Qt
and the engine's glad-GL for no win). Moving the preview off QPainter weakens
the "preview matches bake" guarantee, so the mesh builder mirrors the canvas
paint rules exactly (same flattening, same pieces, same winding).

### Fix adopted: strokes are filled pieces, not pen strokes

The fix taken was to stop asking the rasterizer to stroke at all.
`anim_geometry::StrokeToPieces()` expands a centerline into **union-correct fill
pieces** - one convex quad per segment, a disc at every round joint and round
cap, bevel/miter triangles elsewhere - all sharing one winding, so a single
`WindingFill` paints their exact union. This is what Flash/Animate do - their
brush produces a filled shape, not a stroked path. (An earlier single-outline-
loop version, `StrokeToOutline()`, is kept for the selection highlight, where
only its edges are stroked and winding never applies.)

Measured with the shipped engine code, 12 fitted brush strokes (3648 centerline
vertices, pen width 4, 1200x800):

| Zoom | Qt stroker | Pieces fill | Speedup |
|---|---|---|---|
| 1 | 72.4 ms | 14.1 ms | **5.2x** |
| 2 | 122.3 ms | 21.1 ms | **5.8x** |
| 4 | 165.2 ms | 16.0 ms | **10.3x** |
| 8 | 283.7 ms | 23.1 ms | **12.3x** |
| 16 | 100.5 ms | 15.8 ms | **6.4x** |

The win **grows with zoom**, which is the whole point: the old cost scaled with
the device-space band width. The pieces are cached per shape (rebuilt only when
the transform, width, cap or join changes), so panning and zooming reuse them
entirely.

Fills must use `WindingFill`, not even-odd: the pieces share one winding by
construction (a `sharedWinding` test enforces it - one inconsistent triangle is
enough to punch a hole), and only the winding rule paints their union solid.

### Self-overlap holes, fixed by pieces

The single-loop outline cancelled to zero where a stroke crossed itself: a brush
circle's overlap read as subtracted/masked. Measured on a 380-degree circle
(pen width 10), missing pixels (reference painted, outline left white):

| Zoom | Single loop (winding) | Pieces (winding) |
|---|---|---|
| 1 | 99 missing | **6 missing** |
| 4 | 1418 missing | **17 missing** |

The residual single-digit misses are edge antialiasing, not holes (mean
difference 0.03/765). Pieces also removed the old self-intersection darkening
difference: an opaque stroke no longer darkens where it overlaps itself, and a
non-crossing stroke is pixel-identical to the stroker.

### Preview quality (the zoom lever)

`View > Preview Quality` (persisted in `QSettings`, default Normal):

| Level | Antialiasing | Curve tolerance | Effect at zoom 4-8 |
|---|---|---|---|
| Draft | off | 4x (1.0) | **~12-18x faster** than Normal |
| Normal | on | 1x (0.25) | default |
| High | on | 0.5x (0.125) | slower, smoother curves up close |

Draft is the answer to zoomed-in lag on dense brushwork: it attacks both zoom
costs at once (fill-rate via no AA, tessellation via fewer vertices). It is a
preview-only tradeoff - the stored vector data is untouched, and switching back
to Normal repaints full fidelity. The canvas previously painted with no
antialiasing at all (QPainter's default); Normal now enables it explicitly.

### Pixel fidelity, and one intentional difference

Measured against Qt's stroker, worst case:

- A stroke that does **not** cross itself: **pixel-identical** (0 differing
  pixels, mean difference 0.007/765).
- A stroke that **does** cross itself: ~1.7% of painted pixels differ by more
  than 3% per channel.

The cause is understood, and the outline is the more correct of the two: Qt's
stroker composites each overlapping segment separately, so a self-intersection
is drawn darker than the surrounding stroke, while one filled polygon paints it
uniform. An opaque stroke should not darken where it overlaps itself. The
difference is therefore a rendering behavior change at self-intersections, not a
regression - but it is visible, so it is recorded here rather than buried.

Tightening the arc tolerance barely helps (25x tighter moves 1487 differing
pixels to 1179), confirming the residual is structural rather than tessellation.
The default tolerance is kept because it is also the cheapest.

### Cuts in strokes, and why zoom made them worse

The first outline version produced visible gaps ("cuts") in brush strokes. The
cause was not the outline code alone but an interaction with how the brush
samples input.

The brush sampled every ~2 **screen** px, which is right for constant visual
resolution - but in **stage** units that spacing shrinks as `2 / zoom`. At zoom
64 it was ~0.03 stage units against a 4-unit stroke: input ~130x denser than
the stroke is wide.

Dense input breaks a round join. The join sweeps an arc whose extent along each
adjacent segment is `radius * tan(sweep / 2)`. Once neighbouring points are
closer together than that, the arc runs *past* the next vertex; the outline then
travels forward beyond a vertex and folds back to it, and under the winding rule
the fold cancels to zero and opens a hole. Measured by probing every point along
the centerline with a winding-rule point-in test:

| Input spacing (stroke width 4, radius 2) | Centerline outside the band |
|---|---|
| much greater than width | 0.00% |
| about equal to width | 0.00% |
| less than width/4 | 0.37% |
| ~0.2 units (zoom-64 brush, before fix) | **8.8%** |

Two fixes, both needed:

1. **The join arc is clamped** so it cannot overshoot its adjacent vertices
   (`radius * tan(sweep/2) <= min(adjacent segment lengths)`). This is free
   visually: a clamped arc only ever occurs on micro-vertices whose neighbours
   are closer than the stroke is wide, where the full arc was buried in the band
   anyway. 8.8% -> 0.4%.
2. **The brush floors its sample spacing** at `strokeWidth * 0.2` stage units
   (`BrushTool::sampleSpacing`). Below that floor extra points carry no visible
   detail. This also bounds vertex count independently of zoom, so zooming in no
   longer costs more to draw - the same input that caused the cuts was causing
   the zoomed-in slowdown.

At the floored density the stroke measures **0.06%** centerline holes (2 probes
of 3366, both at the cap), and a 300-unit stroke carries ~26x fewer vertices at
zoom 64 than before.
- **Brush (`B`)**: a freeform Pen — drag to paint a Flash-style brush stroke.
  Input is throttled to ~2 screen px (floored at 20% of the brush width so zoom
  cannot make it arbitrarily dense), fitted to error-bounded Beziers on release
  (least-squares cubics, longest-within-tolerance wins), and stored as the
  compact **centerline + width** (~two dozen segments / ~5 kB for a 100-point
  stroke). Storing the tessellated fill pieces instead multiplied every
  downstream cost by ~500x (12.5k segments / 1.3 MB per stroke), so the model
  keeps source geometry and the canvas tessellates transiently (flatten +
  pieces caches, union-correct, no holes on overlap, never a pen stroke). The
  smoothing slider IS the fit tolerance, so the knob is honest: curves stay
  within it of the drawn input. A bare click makes a filled dot in the stroke
  color. Size (stage units), smoothing, color, and opacity come from the tool
  options strip; `Esc` cancels a stroke. The live preview expands the raw input
  as fill pieces (no QPen stroking, which was the while-drawing lag); the
  committed shape is slightly smoother, matching Flash's ink-then-smooth feel.
- **Draw-list caching**: the resolved draw list is built once per model/frame/
  quality change and shared by every painter and hit-test in a paint, instead
  of deep-copying every FlatPath per frame (29.6 ms → 0.1 ms at 100 old-size
  strokes; ~300x). Live drags invalidate it explicitly since they bypass the
  command stack.
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

## File format (v2 container; v1 JSON still loads)

Saves always write the **v2 container**: a chunked binary file (same
`.incoanim` extension) with a JSON manifest, per-layer/per-frame binary stroke
chunks, DEFLATE-compressed payloads, and an index for lazy loading. v1 files
(a single pretty JSON document, schema below) keep loading unchanged and
migrate the first time they are saved.

Measured on 30px brush strokes (compact centerline storage): 1 stroke 5 kB
JSON -> 1 kB container (4.6x); 500 strokes 1.99 MB -> 238 kB (8.4x), saving in
21 ms and loading in 3 ms. Compression is vendored miniz at its default level
(Qt-free, so the game runtime uses the same code); see `THIRD_PARTY.md`.

### v2 layout (all integers little-endian)

```
header:  "INCOANIM2" (8B) | u16 containerVersion=2 | u16 flags=0 | u32 chunkCount
chunk:   u32 type | u32 flags (bit0 = DEFLATE) | u32 unpackedLen | u32 packedLen
         | payload[packedLen]
footer:  "INCOANIM$" (8B) | u64 indexChunkOffset (from file start)
```

Chunk types: `MHDR` (manifest.json, UTF-8, stored **uncompressed** so it stays
readable in a hex dump), `STRO` (one layer's keyframe: layer id, frame, kind,
keyframe transform + tween, then shapes as id, name, style flags/colors/width/
cap/join, transform, and segments as kind byte + f32 coordinates), `ASET`
(embedded asset path + bytes; reserved, nothing embeds yet), `INDX` (index:
type, file offset, packed length, layer id, frame per chunk).

Rules the code enforces: unknown chunk types are **skipped** (forward
compatibility); every read is bounds-checked (truncation is an error, never a
crash); a future container version is rejected with a message naming both
versions; saves stay atomic (temp file + rename, as before). `LoadManifest`
reads only header + manifest + index (fast project browsing); `LoadLayerFrame`
seeks the index straight to one frame's chunk (the lazy-load primitive the
timeline will use). Implementation: `anim_container.*`; the asset importer
(`anim2d_importer`) loads both formats through `DeserializeBytes`, so shipped
games read old and new files with no asset changes.

### v1 JSON schema (read-only legacy)

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