---
title: "Timeline (M4)"
description: Layers panel, frame grid, playhead, and playback for Incogine Animator.
---

# Timeline — M4

## Status: model + UI built (this milestone)

The timeline **editing model** is implemented and headless-tested
(`TestFrameOps`, ~150 checks): frame/insert/remove/keyframe/clear/paste
commands plus `SetLayerColor`/`SetLayerOutline`. The **UI** is built:
bottom-dock `TimelineWidget` (transport bar + custom grid + scrollbars),
frame ops via F5/F6/F7/Shift+F5/Del/Ctrl+C/V, context menus, layer add/delete/
rename/reorder/eye/lock/outline/color, work area (Shift+drag ruler, loop
respects it), tween spans hatched as reserved, cache strip showing 1.3 cached
frames, prev/next onion-skin toggle. Clipboard holds a single keyframe (ranges later).

## Editing model (done)

All frame ops are single undo steps through `AnimCommandStack`, refuse cleanly
when there is nothing to do, and round-trip through undo *and* redo:

| Operation (Flash key) | Command | Semantics |
|---|---|---|
| Insert Frame (F5) | `InsertFramesCommand` | All layers: keys at/after shift right, timeline grows. Always succeeds (extending is the edit). Exactly invertible, stores only the old length. |
| Remove Frame (Shift+F5) | `RemoveFramesCommand` | All layers: range content **deleted**, tail shifts left, timeline shrinks (floor 1). Undo snapshots touched layers only. Deleting first keeps the shift bijective — no merge can ever eat a keyframe. |
| Insert Keyframe (F6) | `InsertKeyframeCommand` | Copies the span's current artwork (nearest key at/before). Fails if one sits there. |
| Insert Blank Keyframe (F7) | `InsertKeyframeCommand` | Empty blank key. Same refusals. |
| Clear Keyframe | `ClearKeyframeCommand` | Removes the key; the span falls back. Undo restores it exactly. |
| Paste Frames | `PasteFramesCommand` | Relative offsets preserved, replaces destinations, out-of-range skipped, pasted shapes get **fresh ids** (remapped once, so redo is deterministic). |

Controller: `AnimatorDocument::insertFrames/removeFrames/insertKeyframe/
clearKeyframe/pasteFrames`. The copy clipboard lives in the timeline widget
(a read-only model copy); single-layer ranges for M4, multi-layer later.

## UI to build (M4 proper)

- **Layers panel**: add/delete/rename/reorder/show-hide/lock. All commands
  already exist (`Add/Delete/Rename/SetVisible/SetLocked/MoveLayer`); the
  panel is pure wiring plus the controller methods above.
- **Frame grid**: rows = layers, columns = frames; key vs blank glyphs;
  span shading from `AtOrBefore` runs; tween-span reservation display from
  `TweenSpan` (reserved data, sampled in M7).
- **Playhead**: click/drag to scrub, wired to the existing
  `AnimatorCanvas::setCurrentFrame()`.
- **Playback**: play/pause/stop + loop at document FPS via `QTimer` (ticks
  retime when FPS changes mid-playback).
- **Frame ops UI**: F5/F6/F7/Shift+F5 shortcuts, right-click menu
  (insert/clear/copy/paste), all through the controller above.

## Measured: resolve is cheap, raster is what the cache buys

Playback frame-step cost (4 layers x 12 keyframes x 30 strokes, 120 frames):
**0.008 ms/step resolve-only** (5555x headroom at 24 fps), 0.36 ms/step even
with flattening redone. The document is fully in memory and the canvas caches
subdivisions per shape id, so the ±24-frame preload window idea is unnecessary
— verified by measurement, not kept as an option.

That measurement is resolve-only (model queries, no pixels). Rasterizing is
the per-frame cost that actually scales with stroke count, which is what 1.3's
RAM frame cache addresses (see `docs/incoanim.md`): composites blit during
playback, precise invalidation per command footprint, worker prefetch ahead of
the playhead. The cache strip above the ruler shows which frames hold baked
composites.

## Decided for M4

- **Onion skinning: previous/next frame only**, one ghost each side with alpha
  falloff. Smallest correct slice; multi-frame ranges come later.
- **Tween-span display: show as reserved.** The grid marks tween spans
  distinctly now (data exists, inert until M7), so M7 plugs behavior into
  already-visible UI.
- **Known wart**: keyframe promotion (`AddShapesCommand`) and F6 copy
  duplicate shape ids across keyframes. Benign today and under the 1.3 frame
  cache: its layer images key on (layer id, frame), never on shape id — each
  entry rasterizes whatever keyframe resolves at its frame, so duplicated ids
  cannot alias pixels. (The canvas subdivision cache keys on shape id, but it
  clears on every edit and rebuilds per frame, so the same reasoning holds.)
  If a future cache ever keys pixels on shape id alone, this must be fixed
  first (remap on copy, like paste already does).
