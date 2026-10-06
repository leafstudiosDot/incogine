// Incogine - 2D vector animation path types.
// Part of Incogine by leafstudiosDot (MPL-2.0). See LICENSE.
//
// Split out of anim_document.h so the dependency graph stays one-directional:
//
//   anim_types.h    (scalars, color, 2D affine, easing, tween spans)
//   anim_path.h     <- this file: the geometry a path is made of
//   anim_geometry.h (flattening, bounds, hit tests - needs the path)
//   anim_document.h (layers, keyframes, shapes - needs geometry)
//
// A path is a segment list. Segment kinds map 1:1 onto QPainterPath calls in
// the editor and onto the software rasterizer, so all three consume the same
// geometry.
#pragma once

#include <vector>

#include "anim_types.h"

namespace icg {
namespace anim {

struct AnimSegment {
    enum class Kind {
        Move,  // start a new subpath at p[0]
        Line,  // p[0] = end point
        Cubic, // p[0] = c1, p[1] = c2, p[2] = end point
        Close, // close the current subpath
    };

    Kind kind = Kind::Line;
    // Line: p[0] = end. Cubic: p[0] = c1, p[1] = c2, p[2] = end.
    // Move: p[0] = position. Close: unused.
    Vec2 p[3];

    AnimSegment() = default;
    explicit AnimSegment(Kind segmentKind) : kind(segmentKind) {}
};

struct AnimPath {
    std::vector<AnimSegment> segments;

    bool IsEmpty() const { return segments.empty(); }
    // True when the path ends on a Close segment.
    bool IsClosed() const {
        return !segments.empty() && segments.back().kind == AnimSegment::Kind::Close;
    }

    // Axis-aligned rectangle, closed.
    static AnimPath FromRect(float x, float y, float w, float h);
    // Ellipse as four cubic Beziers, closed.
    static AnimPath FromEllipse(float cx, float cy, float rx, float ry);
};

} // namespace anim
} // namespace icg
