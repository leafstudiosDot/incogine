// Incogine - 2D vector animation geometry.
// Part of Incogine by leafstudiosDot (MPL-2.0). See LICENSE.
//
// Shared by the rasterizer, the baker, and the editor's hit-testing, so the
// picture the user sees and the pixels that get baked come from one set of
// flattening rules. Header-only, dependency-free.

#pragma once

#include <cmath>
#include <vector>

#include "anim_path.h"
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
// conservative rather than tight - correct for selection outlines, cheap to
// compute.
void PathBounds(const AnimPath& path, Vec2& minOut, Vec2& maxOut);

// Bounds of `flat` in path space, tight to the flattened geometry.
void FlatBounds(const FlatPath& flat, Vec2& minOut, Vec2& maxOut);

// Bounds of `flat` carried into another space by `matrix`, inflated by `pad
//` (in output units) on every side. The pad covers stroke halos and selection
// affordances so culling and marquee tests work on rendered pixels, not
// centerlines. Empty when the path has no geometry (minOut > maxOut).
void FlatTransformedBounds(const FlatPath& flat, const Mat2x3& matrix,
                           float pad, Vec2& minOut, Vec2& maxOut);

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
// for picking near a curved edge - flattened Beziers are straight chords, so
// an exact inside test misses points right on the curve.
bool PointNearFlatPath(const Vec2& point, const FlatPath& flat, float distance);

// Distance from `point` to the polyline's edges. `nearest` receives the closest
// point on the outline. Used for stroke hit-testing.
float DistanceToPolyline(const Vec2& point, const std::vector<Vec2>& polyline,
                         Vec2& nearest);

// Ramer-Douglas-Peucker simplification of a polyline, keeping endpoints.
// General-purpose decimation for dense polylines; the brush fits raw input
// directly (least-squares cubics smooth jitter on their own, and RDP output
// provably fragments the fit), so this stays as a standalone utility.
std::vector<Vec2> SimplifyPolyline(const std::vector<Vec2>& points,
                                   float tolerance);

// MEASURED, AND CURRENTLY UNUSED BY THE CANVAS - kept because the numbers are
// the reason it is not used, and that is worth not rediscovering.
//
// Decimating a flattened polyline before handing it to a raster stroker looks
// like an obvious win and is not one. Measured on a fitted brush stroke
// (12 strokes, 3648 flattened verts, pen width 4):
//
//   tolerance            verts kept   repaint   worst-case pixel error
//   0.004 (0.1% of pen)     92.5%      1.0x          max 90,  22 bad px
//   0.008 (0.2% of pen)     87.5%      1.0x          max 132, 188 bad px
//   0.020 (0.5% of pen)     73.0%      1.0-1.2x     max 333, 1906 bad px
//   0.080 (2% of pen)       34.9%      1.4-1.7x     max 504, 8548 bad px
//
// The tolerances small enough to be visually safe buy NO speedup; the ones
// that buy speedup damage the artwork. There is no useful operating point in
// between, because the cost that decimation removes (per-vertex stroker setup)
// and the thing decimation destroys (sub-pixel centerline detail that the
// stroker then resolves into visible edges) are the same vertices.
//
// The real cost is in Qt's raster stroker itself: ~20us per vertex, and
// scaling with the DEVICE-space width of the stroke band, so it grows as you
// zoom in. That is not fixable by changing the geometry handed to it - see
// docs/incoanim.md for the options (OpenGL viewport, or caching the stage
// raster between edits).
//
// Takes a tolerance in path units. RDP-based, like SimplifyPolyline.
FlatPath DecimateForStroke(const FlatPath& flat, float tolerance);

// --------------------------------------------------------- stroke -> fill --
//
// Turns a stroke into a CLOSED FILLED OUTLINE, so it can be painted with a fill
// instead of a pen.
//
// Why: Qt's raster stroker (QPainterPathStroker) costs ~20us PER VERTEX and
// scales with the DEVICE-space width of the stroke band, so a dense brush
// stroke gets slower as you zoom in - the exact complaint that made the canvas
// unusable. Measured on 12 fitted brush strokes (3648 centerline vertices,
// pen width 4, 1200x800 viewport):
//
//     zoom    Qt stroker    outline + fill    speedup
//       1       39.7 ms        4.8 ms           8.2x
//       3       83.4 ms        8.3 ms          10.1x
//       8      158.8 ms       11.3 ms          14.1x
//      16       45.2 ms        7.0 ms           6.4x
//
// Filling the same geometry with NoPen costs ~0.1 ms, which is why this wins:
// it removes the work rather than moving it to a GPU. It is also the technique
// Flash/Animate use - their brush produces a FILLED shape, not a stroked path.
//
// The outline has ~3x the centerline's vertices and, being a pure function of
// the shape's path/width/cap/join, is worth caching per shape.
struct StrokeOutlineOptions {
    // Full stroke width in path units.
    float width = 1.0f;
    LineCap cap = LineCap::Round;
    LineJoin join = LineJoin::Round;
    // Miter joins longer than miterLimit * halfWidth fall back to a bevel.
    float miterLimit = 4.0f;
    // Max deviation when approximating round joins/caps with line segments, in
    // path units. Also drives the step count, so arcs cost no more than their
    // on-screen size warrants.
    float tolerance = kFlattenTolerance;
};

// Expands `flat` into one closed outline polyline per input polyline, wound so
// that the WINDING fill rule paints a solid band (even-odd must NOT be used: a
// self-overlapping stroke has to stay solid where it crosses itself, and a
// closed centerline becomes a ring whose hole must not fill in).
//
// A single loop still cancels to zero where the stroke crosses ITSELF (a brush
// circle's overlap reads as subtracted/masked) - that is what StrokeToPieces
// below fixes. StrokeToOutline remains the boundary loop used for selection
// highlights, where only the edges are stroked and winding never applies.
//
// Degenerate input is handled rather than producing NaNs: repeated points are
// collapsed, zero-length segments are dropped, and a path too short to have
// direction returns an empty outline.
//
// One deliberate difference from a raster stroker: overlapping parts of the
// stroke fill uniformly instead of compositing darker, which is what an opaque
// color should do. See docs/incoanim.md for measured pixel fidelity.
FlatPath StrokeToOutline(const FlatPath& flat, const StrokeOutlineOptions& opts);

// Expands `flat` into UNION-CORRECT fill pieces: one convex quad per segment,
// a disc at every round joint and round cap, bevel/miter triangles elsewhere.
// Every piece shares the SAME nonzero winding, so a single WindingFill paints
// their exact union - including where the stroke crosses itself, which is where
// the single-loop StrokeToOutline cancels to zero and punches a hole (a brush
// circle's overlap). This is what the canvas paints strokes with.
//
// Same options struct (width/cap/join/miterLimit/tolerance). Caps behave like
// the outline version: round gets a disc, butt nothing, square an extended
// rect. Degenerate input yields no pieces, never NaNs.
FlatPath StrokeToPieces(const FlatPath& flat, const StrokeOutlineOptions& opts);

// Triangulates one simple (non-self-intersecting) polygon by ear clipping,
// appending triangles as consecutive triples. Holes are NOT supported (no
// hole-producing fills exist today). Returns false for degenerate input
// (< 3 distinct points, zero area), appending nothing. Used by the scene
// mesh builder (and the future GPU renderer); the pieces StrokeToPieces
// emits are convex, but general fills (ellipses, rects, future shape tools)
// are triangulated through this same path so concave shapes work too.
bool TriangulatePolygon(const std::vector<Vec2>& poly,
                        std::vector<Vec2>& trisOut);

// Error-bounded piecewise fit of a polyline (Schneider's FitCurve strategy):
// straight runs become one Line, otherwise the longest cubic within `tolerance`
// wins, splitting at the worst point when nothing fits. `tolerance` is the max
// deviation in path units, so fitted curves stay within it of the input.
// Returns the first point in `startOut` and appends segments to `segmentsOut`.
// A 3000-point scribble typically collapses to dozens of segments instead of
// one per point, which is what keeps .incoanim files small.
void FitBeziersToPolyline(const std::vector<Vec2>& points, float tolerance,
                          Vec2& startOut,
                          std::vector<AnimSegment>& segmentsOut);

} // namespace anim
} // namespace icg
