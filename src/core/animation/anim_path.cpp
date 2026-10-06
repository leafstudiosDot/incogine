#include "anim_path.h"

namespace icg {
namespace anim {

AnimPath AnimPath::FromRect(float x, float y, float w, float h) {
    AnimPath path;
    path.segments.reserve(5);

    AnimSegment move(AnimSegment::Kind::Move);
    move.p[0] = Vec2(x, y);
    path.segments.push_back(move);

    const float points[3][2] = {{x + w, y}, {x + w, y + h}, {x, y + h}};
    for (const auto& point : points) {
        AnimSegment line(AnimSegment::Kind::Line);
        line.p[0] = Vec2(point[0], point[1]);
        path.segments.push_back(line);
    }

    path.segments.push_back(AnimSegment(AnimSegment::Kind::Close));
    return path;
}

AnimPath AnimPath::FromEllipse(float cx, float cy, float rx, float ry) {
    // Magic constant for approximating a quarter circle with a cubic Bezier.
    // The result is a polyline that sits marginally INSIDE the true curve
    // (the chords cut the corner), which is why picking uses a tolerance.
    const float k = 0.5522847498f;

    AnimPath path;
    path.segments.reserve(5);

    AnimSegment move(AnimSegment::Kind::Move);
    move.p[0] = Vec2(cx, cy + ry);
    path.segments.push_back(move);

    struct Arc {
        Vec2 c1, c2, end;
    };
    const Arc arcs[4] = {
        {Vec2(cx + rx * k, cy + ry), Vec2(cx + rx, cy + ry * k), Vec2(cx + rx, cy)},
        {Vec2(cx + rx, cy - ry * k), Vec2(cx + rx * k, cy - ry), Vec2(cx, cy - ry)},
        {Vec2(cx - rx * k, cy - ry), Vec2(cx - rx, cy - ry * k), Vec2(cx - rx, cy)},
        {Vec2(cx - rx, cy + ry * k), Vec2(cx - rx * k, cy + ry), Vec2(cx, cy + ry)},
    };
    for (const Arc& arc : arcs) {
        AnimSegment cubic(AnimSegment::Kind::Cubic);
        cubic.p[0] = arc.c1;
        cubic.p[1] = arc.c2;
        cubic.p[2] = arc.end;
        path.segments.push_back(cubic);
    }

    path.segments.push_back(AnimSegment(AnimSegment::Kind::Close));
    return path;
}

} // namespace anim
} // namespace icg