#include "anim_geometry.h"

#include <algorithm>

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

// ------------------------------------------------------------- bezier fit --

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

    // Walk the polyline and emit one segment per interior point: a line when
    // the point is collinear with its neighbours (cheap, and keeps axis-
    // aligned strokes crisp), a cubic otherwise.
    const size_t n = points.size();
    for (size_t i = 1; i < n; ++i) {
        const Vec2& prev = points[i - 1];
        const Vec2& cur = points[i];
        const Vec2& next = (i + 1 < n) ? points[i + 1] : cur;
        const Vec2 inDir = cur - prev;
        const Vec2 outDir = next - cur;

        const float cross = Cross(inDir, outDir);
        const float scale = std::max(Length(inDir), Length(outDir));
        const float collinear = scale > 1e-6f ? std::fabs(cross) / scale : 0.0f;

        if (collinear <= tolerance || i + 1 == n) {
            AnimSegment line(AnimSegment::Kind::Line);
            line.p[0] = cur;
            segmentsOut.push_back(line);
            continue;
        }

        // Control points at 1/3 of each adjacent run, so the curve leaves and
        // arrives along the local direction (a Catmull-Rom-to-Bezier conversion).
        AnimSegment cubic(AnimSegment::Kind::Cubic);
        cubic.p[0] = prev + inDir * (1.0f / 3.0f);
        cubic.p[1] = cur - outDir * (1.0f / 3.0f);
        cubic.p[2] = cur;
        segmentsOut.push_back(cubic);
    }
}

} // namespace anim
} // namespace icg