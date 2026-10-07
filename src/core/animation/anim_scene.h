// Incogine - GPU-ready scene mesh for 2D vector animation.
// Part of Incogine by leafstudiosDot (MPL-2.0). See LICENSE.
//
// The bridge between the vector model and ANY rasterizer: resolved shapes go
// in (the same ResolvedShape list the canvas paints), one triangle soup in
// stage space comes out. The Qt raster canvas keeps filling QPainterPaths
// today; the QRhi canvas (next) uploads this mesh to static vertex buffers -
// one buffer rebuild per document/frame/quality change, then pan/zoom are
// pure uniform updates with zero geometry work. Timeline frames and
// onion-skinning plug in by building one mesh per frame.
//
// Design rules, so preview cannot drift from bake:
//   * Fills triangulate the flattened subpaths directly (implicitly closed,
//     exactly like bakedPath's fill).
//   * Strokes triangulate the StrokeToPieces output (union-correct, solid on
//     self-overlap), never a pen stroke.
//   * Colors are per-vertex bytes; the mesh carries no renderer state.

#pragma once

#include <cstdint>
#include <vector>

#include "anim_document.h"

namespace icg {
namespace anim {

// One triangle-soup vertex: stage-space position + straight-alpha color.
struct TriVertex {
    float x = 0.0f;
    float y = 0.0f;
    uint8_t r = 0;
    uint8_t g = 0;
    uint8_t b = 0;
    uint8_t a = 0;
};

// The whole visible scene as triangles. vertices.size() is a multiple of 3;
// an empty mesh paints nothing.
struct SceneMesh {
    std::vector<TriVertex> vertices;
};

// Builds the mesh for `shapes` (draw order preserved: later shapes cover
// earlier ones under opaque paint, exactly like the raster canvas).
// `tolerance` is the flatten/arc tolerance in shape-local units - the same
// knob as the canvas preview quality, so both backends tessellate identically.
void BuildSceneMesh(const std::vector<ResolvedShape>& shapes, float tolerance,
                    SceneMesh& out);

} // namespace anim
} // namespace icg
