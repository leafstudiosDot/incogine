// Incogine Animator - vector renderer seam (raster today, QRhi next).
// Part of Incogine by leafstudiosDot (MPL-2.0). See LICENSE.
//
// The canvas paints through QPainter today. The GPU canvas will implement
// this same interface against QRhi, and switching backends then changes which
// implementation the window constructs - not the tools, the document, or the
// scene data. Timeline frames and onion-skinning plug in as additional
// SceneMesh inputs to render(), not as renderer changes.
//
// Data flow, identical for both backends:
//   AnimDocument -> drawList()/visibleList() (ResolvedShape, engine types)
//     -> anim_scene::BuildSceneMesh (one triangle soup, stage space)
//     -> IVectorRenderer::render(mesh, view, overlays)
//
// Overlay mapping (what each current QPainter overlay becomes in GL):
//   stage background / checkerboard -> fullscreen/stage quad + fragment shader
//     (checker via fract(), no texture upload)
//   committed strokes/fills         -> SceneMesh triangles (MSAA for edges)
//   in-progress brush preview       -> small dynamic vertex buffer, same shader
//   selection band trace            -> 1px line strip (core GL lines are 1px;
//     every cosmetic overlay in this editor already is)
//   selection boxes/handles, marquee rect, brush ring, pen anchors/handles
//     -> 1px line strips + small filled quads for handles
//   empty-hint / lock text          -> status-bar message (no text in GL)
//
// Pan/zoom are pure view uniforms (translate + scale); geometry rebuilds only
// when the document, frame, or quality changes - or past a zoom threshold, if
// device-space tessellation is ever adopted (see incoanim.md).
#pragma once

#include <cstdint>
#include <vector>

#include "animation/anim_geometry.h"
#include "animation/anim_scene.h"
#include "animation/anim_types.h"

struct RendererView {
    float zoom = 1.0f; // widget pixels per stage unit
    float offsetX = 0.0f;
    float offsetY = 0.0f;
    int viewportWidth = 1;
    int viewportHeight = 1;
};

// One cosmetic overlay path: a polyline strip (or loop when closed), 1px.
struct OverlayLine {
    std::vector<icg::anim::Vec2> points; // stage space
    icg::anim::AnimColor color = icg::anim::AnimColor(60, 140, 255, 255);
    bool closed = false;
    bool dashed = false;
};

// One filled overlay quad (selection handles, pen anchors): stage-space rect.
struct OverlayQuad {
    float x = 0.0f, y = 0.0f, w = 0.0f, h = 0.0f; // stage space
    icg::anim::AnimColor color = icg::anim::AnimColor(255, 255, 255, 255);
};

struct OverlayList {
    std::vector<OverlayLine> lines;
    std::vector<OverlayQuad> quads;
};

class IVectorRenderer {
  public:
    virtual ~IVectorRenderer() = default;

    // Draws one frame: the stage background, then the scene mesh, then the
    // overlays on top. Backends own their swapchain/surface handling; this
    // call only issues the scene graph for the given view.
    virtual void render(const icg::anim::SceneMesh& scene,
                        const RendererView& view,
                        const OverlayList& overlays) = 0;
};
