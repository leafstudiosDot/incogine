#include "anim_geometry.h"

#include <algorithm>
#include <cmath>

#include "anim_path.h"

namespace icg {
namespace anim {
namespace {

// --- polyline helpers --

float DistanceSq(const Vec2& a, const Vec2& b) {
    const float dx = a.x - b.x;
    const float dy = a.y - b.y;
    return dx * dx + dy * dy;
}

// Closest point on segment ab to p, as a parameter in [0, 1].
float ClosestParamOnSegment(const Vec2& p, const Vec2& a, const Vec2& b) {
    const Vec2 ab = b - a;
    const float lenSq = Dot(ab, ab);
    if (lenSq < 1e-12f) {
        return 0.0f;
    }
    const float t = Dot(p - a, ab) / lenSq;
    return std::min(1.0f, std::max(0.0f, t));
}

Vec2 PointOnSegment(const Vec2& a, const Vec2& b, float t) {
    return {a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t};
}

float PointSegmentDistanceSq(const Vec2& p, const Vec2& a, const Vec2& b) {
    return DistanceSq(p, PointOnSegment(a, b, ClosestParamOnSegment(p, a, b)));
}

// Signed area x2 of a polyline (implicitly closed). Screen space is y-down, so
// positive area reads as counter-clockwise on screen.
float SignedAreaX2(const std::vector<Vec2>& polyline) {
    const size_t n = polyline.size();
    if (n < 3) {
        return 0.0f;
    }
    float sum = 0.0f;
    for (size_t i = 0; i < n; ++i) {
        const Vec2& a = polyline[i];
        const Vec2& b = polyline[(i + 1) % n];
        sum += a.x * b.y - b.x * a.y;
    }
    return sum;
}

// Recursive Douglas-Peucker.
void SimplifyRange(const std::vector<Vec2>& points, size_t first, size_t last,
                   float tolerance, std::vector<bool>& keep) {
    if (last <= first + 1) {
        return;
    }
    float worst = -1.0f;
    size_t worstIndex = first;
    for (size_t i = first + 1; i < last; ++i) {
        const float d = PointSegmentDistanceSq(points[i], points[first], points[last]);
        if (d > worst) {
            worst = d;
            worstIndex = i;
        }
    }
    if (worst <= tolerance * tolerance) {
        return;
    }
    keep[worstIndex] = true;
    SimplifyRange(points, first, worstIndex, tolerance, keep);
    SimplifyRange(points, worstIndex, last, tolerance, keep);
}

} // namespace

// ------------------------------------------------------------- flattening --

void FlattenCubic(const Vec2& p0, const Vec2& c1, const Vec2& c2, const Vec2& p1,
                  float tolerance, std::vector<Vec2>& out) {
    const float tol = std::max(1e-3f, tolerance);
    // Control polygon length upper-bounds the curve length, so it is a safe
    // basis for choosing a step count.
    const float polyLen = Distance(p0, c1) + Distance(c1, c2) + Distance(c2, p1);
    int steps = static_cast<int>(std::sqrt(polyLen / tol) * 2.0f) + 2;
    steps = std::min(256, std::max(2, steps));

    for (int i = 1; i <= steps; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(steps);
        const float u = 1.0f - t;
        const float w0 = u * u * u;
        const float w1 = 3.0f * u * u * t;
        const float w2 = 3.0f * u * t * t;
        const float w3 = t * t * t;
        out.push_back({p0.x * w0 + c1.x * w1 + c2.x * w2 + p1.x * w3,
                       p0.y * w0 + c1.y * w1 + c2.y * w2 + p1.y * w3});
    }
}

FlatPath Flatten(const AnimPath& path, float tolerance) {
    FlatPath flat;
    std::vector<Vec2> current;
    Vec2 cursor;

    auto flush = [&flat, &current](bool closed) {
        if (current.size() >= 2) {
            flat.polylines.push_back(current);
            flat.closed.push_back(closed);
        }
        current.clear();
    };

    for (const AnimSegment& segment : path.segments) {
        switch (segment.kind) {
            case AnimSegment::Kind::Move:
                flush(false);
                cursor = segment.p[0];
                current.push_back(cursor);
                break;
            case AnimSegment::Kind::Line:
                if (current.empty()) {
                    current.push_back(cursor);
                }
                cursor = segment.p[0];
                current.push_back(cursor);
                break;
            case AnimSegment::Kind::Cubic:
                if (current.empty()) {
                    current.push_back(cursor);
                }
                FlattenCubic(cursor, segment.p[0], segment.p[1], segment.p[2],
                             tolerance, current);
                cursor = segment.p[2];
                break;
            case AnimSegment::Kind::Close:
                flush(true);
                cursor = Vec2();
                break;
        }
    }
    flush(false);
    return flat;
}

// ----------------------------------------------------------------- bounds --

void PathBounds(const AnimPath& path, Vec2& minOut, Vec2& maxOut) {
    bool any = false;
    auto include = [&](const Vec2& p) {
        if (!any) {
            minOut = p;
            maxOut = p;
            any = true;
            return;
        }
        minOut.x = std::min(minOut.x, p.x);
        minOut.y = std::min(minOut.y, p.y);
        maxOut.x = std::max(maxOut.x, p.x);
        maxOut.y = std::max(maxOut.y, p.y);
    };

    for (const AnimSegment& segment : path.segments) {
        switch (segment.kind) {
            case AnimSegment::Kind::Move:
                include(segment.p[0]);
                break;
            case AnimSegment::Kind::Line:
                include(segment.p[0]);
                break;
            case AnimSegment::Kind::Cubic:
                // Control points give a conservative box for free; a tight box
                // would need derivative root-finding per axis.
                include(segment.p[0]);
                include(segment.p[1]);
                include(segment.p[2]);
                break;
            case AnimSegment::Kind::Close:
                break;
        }
    }
    if (!any) {
        minOut = Vec2();
        maxOut = Vec2();
        maxOut.x = -1.0f; // empty marker: minOut > maxOut
        maxOut.y = -1.0f;
    }
}

void FlatBounds(const FlatPath& flat, Vec2& minOut, Vec2& maxOut) {
    bool any = false;
    for (const auto& polyline : flat.polylines) {
        for (const Vec2& p : polyline) {
            if (!any) {
                minOut = p;
                maxOut = p;
                any = true;
                continue;
            }
            minOut.x = std::min(minOut.x, p.x);
            minOut.y = std::min(minOut.y, p.y);
            maxOut.x = std::max(maxOut.x, p.x);
            maxOut.y = std::max(maxOut.y, p.y);
        }
    }
    if (!any) {
        minOut = Vec2();
        maxOut = Vec2(-1.0f, -1.0f);
    }
}

void FlatTransformedBounds(const FlatPath& flat, const Mat2x3& matrix,
                           float pad, Vec2& minOut, Vec2& maxOut) {
    const float inset = std::max(0.0f, pad);
    bool any = false;
    for (const auto& polyline : flat.polylines) {
        for (const Vec2& p : polyline) {
            const Vec2 q = TransformPoint(matrix, p);
            if (!any) {
                minOut = q;
                maxOut = q;
                any = true;
                continue;
            }
            minOut.x = std::min(minOut.x, q.x);
            minOut.y = std::min(minOut.y, q.y);
            maxOut.x = std::max(maxOut.x, q.x);
            maxOut.y = std::max(maxOut.y, q.y);
        }
    }
    if (!any) {
        minOut = Vec2();
        maxOut = Vec2(-1.0f, -1.0f);
        return;
    }
    minOut.x -= inset;
    minOut.y -= inset;
    maxOut.x += inset;
    maxOut.y += inset;
}

// ---------------------------------------------------------------- winding --

int Winding(const std::vector<Vec2>& polyline) {
    // Screen space is y-down, so flip the sign relative to math convention.
    const float area = SignedAreaX2(polyline);
    if (area > 1e-6f) {
        return -1; // counter-clockwise on screen
    }
    if (area < -1e-6f) {
        return 1; // clockwise on screen
    }
    return 0;
}

// -------------------------------------------------------------- hit tests --

bool PointInPolyline(const Vec2& point, const std::vector<Vec2>& polyline,
                     float tolerance) {
    if (polyline.size() < 3) {
        return false;
    }
    // Even-odd ray cast to +x.
    bool inside = false;
    const size_t n = polyline.size();
    for (size_t i = 0, j = n - 1; i < n; j = i++) {
        const Vec2& a = polyline[i];
        const Vec2& b = polyline[j];
        if ((a.y > point.y) != (b.y > point.y)) {
            const float x = a.x + (b.x - a.x) * (point.y - a.y) / (b.y - a.y);
            if (point.x < x) {
                inside = !inside;
            }
        }
    }
    if (!inside) {
        return false;
    }
    if (tolerance > 0.0f) {
        // Grow the hit area by snapping to the nearest edge when close.
        for (size_t i = 0, j = n - 1; i < n; j = i++) {
            if (PointSegmentDistanceSq(point, polyline[j], polyline[i]) <=
                tolerance * tolerance) {
                return true;
            }
        }
    }
    return true;
}

bool PointInFlatPath(const Vec2& point, const FlatPath& flat, float tolerance) {
    for (const auto& polyline : flat.polylines) {
        if (PointInPolyline(point, polyline, tolerance)) {
            return true;
        }
    }
    return false;
}

// True when the point is within `distance` of the outline (in or out), used
// for picking near a curved edge.
bool PointNearFlatPath(const Vec2& point, const FlatPath& flat, float distance) {
    for (const auto& polyline : flat.polylines) {
        Vec2 nearest;
        if (DistanceToPolyline(point, polyline, nearest) <= distance) {
            return true;
        }
    }
    return false;
}

float DistanceToPolyline(const Vec2& point, const std::vector<Vec2>& polyline,
                         Vec2& nearest) {
    if (polyline.empty()) {
        nearest = point;
        return 0.0f;
    }
    if (polyline.size() == 1) {
        nearest = polyline[0];
        return Distance(point, nearest);
    }
    float best = -1.0f;
    const size_t edges = polyline.size() - 1;
    for (size_t i = 0; i < edges; ++i) {
        const float t = ClosestParamOnSegment(point, polyline[i], polyline[i + 1]);
        const Vec2 candidate = PointOnSegment(polyline[i], polyline[i + 1], t);
        const float d = Distance(point, candidate);
        if (best < 0.0f || d < best) {
            best = d;
            nearest = candidate;
        }
    }
    return best < 0.0f ? 0.0f : best;
}

// ------------------------------------------------------------ simplification --

std::vector<Vec2> SimplifyPolyline(const std::vector<Vec2>& points,
                                   float tolerance) {
    if (points.size() < 3) {
        return points;
    }
    const float tol = std::max(0.0f, tolerance);
    std::vector<bool> keep(points.size(), false);
    keep.front() = true;
    keep.back() = true;
    SimplifyRange(points, 0, points.size() - 1, tol, keep);

    std::vector<Vec2> out;
    out.reserve(points.size());
    for (size_t i = 0; i < points.size(); ++i) {
        if (keep[i]) {
            out.push_back(points[i]);
        }
    }
    return out;
}

FlatPath DecimateForStroke(const FlatPath& flat, float tolerance) {
    FlatPath out;
    if (flat.polylines.empty()) {
        return out;
    }
    const float tol = std::max(0.0f, tolerance);
    out.polylines.reserve(flat.polylines.size());
    out.closed.reserve(flat.polylines.size());
    for (size_t i = 0; i < flat.polylines.size(); ++i) {
        out.polylines.push_back(SimplifyPolyline(flat.polylines[i], tol));
        out.closed.push_back(i < flat.closed.size() ? flat.closed[i] : false);
    }
    return out;
}

AnimPath OutlineToAnimPath(const FlatPath& flat) {
    AnimPath path;
    for (size_t i = 0; i < flat.polylines.size(); ++i) {
        const std::vector<Vec2>& poly = flat.polylines[i];
        const bool closed = i < flat.closed.size() ? flat.closed[i] : false;
        // A closed loop needs at least 3 points to enclose an area; an open
        // subpath needs at least 2 to draw. Anything smaller is degenerate
        // input (a dot is the brush's filled-ellipse path, not this).
        if (poly.size() < 2 || (closed && poly.size() < 3)) {
            continue;
        }
        AnimSegment move(AnimSegment::Kind::Move);
        move.p[0] = poly[0];
        path.segments.push_back(move);
        for (size_t k = 1; k < poly.size(); ++k) {
            AnimSegment line(AnimSegment::Kind::Line);
            line.p[0] = poly[k];
            path.segments.push_back(line);
        }
        if (closed) {
            path.segments.push_back(AnimSegment(AnimSegment::Kind::Close));
        }
    }
    return path;
}

// ------------------------------------------------------- stroke -> outline --

namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kTwoPi = 6.283185307179586476925;

// Wraps an angle into (-pi, pi].
double WrapPi(double a) {
    if (a <= -kPi) {
        a += std::floor((-a + kPi) / kTwoPi + 1.0) * kTwoPi;
    } else if (a > kPi) {
        a -= std::floor((a + kPi) / kTwoPi) * kTwoPi;
    }
    return a;
}

// One straight piece of a flattened polyline: unit direction and left normal
// (y-down screen space, so the left normal is (-dy, dx)).
struct Offset {
    Vec2 dir;
    Vec2 normal;
    Vec2 p0;
    Vec2 p1;
};

Offset MakeOffset(const Vec2& a, const Vec2& b) {
    Offset o;
    o.p0 = a;
    o.p1 = b;
    const float dx = b.x - a.x;
    const float dy = b.y - a.y;
    const float len = std::sqrt(dx * dx + dy * dy);
    if (len < 1e-6f) {
        o.dir = Vec2(1.0f, 0.0f);
    } else {
        o.dir = Vec2(dx / len, dy / len);
    }
    o.normal = Vec2(-o.dir.y, o.dir.x);
    return o;
}

// Appends a circular arc of radius `radius` around (cx,cy) from angle a0 to a1.
// `sweepSign` (0 = choose the short way, +1/-1 = force that direction) exists
// because the short way is AMBIGUOUS near a 180-degree turn, which dense brush
// input hits constantly. Picking wrong there swings the arc back through the
// band and punches a visible gap; the caller knows the turn's sign, so it says.
//
// Steps come from `tol`: the chord sagitta of a step must stay within it, so a
// tiny stroke is not tessellated needlessly and a large one stays smooth.
void AppendArc(std::vector<Vec2>& out, double cx, double cy, double radius,
               double a0, double a1, float tol, int sweepSign = 0) {
    double sweep = WrapPi(a1 - a0);
    if (sweepSign > 0 && sweep < 0.0) {
        sweep += kTwoPi;
    } else if (sweepSign < 0 && sweep > 0.0) {
        sweep -= kTwoPi;
    }
    if (std::fabs(sweep) < 1e-9 || radius <= 0.0) {
        return;
    }
    // Largest angular step whose chord deviates by at most tol:
    // r * (1 - cos(step/2)) <= tol.
    const float safeTol = std::max(0.0f, tol);
    double maxStep = kPi;
    if (safeTol > 0.0f && radius > static_cast<double>(safeTol)) {
        const double cosHalf = 1.0 - static_cast<double>(safeTol) / radius;
        maxStep = 2.0 * std::acos(std::clamp(cosHalf, -1.0, 1.0));
    } else if (safeTol <= 0.0f) {
        maxStep = kPi;
    } else {
        maxStep = 2.0 * std::acos(std::clamp(1.0 - static_cast<double>(safeTol) / radius,
                                            -1.0, 1.0));
    }
    maxStep = std::clamp(maxStep, 1e-3, kPi);
    int steps = std::max(
        1, static_cast<int>(std::ceil(std::fabs(sweep) / maxStep)));
    // Floor the step count at one segment per 60 degrees of sweep. Without this
    // a tolerance as large as the radius satisfies the sagitta bound with a
    // single step, and a round cap degenerates into a bare point - measured as
    // a round-capped band losing its overhang entirely. The floor costs at most
    // a couple of points per arc and keeps caps looking like caps.
    const int minSteps = std::max(
        1, static_cast<int>(std::ceil(std::fabs(sweep) / (kPi / 3.0))));
    steps = std::max(steps, minSteps);
    for (int i = 1; i <= steps; ++i) {
        const double a = a0 + sweep * (static_cast<double>(i) /
                                       static_cast<double>(steps));
        out.push_back(Vec2(static_cast<float>(cx + std::cos(a) * radius),
                           static_cast<float>(cy + std::sin(a) * radius)));
    }
}

// Emits the corner at the join between `prev` and `next` on ONE side of the
// band. `side` is +1 for the (-dy, dx) offset, -1 for the other.
//
// Only the CONVEX side gets a round arc. Adding arcs to both sides (the naive
// version) bulges the concave side too, and those bulges are what produce the
// visible artifacts - measured up to 2400 wrong pixels per frame.
void AppendJoin(std::vector<Vec2>& out, const Offset& prev, const Offset& next,
                const Vec2& corner, float radius, float side, LineJoin join,
                float miterLimit, float tol) {
    const double cross = static_cast<double>(prev.dir.x) * next.dir.y -
                         static_cast<double>(prev.dir.y) * next.dir.x;
    const double prevAngle =
        std::atan2(static_cast<double>(prev.normal.y) * side,
                   static_cast<double>(prev.normal.x) * side);
    const double nextAngle =
        std::atan2(static_cast<double>(next.normal.y) * side,
                   static_cast<double>(next.normal.x) * side);

    // Straight through (or a full reversal, which is degenerate): no corner.
    if (std::fabs(cross) < 1e-9) {
        return;
    }
    // Which side bulges outward. `normal` is (-dy, dx); with y pointing down, a
    // positive cross (a right turn on screen) has its outside on the +normal
    // side.
    const bool convexOnThisSide = (cross > 0.0) == (side > 0.0f);
    if (!convexOnThisSide) {
        // Concave side: the two offset points already cross over and the
        // winding fill rule absorbs the overlap, so just bridge them.
        out.push_back(Vec2(static_cast<float>(corner.x +
                                              static_cast<double>(next.normal.x) *
                                                  side * radius),
                           static_cast<float>(corner.y +
                                              static_cast<double>(next.normal.y) *
                                                  side * radius)));
        return;
    }

    switch (join) {
        case LineJoin::Round: {
            // Clamp the arc so it cannot overshoot the adjacent vertices.
            //
            // This is the whole ballgame for dense input. A round join sweeps an
            // arc of length radius * sweep, and its extent ALONG each adjacent
            // segment is radius * tan(sweep/2). When the input is sampled denser
            // than the stroke is wide - which is exactly what a brush does at
            // high zoom, since it samples every ~2 screen px - that extent runs
            // past the next vertex. The outline then travels forward beyond a
            // vertex and folds back to it, and under the winding rule the fold
            // cancels to zero and opens a HOLE in the middle of the stroke.
            // Measured: 7.5% of band probes outside the outline on zoomed-in
            // brush input, appearing as cuts in the painted stroke.
            //
            // Clamping is free visually: the truncated arc only ever occurs on
            // micro-vertices whose neighbours are closer than the stroke is
            // wide, where a full arc would have been buried in the band anyway.
            const float prevLen = std::sqrt(DistanceSq(prev.p0, prev.p1));
            const float nextLen = std::sqrt(DistanceSq(next.p0, next.p1));
            const float reach = std::min(prevLen, nextLen);
            double a0 = prevAngle;
            double a1 = nextAngle;
            if (reach < radius * 4.0f && radius > 0.0f) {
                const double maxSweep =
                    2.0 * std::atan(static_cast<double>(reach) / radius);
                double sweep = WrapPi(a1 - a0);
                if (std::fabs(sweep) > maxSweep) {
                    sweep = (sweep > 0.0 ? 1.0 : -1.0) * maxSweep;
                    a1 = a0 + sweep;
                }
            }
            // Force the arc to sweep the way the path actually turns, so a
            // near-reversal cannot flip it back through the band.
            AppendArc(out, corner.x, corner.y, radius, a0, a1, tol,
                      cross > 0.0 ? 1 : -1);
            break;
        }
        case LineJoin::Bevel:
            out.push_back(Vec2(static_cast<float>(corner.x +
                                                  static_cast<double>(next.normal.x) *
                                                      side * radius),
                               static_cast<float>(corner.y +
                                                  static_cast<double>(next.normal.y) *
                                                      side * radius)));
            break;
        case LineJoin::Miter: {
            // Intersection of the two offset lines through the corner.
            const double d0x = prev.dir.x, d0y = prev.dir.y;
            const double d1x = next.dir.x, d1y = next.dir.y;
            const double denom = d0x * d1y - d0y * d1x;
            if (std::fabs(denom) < 1e-9) {
                out.push_back(Vec2(static_cast<float>(corner.x +
                                                      static_cast<double>(next.normal.x) *
                                                          side * radius),
                                   static_cast<float>(corner.y +
                                                      static_cast<double>(next.normal.y) *
                                                          side * radius)));
                break;
            }
            const double rx = static_cast<double>(prev.normal.x) * side * radius;
            const double ry = static_cast<double>(prev.normal.y) * side * radius;
            const double sx = static_cast<double>(next.normal.x) * side * radius;
            const double sy = static_cast<double>(next.normal.y) * side * radius;
            // corner + t*d0 == corner + u*d1  =>  t*d0 - u*d1 = (s - r)
            const double ex = sx - rx;
            const double ey = sy - ry;
            const double t = (ex * d1y - ey * d1x) / denom;
            // The intersection of the two OFFSET lines, so the offset itself
            // (rx, ry) has to be added back - without it the miter vertex lands
            // on the centerline's offset by one radius in the wrong direction.
            const double mx = corner.x + t * d0x + rx;
            const double my = corner.y + t * d0y + ry;
            const double miterLen = std::sqrt((mx - corner.x) * (mx - corner.x) +
                                              (my - corner.y) * (my - corner.y));
            if (miterLen > miterLimit * radius) {
                out.push_back(Vec2(static_cast<float>(corner.x + sx),
                                   static_cast<float>(corner.y + sy)));
            } else {
                out.push_back(Vec2(static_cast<float>(mx), static_cast<float>(my)));
            }
            break;
        }
    }
}

// The cap at one end of a subpath, bridging the left offset to the right.
// `atEnd` selects the far end. Round gets a half-disc bulging outward, butt a
// flat edge, square the flat edge pushed out by the radius.
void AppendCap(std::vector<Vec2>& out, const Offset& seg, bool atEnd, LineCap cap,
               float radius, float tol) {
    const Vec2 p = atEnd ? seg.p1 : seg.p0;
    // Outward direction: the cap bulges away from the subpath.
    const double dirX = atEnd ? seg.dir.x : -seg.dir.x;
    const double dirY = atEnd ? seg.dir.y : -seg.dir.y;
    const double nx = -dirY;
    const double ny = dirX;
    const double lx = p.x + nx * radius;
    const double ly = p.y + ny * radius;
    const double rx = p.x - nx * radius;
    const double ry = p.y - ny * radius;
    const double outAng = std::atan2(dirY, dirX);

    if (cap == LineCap::Round) {
        // The left offset sits a quarter turn from `outAng`, and the right
        // offset half a turn from it, so the half-disc is always a sweep of
        // exactly pi through `outAng` - never the long way round the back.
        const double from = outAng + (atEnd ? kPi * 0.5 : -kPi * 0.5);
        const double sweep = atEnd ? -kPi : kPi;
        AppendArc(out, p.x, p.y, radius, from, from + sweep, tol);
        return;
    }
    if (cap == LineCap::Square) {
        out.push_back(Vec2(static_cast<float>(lx + dirX * radius),
                           static_cast<float>(ly + dirY * radius)));
        out.push_back(Vec2(static_cast<float>(rx + dirX * radius),
                           static_cast<float>(ry + dirY * radius)));
        return;
    }
    out.push_back(Vec2(static_cast<float>(rx), static_cast<float>(ry)));
}

} // namespace

FlatPath StrokeToOutline(const FlatPath& flat, const StrokeOutlineOptions& opts) {
    FlatPath out;
    const float width = std::max(0.0f, opts.width);
    if (flat.polylines.empty() || width <= 0.0f) {
        return out;
    }
    const float radius = width * 0.5f;
    const float tol = std::max(0.0f, opts.tolerance);

    out.polylines.reserve(flat.polylines.size());
    out.closed.reserve(flat.polylines.size());

    for (size_t pi = 0; pi < flat.polylines.size(); ++pi) {
        const std::vector<Vec2>& src = flat.polylines[pi];
        const bool closedInput =
            pi < flat.closed.size() ? flat.closed[pi] : false;

        // Collapse repeated points, and drop zero-length segments: they have no
        // direction, and a zero normal would poison every offset derived from it.
        std::vector<Vec2> pts;
        pts.reserve(src.size());
        for (const Vec2& p : src) {
            if (!pts.empty() && std::fabs(p.x - pts.back().x) < 1e-6f &&
                std::fabs(p.y - pts.back().y) < 1e-6f) {
                continue;
            }
            pts.push_back(p);
        }
        const size_t n = pts.size();
        const size_t segCount = closedInput && n > 2 ? n : (n > 1 ? n - 1 : 0);
        if (segCount == 0) {
            out.polylines.push_back({});
            out.closed.push_back(true);
            continue;
        }

        std::vector<Offset> segs;
        segs.reserve(segCount);
        for (size_t i = 0; i < segCount; ++i) {
            const Vec2& a = pts[i];
            const Vec2& b = pts[(i + 1) % n];
            if (DistanceSq(a, b) < 1e-12f) {
                continue;
            }
            segs.push_back(MakeOffset(a, b));
        }
        if (segs.empty()) {
            out.polylines.push_back({});
            out.closed.push_back(true);
            continue;
        }
        const size_t m = segs.size();

        std::vector<Vec2> outline;
        outline.reserve(m * 4 + 16);

        auto offsetPoint = [](const Offset& s, bool start, float r,
                              int side) -> Vec2 {
            const Vec2 p = start ? s.p0 : s.p1;
            const float sgn = static_cast<float>(side);
            return Vec2(p.x + s.normal.x * r * sgn, p.y + s.normal.y * r * sgn);
        };

        // Left side, forward.
        const Vec2 start = offsetPoint(segs[0], true, radius, +1);
        outline.push_back(start);
        for (size_t i = 0; i + 1 < m; ++i) {
            outline.push_back(offsetPoint(segs[i], false, radius, +1));
            AppendJoin(outline, segs[i], segs[i + 1], segs[i].p1, radius, +1,
                       opts.join, opts.miterLimit, tol);
        }
        outline.push_back(offsetPoint(segs[m - 1], false, radius, +1));
        if (closedInput) {
            AppendJoin(outline, segs[m - 1], segs[0], segs[m - 1].p1, radius, +1,
                       opts.join, opts.miterLimit, tol);
        } else {
            AppendCap(outline, segs[m - 1], true, opts.cap, radius, tol);
        }

        // Right side, backward. Walks segments m-1 down to 1, emitting each
        // segment's start on the right offset plus the corner joining it to the
        // segment before. (An earlier version iterated m-2 down to 0, which both
        // skipped the last segment and indexed segs[-1] - reading outside the
        // vector, feeding NaNs into the arc step count.)
        for (size_t s = m; s-- > 1;) {
            outline.push_back(offsetPoint(segs[s], true, radius, -1));
            AppendJoin(outline, segs[s], segs[s - 1], segs[s].p0, radius, -1,
                       opts.join, opts.miterLimit, tol);
        }
        outline.push_back(offsetPoint(segs[0], true, radius, -1));
        if (closedInput) {
            AppendJoin(outline, segs[0], segs[m - 1], segs[0].p0, radius, -1,
                       opts.join, opts.miterLimit, tol);
        } else {
            AppendCap(outline, segs[0], false, opts.cap, radius, tol);
        }

        out.polylines.push_back(std::move(outline));
        out.closed.push_back(true);
    }
    return out;
}

// ------------------------------------------------------ stroke -> pieces --

// A full disc around (cx,cy), emitted as its own closed piece. Angles run
// DECREASING to match the quad winding below (all pieces must share one
// winding, or their overlaps cancel under the fill rule - the very bug this
// exists to fix). Step count from the same sagitta bound as AppendArc, with a
// floor of 6 so a hairline joint still covers its vertex.
void AppendDisc(FlatPath& out, double cx, double cy, double radius, float tol) {
    if (radius <= 0.0) {
        return;
    }
    const float safeTol = std::max(0.0f, tol);
    double maxStep = kPi;
    if (safeTol > 0.0f && radius > static_cast<double>(safeTol)) {
        maxStep = 2.0 * std::acos(std::clamp(
                              1.0 - static_cast<double>(safeTol) / radius, -1.0,
                              1.0));
    }
    maxStep = std::clamp(maxStep, 1e-3, kPi);
    int n = std::max(
        6, static_cast<int>(std::ceil(kTwoPi / maxStep)));
    std::vector<Vec2> disc;
    disc.reserve(static_cast<size_t>(n));
    for (int i = 0; i < n; ++i) {
        const double a =
            -kTwoPi * (static_cast<double>(i) / static_cast<double>(n));
        disc.push_back(Vec2(static_cast<float>(cx + std::cos(a) * radius),
                            static_cast<float>(cy + std::sin(a) * radius)));
    }
    out.polylines.push_back(std::move(disc));
    out.closed.push_back(true);
}

// One segment's band as a convex quad, same construction for every segment so
// the winding is identical by construction.
void AppendPieceQuad(FlatPath& out, const Offset& s, float radius) {
    out.polylines.push_back(
        {Vec2(s.p0.x + s.normal.x * radius, s.p0.y + s.normal.y * radius),
         Vec2(s.p1.x + s.normal.x * radius, s.p1.y + s.normal.y * radius),
         Vec2(s.p1.x - s.normal.x * radius, s.p1.y - s.normal.y * radius),
         Vec2(s.p0.x - s.normal.x * radius, s.p0.y - s.normal.y * radius)});
    out.closed.push_back(true);
}

Vec2 OffsetEnd(const Offset& s, bool start, float radius, float side) {
    const Vec2 p = start ? s.p0 : s.p1;
    return Vec2(p.x + s.normal.x * radius * side,
                p.y + s.normal.y * radius * side);
}

// Miter vertex on ONE side of the joint, or the bevel point when the miter
// would exceed the limit (or the offset lines are parallel).
Vec2 PieceMiter(const Offset& prev, const Offset& next, const Vec2& corner,
                float radius, float side, float miterLimit) {
    const double d0x = prev.dir.x, d0y = prev.dir.y;
    const double d1x = next.dir.x, d1y = next.dir.y;
    const double denom = d0x * d1y - d0y * d1x;
    const Vec2 fallBack = OffsetEnd(next, true, radius, side);
    if (std::fabs(denom) < 1e-9) {
        return fallBack;
    }
    const double rx = static_cast<double>(prev.normal.x) * side * radius;
    const double ry = static_cast<double>(prev.normal.y) * side * radius;
    const double sx = static_cast<double>(next.normal.x) * side * radius;
    const double sy = static_cast<double>(next.normal.y) * side * radius;
    const double ex = sx - rx;
    const double ey = sy - ry;
    const double t = (ex * d1y - ey * d1x) / denom;
    const double mx = corner.x + t * d0x + rx;
    const double my = corner.y + t * d0y + ry;
    const double miterLen =
        std::sqrt((mx - corner.x) * (mx - corner.x) +
                  (my - corner.y) * (my - corner.y));
    if (miterLen > miterLimit * radius) {
        return fallBack;
    }
    return Vec2(static_cast<float>(mx), static_cast<float>(my));
}

// Joint pieces at the vertex `corner` between `prev` and `next`. Round gets a
// disc (covers the turn on both sides - overdraw on the concave side is the
// same color, so invisible). Bevel gets a flat triangle per side. Miter gets
// the true miter triangle on the convex side (degrading to a bevel past the
// limit) and a plain bevel triangle on the concave side, which sits inside the
// quad overlap anyway.
void AppendPieceJoin(FlatPath& out, const Offset& prev, const Offset& next,
                     const Vec2& corner, float radius, LineJoin join,
                     float miterLimit, float tol) {
    const double cross = static_cast<double>(prev.dir.x) * next.dir.y -
                         static_cast<double>(prev.dir.y) * next.dir.x;
    // Straight through (or a full reversal): the quads abut or overlap
    // exactly, so there is no wedge to cover.
    if (std::fabs(cross) < 1e-9) {
        return;
    }
    if (join == LineJoin::Round) {
        AppendDisc(out, corner.x, corner.y, radius, tol);
        return;
    }
    // +normal side is convex on a positive (right, y-down) turn.
    const float convexSide = cross > 0.0 ? 1.0f : -1.0f;
    // Emits a triangle wound to match the quads (negative shoelace, Winding
    // +1). A triangle's orientation flips with the turn direction, so this is
    // decided per triangle rather than by a fixed vertex order - one
    // inconsistent triangle is enough to punch a hole under the fill rule.
    auto emitTri = [&](const Vec2& t0, const Vec2& t1, const Vec2& t2) {
        const float s = (t1.x - t0.x) * (t2.y - t0.y) -
                        (t2.x - t0.x) * (t1.y - t0.y);
        if (s < 0.0f) {
            out.polylines.push_back({t0, t1, t2});
        } else {
            out.polylines.push_back({t0, t2, t1});
        }
        out.closed.push_back(true);
    };
    auto bevelTri = [&](float side) {
        emitTri(corner, OffsetEnd(prev, false, radius, side),
                OffsetEnd(next, true, radius, side));
    };
    if (join == LineJoin::Bevel) {
        bevelTri(+1.0f);
        bevelTri(-1.0f);
        return;
    }
    // Miter.
    bevelTri(-convexSide);
    const Vec2 tip =
        PieceMiter(prev, next, corner, radius, convexSide, miterLimit);
    emitTri(tip, OffsetEnd(prev, false, radius, convexSide),
            OffsetEnd(next, true, radius, convexSide));
}

FlatPath StrokeToPieces(const FlatPath& flat, const StrokeOutlineOptions& opts) {
    FlatPath out;
    const float width = std::max(0.0f, opts.width);
    if (flat.polylines.empty() || width <= 0.0f) {
        return out;
    }
    const float radius = width * 0.5f;
    const float tol = std::max(0.0f, opts.tolerance);

    for (size_t pi = 0; pi < flat.polylines.size(); ++pi) {
        const std::vector<Vec2>& src = flat.polylines[pi];
        const bool closedInput = pi < flat.closed.size() ? flat.closed[pi] : false;

        // Same collapsing as the outline: repeated points have no direction.
        std::vector<Vec2> pts;
        pts.reserve(src.size());
        for (const Vec2& p : src) {
            if (!pts.empty() && std::fabs(p.x - pts.back().x) < 1e-6f &&
                std::fabs(p.y - pts.back().y) < 1e-6f) {
                continue;
            }
            pts.push_back(p);
        }
        const size_t n = pts.size();
        const size_t segCount = closedInput && n > 2 ? n : (n > 1 ? n - 1 : 0);
        if (segCount == 0) {
            continue;
        }
        std::vector<Offset> segs;
        segs.reserve(segCount);
        for (size_t i = 0; i < segCount; ++i) {
            const Vec2& a = pts[i];
            const Vec2& b = pts[(i + 1) % n];
            if (DistanceSq(a, b) < 1e-12f) {
                continue;
            }
            segs.push_back(MakeOffset(a, b));
        }
        if (segs.empty()) {
            continue;
        }
        const size_t m = segs.size();
        out.polylines.reserve(out.polylines.size() + m * 2 + 2);

        for (const Offset& s : segs) {
            AppendPieceQuad(out, s, radius);
        }
        if (closedInput) {
            for (size_t j = 0; j < m; ++j) {
                AppendPieceJoin(out, segs[(j + m - 1) % m], segs[j],
                                segs[j].p0, radius, opts.join, opts.miterLimit,
                                tol);
            }
            continue;
        }
        // Caps. Round gets a disc; square an extended rect (built as a quad
        // over a synthetic segment so its winding matches automatically);
        // butt needs nothing - the end quad is already flush.
        auto capAt = [&](const Vec2& p, const Vec2& outward) {
            if (opts.cap == LineCap::Round) {
                AppendDisc(out, p.x, p.y, radius, tol);
            } else if (opts.cap == LineCap::Square) {
                const Vec2 tip(p.x + outward.x * radius,
                               p.y + outward.y * radius);
                AppendPieceQuad(out, MakeOffset(p, tip), radius);
            }
        };
        capAt(segs[0].p0, Vec2(-segs[0].dir.x, -segs[0].dir.y));
        capAt(segs[m - 1].p1, segs[m - 1].dir);
        for (size_t j = 1; j < m; ++j) {
            AppendPieceJoin(out, segs[j - 1], segs[j], segs[j].p0, radius,
                            opts.join, opts.miterLimit, tol);
        }
    }
    return out;
}

// ------------------------------------------------------------- bezier fit --

namespace {

// Evaluates a cubic Bezier at t with double precision (fitting stability).
Vec2 CubicAt(const Vec2& p0, const Vec2& c1, const Vec2& c2, const Vec2& p1,
             double t) {
    const double u = 1.0 - t;
    const double w0 = u * u * u;
    const double w1 = 3.0 * u * u * t;
    const double w2 = 3.0 * u * t * t;
    const double w3 = t * t * t;
    return Vec2(static_cast<float>(p0.x * w0 + c1.x * w1 + c2.x * w2 + p1.x * w3),
                static_cast<float>(p0.y * w0 + c1.y * w1 + c2.y * w2 + p1.y * w3));
}

// Chord-length parameters for points[first..last]. Degenerate runs (all points
// coincident) yield uniform spacing so the solver never divides by zero.
void ChordParams(const std::vector<Vec2>& points, size_t first, size_t last,
                 std::vector<double>& out) {
    out.clear();
    out.reserve(last - first + 1);
    out.push_back(0.0);
    double total = 0.0;
    for (size_t i = first + 1; i <= last; ++i) {
        total += Distance(points[i - 1], points[i]);
        out.push_back(total);
    }
    if (total < 1e-9) {
        for (size_t i = 0; i < out.size(); ++i) {
            out[i] = static_cast<double>(i) / (out.size() - 1);
        }
        return;
    }
    for (double& u : out) {
        u /= total;
    }
}

Vec2 NormalizedOr(const Vec2& v, const Vec2& fallback) {
    const float len = Length(v);
    return len > 1e-6f ? v * (1.0f / len) : fallback;
}

// Least-squares cubic fit through points[first..last] (Schneider's FitCubic):
// endpoints fixed, control distances along the end tangents solved 2x2.
// Returns the max deviation and its index. Controls are meaningless when the
// run is degenerate (caller checks the point count first).
double FitCubicRun(const std::vector<Vec2>& points, size_t first, size_t last,
                   Vec2& c1Out, Vec2& c2Out, size_t& splitOut) {
    const Vec2& p0 = points[first];
    const Vec2& p3 = points[last];
    Vec2 t0 = NormalizedOr(points[first + 1] - p0,
                           NormalizedOr(p3 - p0, Vec2(1.0f, 0.0f)));
    Vec2 t1 = NormalizedOr(points[last - 1] - p3,
                           NormalizedOr(p0 - p3, Vec2(-1.0f, 0.0f)));

    std::vector<double> u;
    ChordParams(points, first, last, u);

    // Normal equations for the two control distances. Q(u) = P0*B0 + C1*B1 +
    // C2*B2 + P3*B3 with C1 = P0 + alpha*t0 and C2 = P3 + beta*t1, so the
    // residual against Pi is Pi - P0*(b0+b1) - P3*(b2+b3); projecting it onto
    // the two tangent directions gives the 2x2 system. (Using only b0/b3 here
    // is a classic transcription slip - it biases every fit and the error
    // check then rejects all cubics, degrading everything to lines.)
    double c00 = 0.0, c01 = 0.0, c11 = 0.0, x0 = 0.0, x1 = 0.0;
    for (size_t k = 1; k + 1 < u.size(); ++k) {
        const double t = u[k];
        const double b0 = (1 - t) * (1 - t) * (1 - t);
        const double b1 = 3 * t * (1 - t) * (1 - t);
        const double b2 = 3 * t * t * (1 - t);
        const double b3 = t * t * t;
        const double a1x = t0.x * b1, a1y = t0.y * b1;
        const double a2x = t1.x * b2, a2y = t1.y * b2;
        c00 += a1x * a1x + a1y * a1y;
        c01 += a1x * a2x + a1y * a2y;
        c11 += a2x * a2x + a2y * a2y;
        const double q0x = p0.x * (b0 + b1) + p3.x * (b2 + b3);
        const double q0y = p0.y * (b0 + b1) + p3.y * (b2 + b3);
        const double px = points[first + k].x - q0x;
        const double py = points[first + k].y - q0y;
        x0 += px * a1x + py * a1y;
        x1 += px * a2x + py * a2y;
    }

    double alpha = 0.0, beta = 0.0;
    const double det = c00 * c11 - c01 * c01;
    if (std::fabs(det) > 1e-9) {
        alpha = (x0 * c11 - x1 * c01) / det;
        beta = (c00 * x1 - c01 * x0) / det;
    }
    // Negative or degenerate distances mean the tangents fight the data (a cusp
    // or a near-straight run): fall back to the 1/3 heuristic instead of
    // emitting a looped or collapsed cubic.
    const double segLen = Distance(p0, p3);
    if (!(alpha > 1e-6) || !(beta > 1e-6)) {
        const double fallback = segLen / 3.0;
        c1Out = p0 + t0 * static_cast<float>(fallback);
        c2Out = p3 + t1 * static_cast<float>(fallback);
    } else {
        c1Out = p0 + t0 * static_cast<float>(alpha);
        c2Out = p3 + t1 * static_cast<float>(beta);
    }

    double worst = 0.0;
    splitOut = first + 1;
    for (size_t k = 1; k + 1 < u.size(); ++k) {
        const Vec2 onCurve = CubicAt(p0, c1Out, c2Out, p3, u[k]);
        const double d = Distance(points[first + k], onCurve);
        if (d > worst) {
            worst = d;
            splitOut = first + k;
        }
    }
    return worst;
}

// Max distance of points[first..last] from the straight chord. Used to emit a
// single Line for straight runs instead of a wasteful cubic.
double ChordDeviation(const std::vector<Vec2>& points, size_t first,
                      size_t last) {
    const Vec2& a = points[first];
    const Vec2& b = points[last];
    double worst = 0.0;
    for (size_t i = first + 1; i < last; ++i) {
        worst = std::max(worst, static_cast<double>(
                                     PointSegmentDistanceSq(points[i], a, b)));
    }
    return std::sqrt(worst);
}

// Greedy error-bounded fit of points[first..last]: a straight run becomes one
// Line, otherwise the longest cubic within tolerance wins, splitting at the
// worst point when nothing fits (Schneider's FitCurve strategy). Caps the
// window so a pathological run cannot turn quadratic.
void FitRange(const std::vector<Vec2>& points, size_t first, size_t last,
              float tolerance, std::vector<AnimSegment>& out) {
    static constexpr size_t kMaxWindow = 128;
    if (last <= first) {
        return;
    }
    if (last == first + 1) {
        AnimSegment line(AnimSegment::Kind::Line);
        line.p[0] = points[last];
        out.push_back(line);
        return;
    }
    // Straight runs stay lines: exact, compact, and axis-aligned crisp.
    if (ChordDeviation(points, first, std::min(last, first + kMaxWindow)) <=
        tolerance) {
        size_t end = first + 1;
        while (end < last && end - first < kMaxWindow &&
               ChordDeviation(points, first, end + 1) <= tolerance) {
            ++end;
        }
        AnimSegment line(AnimSegment::Kind::Line);
        line.p[0] = points[end];
        out.push_back(line);
        FitRange(points, end, last, tolerance, out);
        return;
    }
    // Longest cubic within tolerance.
    size_t best = first + 1;
    Vec2 bestC1, bestC2;
    size_t end = std::min(last, first + kMaxWindow);
    size_t split = first + 1;
    for (size_t j = first + 2; j <= end; ++j) {
        Vec2 c1, c2;
        size_t candidate = first + 1;
        const double err = FitCubicRun(points, first, j, c1, c2, candidate);
        if (err <= tolerance) {
            best = j;
            bestC1 = c1;
            bestC2 = c2;
        } else {
            split = candidate;
            break;
        }
        if (j == end) {
            split = candidate;
        }
    }
    if (best == first + 1) {
        // Even a 2-point cubic is over tolerance (a kink): emit a line to the
        // split point and continue from there so progress is guaranteed.
        AnimSegment line(AnimSegment::Kind::Line);
        line.p[0] = points[split > first ? split : first + 1];
        out.push_back(line);
        FitRange(points, split > first ? split : first + 1, last, tolerance,
                 out);
        return;
    }
    AnimSegment cubic(AnimSegment::Kind::Cubic);
    cubic.p[0] = bestC1;
    cubic.p[1] = bestC2;
    cubic.p[2] = points[best];
    out.push_back(cubic);
    FitRange(points, best, last, tolerance, out);
}

} // namespace

void FitBeziersToPolyline(const std::vector<Vec2>& points, float tolerance,
                          Vec2& startOut,
                          std::vector<AnimSegment>& segmentsOut) {
    if (points.empty()) {
        return;
    }
    startOut = points.front();
    if (points.size() == 1) {
        return;
    }
    // `tolerance` is the max deviation in path units: fitted curves stay within
    // it of the input polyline. Corners sharper than tolerance split naturally
    // (the error check fails across them), rounding them by at most tolerance.
    FitRange(points, 0, points.size() - 1, std::max(0.05f, tolerance),
             segmentsOut);
}

} // namespace anim
} // namespace icg