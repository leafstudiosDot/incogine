---
title: "3D in Animator (design note, no implementation)"
description: How later milestones add 3D without rewriting the 2D timeline, renderer seam, cache, or file format.
---

# 3D in Animator — design note (M8+, no code yet)

2D stays the special case, not a parallel system. Everything below reuses a
seam that already exists; the flags mark what each current decision must not
break.

## What already exists (do not duplicate)

- **Math**: `Vec3` / `Mat4` in `src/core/render/camera_math.h`.
- **Camera**: `src/core/render/Camera` — 2D pixel-exact mode plus 3D orbit
  (perspective / orthographic / isometric), with `ScreenPointToRay` picking so
  gizmos agree with the renderer by construction. Not yet wired into
  `QuadRenderer` (tracked follow-up).
- **GL loading**: glad vendored (`src/core/render/`), desktop GL 3.3 core +
  GLES 3.0. The editor's future `QRhiWidget` canvas is a *second* context, not
  a replacement: engine GL and editor RHI coexist, sharing only Qt-free
  tessellation/geometry code.
- **Importer seam**: `IAssetImporter` + registry names FBX/OBJ/glTF/`.blend`
  as future formats. A 3D mesh importer registers there and produces a
  `MeshDocument`-style type, mirroring `Anim2DImporter`.
- **Container**: `.incoanim` v2 chunks skip unknown types, so 3D payloads
  ride new chunk types with zero changes to the v2 reader.

## Scene graph and transform model

- Keep `AnimTransform` 2D-only. When the first 3D shape type lands, add a
  **separate** `AnimTransform3D` (`position: Vec3`, `rotationEuler: Vec3`,
  `scale: Vec3`, alpha, `ToMatrix() -> Mat4`) rather than widening the 2D
  struct. Reason: every canvas hit-test, bound, flatten tolerance, and JSON
  line today assumes 2D; a shared struct would force branches everywhere.
- 2D content under a 3D camera is then just `z = 0, rotation = (0,0,rz)`.
  The timeline already resolves "nearest keyframe at or before N" independent
  of payload type, so keyframe *timing* needs no changes — only per-kind
  sampling (2D transform lerp vs 3D transform slerp/lerp, handled per kind).
- Layers carry a `kind` (vector / bitmap / mesh / camera — the 1.4 asset
  library's `kind` enum is the same field, kept open). A **camera layer**
  (After Effects-style) selects which `Camera` renders the composite; without
  one, the stage renders orthographic 2D exactly as today.

## Renderer (QRhi) with depth and camera

- Request a **depth-stencil buffer on the QRhi render target from day one**,
  even while all content is 2D. It costs nothing unused and avoids recreating
  the target (and every pipeline) when the first 3D layer lands.
- `SceneMesh` vertices gain `z` (default 0 from 2D producers — additive
  field, no migration of existing code paths beyond the struct).
- `IVectorRenderer::render` gains a camera (view + projection matrices);
  the 2D path passes ortho + identity view, i.e. today's transform.
- 3D mesh rendering is a second pipeline (lit or unlit vertices) fed by the
  mesh importer's baked vertex buffers, not by `BuildSceneMesh`. The two
  pipelines share the render target, the view uniforms, and the overlay pass.
- `grabFramebuffer()` keeps working for export/tests regardless of content.

## Cache and invalidation with 3D layers

- The RAM cache stores **images per (layer, frame) plus the composite** —
  never geometry — so a 3D layer is "just another image producer" (rendered
  with the camera layer's camera). No cache redesign: invalidation stays
  keyed on layer/frame ranges, eviction stays budget + LRU/distance.
- One new invalidation source: **camera moves** invalidate every 3D layer's
  cached frames (and nothing else). The dirty-range structure needs a
  camera-generation counter alongside layer/frame ranges — reserve the field
  when the cache lands, don't retrofit it.
- Per-layer caching (not composite-only) is what makes this cheap: a static
  2D background never re-renders because a 3D foreground orbits. This is a
  point in favor of layer-level caching in the 1.3 decision.

## Engine integration points (no code yet)

| Need | Plug-in point (exists today) |
|---|---|
| `.blend` / FBX / OBJ / glTF import | `IAssetImporter` registry (`RegisterBuiltins`) |
| Shipped 3D assets | `.incoba` bundles via `AssetManager::Open` (importers take bytes, never paths) |
| Runtime 3D playback | Baked meshes + keyframed `AnimTransform3D`, sampled like 2D |
| 3D file payloads | New `.incoanim` chunk types (reader skips unknown) |
| Editor viewport parity | Same `Camera` class the engine will use |

## Decisions in Part 1 that would make 3D harder (do not do these)

1. **Widening `AnimTransform` with optional Z fields.** Branches in every 2D
   path forever. Separate struct instead.
2. **A 2D-only scene mesh** (`TriVertex{x,y}` with no room for `z`, or a
   renderer interface without a camera slot). Add `z` + camera uniforms with
   the QRhi turn, defaulted for 2D.
3. **Composite-only RAM cache.** Kills the "static 2D bg + orbiting 3D fg"
   win and forces full re-renders on camera moves. Keep layer images.
4. **A closed asset `kind` enum.** 1.4's library must accept `mesh` (and
   later `camera`, `light`) without renumbering existing kinds.
5. **Renderer without depth from day one.** See above.
6. **Keyframes typed by payload in the timing logic.** `AtOrBefore`/spans
   stay payload-agnostic; only sampling branches per kind.
