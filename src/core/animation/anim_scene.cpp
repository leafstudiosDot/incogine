// Incogine - GPU-ready scene mesh for 2D vector animation.
// Part of Incogine by leafstudiosDot (MPL-2.0). See LICENSE.

#include "anim_scene.h"

namespace icg {
namespace anim {
namespace {

void EmitTris(SceneMesh& out, const std::vector<Vec2>& tris,
              const Mat2x3& matrix, const AnimColor& color) {
    for (size_t i = 0; i + 2 < tris.size(); i += 3) {
        for (int k = 0; k < 3; ++k) {
            const Vec2 stage = TransformPoint(matrix, tris[i + k]);
            TriVertex v;
            v.x = stage.x;
            v.y = stage.y;
            v.r = static_cast<uint8_t>(color.r);
            v.g = static_cast<uint8_t>(color.g);
            v.b = static_cast<uint8_t>(color.b);
            v.a = static_cast<uint8_t>(color.a);
            out.vertices.push_back(v);
        }
    }
}

} // namespace

void BuildSceneMesh(const std::vector<ResolvedShape>& shapes, float tolerance,
                    SceneMesh& out) {
    out.vertices.clear();
    StrokeOutlineOptions opts;
    opts.tolerance = std::max(0.0f, tolerance);
    for (const ResolvedShape& shape : shapes) {
        if (!shape.drawable) {
            continue;
        }
        if (shape.hasFill && shape.fill.a > 0) {
            for (const std::vector<Vec2>& poly : shape.path.polylines) {
                std::vector<Vec2> tris;
                if (poly.size() >= 3 && TriangulatePolygon(poly, tris)) {
                    EmitTris(out, tris, shape.matrix, shape.fill);
                }
            }
        }
        if (shape.hasStroke && shape.stroke.a > 0 && shape.strokeWidth > 0.0f) {
            opts.width = shape.strokeWidth;
            opts.cap = shape.cap;
            opts.join = shape.join;
            const FlatPath pieces = StrokeToPieces(shape.path, opts);
            for (const std::vector<Vec2>& poly : pieces.polylines) {
                std::vector<Vec2> tris;
                if (poly.size() >= 3 && TriangulatePolygon(poly, tris)) {
                    EmitTris(out, tris, shape.matrix, shape.stroke);
                }
            }
        }
    }
}

} // namespace anim
} // namespace icg
