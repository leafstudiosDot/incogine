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