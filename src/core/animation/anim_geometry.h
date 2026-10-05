// Incogine — 2D vector animation geometry.
// Part of Incogine by leafstudiosDot (MPL-2.0). See LICENSE.
//
// Shared by the rasterizer, the baker, and the editor's hit-testing, so the
// picture the user sees and the pixels that get baked come from one set of
// flattening rules. Header-only, dependency-free.

#pragma once

#include <cmath>
#include <vector>

#include "anim_document.h"
#include "anim_types.h"

namespace icg {
namespace anim {

// A flattened path: polylines in path space. `closed[i]` says whether
// polyline i returns to its first point.
struct FlatPath {
    std::vector<std::vector<Vec2>> polylines;
    std::vector<bool> closed;
};

// Default tolerance for a cubic Bezier, chosen for a 1080p stage: roughly
// half a pixel of error on a 1080-unit-tall frame.
inline constexpr float kFlattenTolerance = 0.25f;

// A cubic flattened into `out` (appended, not cleared). Subdivision count is
// derived from the control polygon length so long curves get more segments
// without a fixed per-curve cost.
void FlattenCubic(const Vec2& p0, const Vec2& c1, const Vec2& c2, const Vec2& p1,
                  float tolerance, std::vector<Vec2>& out);

// Flattens `path` into polylines. `tolerance` is in path units.
FlatPath Flatten(const AnimPath& path, float tolerance = kFlattenTolerance);

// Axis-aligned bounds of `path` in path space. `minOut > maxOut` (empty) when
// the path has no geometry. Bezier control points are included, so the box is
// conservative rather than tight — correct for selection outlines, cheap to
// compute.
void PathBounds(const AnimPath& path, Vec2& minOut, Vec2& maxOut);

// Bounds of `flat` in path space, tight to the flattened geometry.
void FlatBounds(const FlatPath& flat, Vec2& minOut, Vec2& maxOut);

// Winding direction of a closed polyline: +1 for clockwise (screen space,
// y-down), -1 for counter-clockwise, 0 for degenerate. Shape Tween needs this
// to match morph pairs.
int Winding(const std::vector<Vec2>& polyline);

// Even-odd crossing test against a closed polyline. `tolerance` inflates the
// polyline so thin strokes stay clickable.
bool PointInPolyline(const Vec2& point, const std::vector<Vec2>& polyline,
                     float tolerance);

// True when `point` is inside any subpath of `flat` (even-odd). Used for fill
// hit-testing in the editor.
bool PointInFlatPath(const Vec2& point, const FlatPath& flat, float tolerance);

// True when `point` is within `distance` of the outline, inside or out. Used
// for picking near a curved edge — flattened Beziers are straight chords, so
// an exact inside test misses points right on the curve.
bool PointNearFlatPath(const Vec2& point, const FlatPath& flat, float distance);

// Distance from `point` to the polyline's edges. `nearest` receives the closest
// point on the outline. Used for stroke hit-testing.
float DistanceToPolyline(const Vec2& point, const std::vector<Vec2>& polyline,
                         Vec2& nearest);

// Ramer-Douglas-Peucker simplification of a polyline, keeping endpoints.
// The brush tool runs this on raw input points before fitting Beziers, so a
// shaky hand produces a clean path.
std::vector<Vec2> SimplifyPolyline(const std::vector<Vec2>& points,
                                   float tolerance);

// Approximates a polyline with cubic Bezier segments. Each run of interior
// points becomes one cubic whose control points are parallel and scaled by
// 1/3 of the run length (Schneider's method, kappa-elbow simplified), which
// matches how the canvas displays it. Returns segments with the first point in
// `startOut` and appends Cubic segments to `segmentsOut`; a Straight run
// produces a Line segment instead.
void FitBeziersToPolyline(const std::vector<Vec2>& points, float tolerance,
                          Vec2& startOut,
                          std::vector<AnimSegment>& segmentsOut);

} // namespace anim
} // namespace icg