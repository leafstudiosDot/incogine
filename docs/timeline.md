---
title: "Timeline (M4)"
description: Layers panel, frame grid, playhead, and playback for Incogine Animator.
---

# Timeline — M4

## Status: model done, UI pending

The timeline **editing model** is implemented and headless-tested
(`TestFrameOps` in `tests/anim_tests.cpp`, ~150 checks). The **UI** (layers
panel, frame grid, playback controls) is not built yet — this page is the
design it should follow.

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
- **Playback**: play/pause/stop + loop at document FPS via `QTimer`.
- **Frame ops UI**: F5/F6/F7/Shift+F5 shortcuts, right-click menu
  (insert/clear/copy/paste), all through the controller above.

## Measured: no frame cache needed

Playback frame-step cost (4 layers x 12 keyframes x 30 strokes, 120 frames):
**0.008 ms/step resolve-only** (5555x headroom at 24 fps), 0.36 ms/step even
with flattening redone. The document is fully in memory and the canvas caches
subdivisions per shape id, so the ±24-frame preload window idea is unnecessary
— verified by measurement, not kept as an option.

## Decided for M4

- **Onion skinning: previous/next frame only**, one ghost each side with alpha
  falloff. Smallest correct slice; multi-frame ranges come later.
- **Tween-span display: show as reserved.** The grid marks tween spans
  distinctly now (data exists, inert until M7), so M7 plugs behavior into
  already-visible UI.
- **Known wart**: keyframe promotion (`AddShapesCommand`) and F6 copy
  duplicate shape ids across keyframes. Benign today (caches clear on every
  edit and rebuild per frame, copies start identical), but the timeline
  switches frames *without* a document change — safe only because of that.
  If per-frame caches ever key on id alone, this must be fixed first
  (remap on copy, like paste already does).
