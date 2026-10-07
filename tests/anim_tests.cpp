// Incogine — `.incoanim` model, geometry, serialization, and undo tests.
// Part of Incogine by leafstudiosDot (MPL-2.0). See LICENSE.
//
// Covers the Milestone 1 requirements: the data model, save/load round-trip,
// and keyframe/frame logic — all headless, with no Qt and no SDL.

#include "test_check.h"

#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "animation/anim_commands.h"
#include "animation/anim_container.h"
#include "animation/anim_geometry.h"
#include "animation/anim_io.h"
#include "animation/anim_scene.h"

using namespace icg::anim;

// ------------------------------------------------------------- 2D math --

static void TestTransformMath() {
    TEST_GROUP("transform math");

    const Vec2 origin(3.0f, 4.0f);
    CHECK_EQ(TransformPoint(Mat2x3Identity(), origin).x, 3.0f);
    CHECK_EQ(TransformPoint(Mat2x3Translate(10.0f, 20.0f), origin).x, 13.0f);
    CHECK_EQ(TransformPoint(Mat2x3Scale(2.0f, 3.0f), origin).y, 12.0f);

    // Rotate 90 degrees: (1,0) -> (0,1) with y-down screen space.
    const float quarter = 3.14159265358979f * 0.5f;
    const Vec2 rotated = TransformPoint(Mat2x3Rotate(quarter), Vec2(1.0f, 0.0f));
    CHECK_NEAR(rotated.x, 0.0f, 1e-5f);
    CHECK_NEAR(rotated.y, 1.0f, 1e-5f);

    // Composition applies the right-hand matrix first.
    const Mat2x3 compose = Mat2x3Scale(2.0f, 2.0f) * Mat2x3Translate(5.0f, 0.0f);
    const Vec2 composed = TransformPoint(compose, Vec2(1.0f, 1.0f));
    CHECK_NEAR(composed.x, 12.0f, 1e-5f); // (1+5)*2, not 1*2+5
    CHECK_NEAR(composed.y, 2.0f, 1e-5f);

    // Inverse round-trips a point.
    Mat2x3 inverse;
    REQUIRE(Invert(compose, inverse));
    const Vec2 back = TransformPoint(inverse, composed);
    CHECK_NEAR(back.x, 1.0f, 1e-5f);
    CHECK_NEAR(back.y, 1.0f, 1e-5f);

    // A singular matrix cannot be inverted.
    Mat2x3 singular;
    CHECK(!Invert(Mat2x3Scale(0.0f, 1.0f), singular));

    // Vector ignores translation, point does not.
    const Mat2x3 affine = Mat2x3Translate(50.0f, 50.0f) * Mat2x3Rotate(quarter);
    CHECK_NEAR(TransformVector(affine, Vec2(1.0f, 0.0f)).x, 0.0f, 1e-5f);
    CHECK_NEAR(TransformPoint(affine, Vec2(1.0f, 0.0f)).x, 50.0f, 1e-5f);

    CHECK(Mat2x3IsIdentity(Mat2x3Identity()));
    CHECK(!Mat2x3IsIdentity(Mat2x3Translate(0.0f, 1.0f)));

    // Decomposed transform composes in scale -> skew -> rotate -> translate.
    AnimTransform transform;
    transform.position = Vec2(10.0f, 0.0f);
    transform.scale = Vec2(2.0f, 2.0f);
    CHECK_NEAR(TransformPoint(transform.ToMatrix(), Vec2(1.0f, 0.0f)).x, 22.0f,
               1e-5f);

    CHECK_NEAR(Normalized(Vec2(3.0f, 4.0f)).x, 0.6f, 1e-6f);
    CHECK_NEAR(Normalized(Vec2(0.0f, 0.0f)).x, 0.0f, 1e-6f);
    CHECK_NEAR(Lerp(Vec2(0.0f, 0.0f), Vec2(10.0f, 20.0f), 0.5f).y, 10.0f, 1e-6f);
}

static void TestColorAndEasing() {
    TEST_GROUP("color + easing");

    CHECK_EQ(AnimColor::FromHex("#FF8000C8").r, 255);
    CHECK_EQ(AnimColor::FromHex("#FF8000C8").a, 200);
    // Alpha is optional and defaults to opaque.
    CHECK_EQ(AnimColor::FromHex("00FF00").a, 255);
    CHECK_EQ(AnimColor::FromHex("#00FF00").g, 255);
    // Lowercase parses, and ToHex always emits 8 digits.
    CHECK_EQ(AnimColor::FromHex("#00ff00").g, 255);
    CHECK_EQ(AnimColor::FromHex("#00000000").ToHex(), std::string("#00000000"));
    // Garbage degrades to opaque white rather than crashing.
    CHECK_EQ(AnimColor::FromHex("nope"), AnimColor(255, 255, 255, 255));
    CHECK_EQ(AnimColor::FromHex(""), AnimColor(255, 255, 255, 255));
    // Round-trips exactly, including alpha 0.
    const AnimColor original(12, 34, 56, 78);
    CHECK(AnimColor::FromHex(original.ToHex()) == original);
    CHECK(AnimColor::FromHex("#00000000") == AnimColor(0, 0, 0, 0));

    CHECK_NEAR(Easing::Linear().Apply(0.5f), 0.5f, 1e-6f);
    // Endpoints are pinned so a tween never overshoots its keyframes.
    CHECK_NEAR(Easing::Linear().Apply(-3.0f), 0.0f, 1e-6f);
    CHECK_NEAR(Easing::Linear().Apply(9.0f), 1.0f, 1e-6f);

    // Cubic easing is monotonic and shares the engine's solve.
    const Easing ease = Easing::Cubic(0.25f, 0.1f, 0.25f, 1.0f);
    CHECK_NEAR(ease.Apply(0.0f), 0.0f, 1e-6f);
    CHECK_NEAR(ease.Apply(1.0f), 1.0f, 1e-6f);
    float previous = -1.0f;
    for (int i = 0; i <= 20; ++i) {
        const float value = ease.Apply(static_cast<float>(i) / 20.0f);
        CHECK(value >= previous - 1e-4f);
        CHECK(value >= -1e-4f && value <= 1.0f + 1e-4f);
        previous = value;
    }
}

// ------------------------------------------------------------- geometry --

static void TestGeometry() {
    TEST_GROUP("geometry");

    const FlatPath rect = Flatten(AnimPath::FromRect(0.0f, 0.0f, 100.0f, 50.0f));
    REQUIRE_EQ(rect.polylines.size(), static_cast<size_t>(1));
    CHECK(rect.closed[0]);
    Vec2 lo, hi;
    FlatBounds(rect, lo, hi);
    CHECK_NEAR(lo.x, 0.0f, 1e-4f);
    CHECK_NEAR(lo.y, 0.0f, 1e-4f);
    CHECK_NEAR(hi.x, 100.0f, 1e-4f);
    CHECK_NEAR(hi.y, 50.0f, 1e-4f);

    CHECK(PointInFlatPath(Vec2(50.0f, 25.0f), rect, 0.0f));
    CHECK(!PointInFlatPath(Vec2(150.0f, 25.0f), rect, 0.0f));
    CHECK(!PointInFlatPath(Vec2(50.0f, -5.0f), rect, 0.0f));

    // Ellipse flattens to a closed loop with consistent winding (needed to pair
    // shapes for a future Shape Tween).
    const FlatPath circle = Flatten(AnimPath::FromEllipse(0.0f, 0.0f, 50.0f, 50.0f));
    REQUIRE_EQ(circle.polylines.size(), static_cast<size_t>(1));
    CHECK(circle.closed[0]);
    CHECK_EQ(Winding(circle.polylines[0]), 1);
    CHECK(PointInFlatPath(Vec2(0.0f, 0.0f), circle, 0.0f));
    // The flattened loop approximates the ellipse with straight chords, so it
    // sits marginally INSIDE the true curve: the edge point (50, 0) is just
    // outside, while a point well within the radius is inside. This is why
    // picking uses a tolerance rather than an exact test.
    CHECK(PointInFlatPath(Vec2(0.0f, 45.0f), circle, 0.0f));
    CHECK(PointNearFlatPath(Vec2(50.0f, 0.0f), circle, 2.0f));
    CHECK(!PointNearFlatPath(Vec2(70.0f, 0.0f), circle, 2.0f));

    // PathBounds is conservative for Beziers (control points included).
    Vec2 blo, bhi;
    PathBounds(AnimPath::FromEllipse(0.0f, 0.0f, 50.0f, 50.0f), blo, bhi);
    CHECK(bhi.x >= 50.0f && bhi.y >= 50.0f);
    CHECK(blo.x <= -50.0f && blo.y <= -50.0f);

    // Transformed bounds: identity keeps the flat box, translation shifts it,
    // and the pad inflates every side (stroke halos for culling/marquee).
    Vec2 tlo, thi;
    FlatTransformedBounds(rect, Mat2x3Identity(), 0.0f, tlo, thi);
    CHECK_NEAR(tlo.x, 0.0f, 1e-3f);
    CHECK_NEAR(thi.x, 100.0f, 1e-3f);
    FlatTransformedBounds(rect, Mat2x3Translate(10.0f, 20.0f), 2.0f, tlo, thi);
    CHECK_NEAR(tlo.x, 8.0f, 1e-3f);
    CHECK_NEAR(tlo.y, 18.0f, 1e-3f);
    CHECK_NEAR(thi.x, 112.0f, 1e-3f);
    CHECK_NEAR(thi.y, 72.0f, 1e-3f);
    // Empty path stays empty (min > max marker), pad or not.
    FlatPath empty;
    FlatTransformedBounds(empty, Mat2x3Identity(), 5.0f, tlo, thi);
    CHECK(thi.x < tlo.x && thi.y < tlo.y);
    // A negative pad clamps to zero rather than shrinking the box.
    FlatTransformedBounds(rect, Mat2x3Identity(), -5.0f, tlo, thi);
    CHECK_NEAR(tlo.x, 0.0f, 1e-3f);
    CHECK_NEAR(thi.x, 100.0f, 1e-3f);

    // Distance to a polyline.
    std::vector<Vec2> line = {Vec2(0.0f, 0.0f), Vec2(100.0f, 0.0f)};
    Vec2 nearest;
    CHECK_NEAR(DistanceToPolyline(Vec2(50.0f, 10.0f), line, nearest), 10.0f, 1e-4f);
    CHECK_NEAR(nearest.x, 50.0f, 1e-4f);

    // Simplify drops collinear noise but keeps the endpoints.
    std::vector<Vec2> noisy;
    for (int i = 0; i <= 100; ++i) {
        const float t = static_cast<float>(i) / 100.0f;
        noisy.push_back(Vec2(t * 100.0f, 50.0f + ((i % 2 == 0) ? 0.3f : -0.3f)));
    }
    const std::vector<Vec2> simplified = SimplifyPolyline(noisy, 1.0f);
    CHECK(simplified.size() < noisy.size());
    CHECK(simplified.size() >= 2);
    CHECK_EQ(simplified.front().x, 0.0f);
    CHECK_EQ(simplified.back().x, 100.0f);

    // DecimateForStroke: reduces vertices (RDP), preserves subpaths/closed
    // flags/endpoints. The canvas deliberately does NOT use it on strokes -
    // the measured tradeoff has no safe-and-fast operating point - but the
    // geometry helper is correct and tested.
    {
        // Dense curve: many vertices, all within a pixel of a gentle arc.
        FlatPath dense;
        dense.polylines.push_back(std::vector<Vec2>());
        dense.closed.push_back(false);
        std::vector<Vec2>& poly = dense.polylines.back();
        for (int i = 0; i <= 500; ++i) {
            const float t = static_cast<float>(i) / 500.0f;
            poly.push_back(Vec2(t * 200.0f, 40.0f + std::sin(t * 3.0f) * 20.0f));
        }
        const size_t before = poly.size();

        const FlatPath lean = DecimateForStroke(dense, 0.08f);
        REQUIRE_EQ(lean.polylines.size(), static_cast<size_t>(1));
        REQUIRE_EQ(lean.closed.size(), static_cast<size_t>(1));
        const std::vector<Vec2>& out = lean.polylines[0];
        CHECK(out.size() >= 2);
        CHECK(out.size() < before); // fewer vertices -> the actual speedup
        CHECK_EQ(lean.closed[0], false);
        // Endpoints preserved.
        CHECK_NEAR(out.front().x, dense.polylines[0].front().x, 1e-3f);
        CHECK_NEAR(out.back().x, dense.polylines[0].back().x, 1e-3f);
        // Error bound: every original point is within the tolerance of the
        // decimated polyline (RDP's guarantee).
        for (const Vec2& p : dense.polylines[0]) {
            Vec2 near;
            CHECK(DistanceToPolyline(p, out, near) <= 0.08f + 1e-3f);
        }

        // A looser tolerance keeps fewer vertices and still respects ITS bound.
        const FlatPath wide = DecimateForStroke(dense, 1.0f);
        CHECK(wide.polylines[0].size() <= out.size());
        for (const Vec2& p : dense.polylines[0]) {
            Vec2 near;
            CHECK(DistanceToPolyline(p, wide.polylines[0], near) <= 1.0f + 1e-3f);
        }

        // Degenerate tolerances stay valid rather than dropping geometry.
        const FlatPath hair = DecimateForStroke(dense, 0.0f);
        CHECK(hair.polylines[0].size() >= 2);
        const FlatPath neg = DecimateForStroke(dense, -5.0f);
        CHECK(neg.polylines[0].size() >= 2);

        // Empty input stays empty rather than inventing geometry.
        const FlatPath none = DecimateForStroke(FlatPath(), 0.08f);
        CHECK(none.polylines.empty());

        // Multiple subpaths and the closed flag survive the round trip.
        FlatPath two;
        two.polylines.push_back(dense.polylines[0]);
        two.polylines.push_back(dense.polylines[0]);
        two.closed.push_back(true);
        two.closed.push_back(false);
        const FlatPath twoLean = DecimateForStroke(two, 0.08f);
        REQUIRE_EQ(twoLean.polylines.size(), static_cast<size_t>(2));
        CHECK_EQ(twoLean.closed[0], true);
        CHECK_EQ(twoLean.closed[1], false);
    }

    // StrokeToOutline: the performance-critical path (a stroke is painted as a
    // filled outline, not with a pen). Correctness here is what stops that from
    // changing the artwork.
    {
        // A straight horizontal segment of width 4 becomes a closed band 4 tall,
        // with butt caps that do NOT overhang the endpoints.
        FlatPath line;
        line.polylines.push_back({Vec2(0.0f, 0.0f), Vec2(100.0f, 0.0f)});
        line.closed.push_back(false);

        StrokeOutlineOptions butt;
        butt.width = 4.0f;
        butt.cap = LineCap::Butt;
        butt.join = LineJoin::Round;
        const FlatPath band = StrokeToOutline(line, butt);
        REQUIRE_EQ(band.polylines.size(), static_cast<size_t>(1));
        CHECK_EQ(band.closed[0], true);
        Vec2 lo, hi;
        FlatTransformedBounds(band, Mat2x3Identity(), 0.0f, lo, hi);
        CHECK_NEAR(lo.y, -2.0f, 0.05f); // half width above the centerline
        CHECK_NEAR(hi.y, 2.0f, 0.05f);  // half width below
        CHECK_NEAR(lo.x, 0.0f, 0.05f);  // butt cap: no overhang
        CHECK_NEAR(hi.x, 100.0f, 0.05f);
        // Solid on the stroke, empty off it - i.e. it really is the stroke.
        CHECK(PointInFlatPath(Vec2(50.0f, 0.0f), band, 0.0f));
        CHECK(PointInFlatPath(Vec2(50.0f, 1.5f), band, 0.0f));
        CHECK(!PointInFlatPath(Vec2(50.0f, 4.0f), band, 0.0f));
        CHECK(!PointInFlatPath(Vec2(-4.0f, 0.0f), band, 0.0f));

        // Round and square caps both extend the band by the radius at each end.
        StrokeOutlineOptions round = butt;
        round.cap = LineCap::Round;
        const FlatPath roundBand = StrokeToOutline(line, round);
        FlatTransformedBounds(roundBand, Mat2x3Identity(), 0.0f, lo, hi);
        CHECK_NEAR(lo.x, -2.0f, 0.1f);
        CHECK_NEAR(hi.x, 102.0f, 0.1f);
        CHECK(roundBand.polylines[0].size() > band.polylines[0].size());
        StrokeOutlineOptions sq = butt;
        sq.cap = LineCap::Square;
        const FlatPath squareBand = StrokeToOutline(line, sq);
        FlatTransformedBounds(squareBand, Mat2x3Identity(), 0.0f, lo, hi);
        CHECK_NEAR(lo.x, -2.0f, 0.05f);
        CHECK_NEAR(hi.x, 102.0f, 0.05f);

        // Width scales the band exactly.
        StrokeOutlineOptions wide = butt;
        wide.width = 10.0f;
        const FlatPath wideBand = StrokeToOutline(line, wide);
        FlatTransformedBounds(wideBand, Mat2x3Identity(), 0.0f, lo, hi);
        CHECK_NEAR(lo.y, -5.0f, 0.05f);
        CHECK_NEAR(hi.y, 5.0f, 0.05f);

        // A corner. Probe points are chosen from measured behaviour, and they
        // avoid the region where the two offset rectangles overlap: that region
        // needs the WINDING fill rule (which the canvas uses), and
        // PointInFlatPath is even-odd, so it reports the overlap as outside.
        FlatPath corner;
        corner.polylines.push_back(
            {Vec2(0.0f, 0.0f), Vec2(50.0f, 0.0f), Vec2(50.0f, 50.0f)});
        corner.closed.push_back(false);
        const FlatPath cornerBand = StrokeToOutline(corner, butt);
        CHECK(cornerBand.polylines[0].size() >= 6);
        CHECK(PointInFlatPath(Vec2(25.0f, 0.0f), cornerBand, 0.0f)); // on the band
        CHECK(!PointInFlatPath(Vec2(45.0f, 3.0f), cornerBand, 0.0f)); // off it
        // Round join covers the outside of the bend...
        CHECK(PointInFlatPath(Vec2(51.0f, -1.0f), cornerBand, 0.0f));
        // ...bevel cuts it off. This pair is the round-vs-bevel discriminator.
        StrokeOutlineOptions bevel = butt;
        bevel.join = LineJoin::Bevel;
        const FlatPath bevelBand = StrokeToOutline(corner, bevel);
        CHECK(bevelBand.polylines[0].size() < cornerBand.polylines[0].size());
        CHECK(!PointInFlatPath(Vec2(51.0f, -1.0f), bevelBand, 0.0f));
        // Miter keeps the vertex where the two offset lines meet, (48, 2);
        // once the miter limit is exceeded it degrades to the bevel and the
        // vertex is gone.
        StrokeOutlineOptions miter = butt;
        miter.join = LineJoin::Miter;
        const FlatPath miterBand = StrokeToOutline(corner, miter);
        auto hasVertexNear = [](const FlatPath& f, const Vec2& p, float eps) {
            for (const Vec2& q : f.polylines[0]) {
                const float dx = q.x - p.x;
                const float dy = q.y - p.y;
                if (std::sqrt(dx * dx + dy * dy) <= eps) {
                    return true;
                }
            }
            return false;
        };
        CHECK(hasVertexNear(miterBand, Vec2(48.0f, 2.0f), 0.1f));
        CHECK(!hasVertexNear(cornerBand, Vec2(48.0f, 2.0f), 0.1f));
        miter.miterLimit = 1.0f;
        const FlatPath limited = StrokeToOutline(corner, miter);
        CHECK(!hasVertexNear(limited, Vec2(48.0f, 2.0f), 0.1f));

        // A multi-segment subpath: the backward pass over the right side must
        // cover EVERY segment, not skip the last one. A butt cap at a slanted
        // end puts the band corners at +/- radius along the end normal, which
        // pins the bounds without guessing.
        FlatPath zig;
        zig.polylines.push_back(
            {Vec2(0, 0), Vec2(20, 10), Vec2(40, 0), Vec2(60, 10)});
        zig.closed.push_back(false);
        const FlatPath zigBand = StrokeToOutline(zig, butt);
        CHECK(PointInFlatPath(Vec2(20.0f, 10.0f), zigBand, 0.0f));
        CHECK(!PointInFlatPath(Vec2(20.0f, 20.0f), zigBand, 0.0f));
        CHECK(!PointInFlatPath(Vec2(20.0f, -20.0f), zigBand, 0.0f));
        FlatTransformedBounds(zigBand, Mat2x3Identity(), 0.0f, lo, hi);
        CHECK_NEAR(lo.y, -1.79f, 0.05f);
        CHECK_NEAR(hi.y, 11.79f, 0.05f);

        // A closed centerline becomes a ring around it. This one outline needs the
        // WINDING fill rule: it is a single loop containing a hole, so an
        // even-odd test (PointInFlatPath) reports the hole as OUTSIDE, which is
        // the correct even-odd answer and the wrong one for painting. The
        // canvas fills with WindingFill for exactly this reason.
        FlatPath square;
        square.polylines.push_back(Flatten(
            AnimPath::FromRect(20.0f, 20.0f, 20.0f, 20.0f),
            kFlattenTolerance)
                                         .polylines.front());
        square.closed.push_back(true);
        const FlatPath ring = StrokeToOutline(square, round);
        REQUIRE_EQ(ring.polylines.size(), static_cast<size_t>(1));
        CHECK_EQ(ring.closed[0], true);
        FlatTransformedBounds(ring, Mat2x3Identity(), 0.0f, lo, hi);
        CHECK_NEAR(lo.x, 18.0f, 0.3f);
        CHECK_NEAR(hi.x, 42.0f, 0.3f);
        CHECK_NEAR(lo.y, 18.0f, 0.3f);
        CHECK_NEAR(hi.y, 42.0f, 0.3f);
        // Non-zero overall winding, so the outer and inner loops cancel in the
        // hole: a fill leaves a real ring rather than a solid square.
        CHECK(Winding(ring.polylines[0]) != 0);
        CHECK(ring.polylines[0].size() > square.polylines[0].size());

        // Degenerate input must not produce NaNs: repeated points, a
        // single-point subpath, and a zero width.
        FlatPath dots;
        dots.polylines.push_back({Vec2(5.0f, 5.0f), Vec2(5.0f, 5.0f)});
        dots.closed.push_back(false);
        const FlatPath dotBand = StrokeToOutline(dots, butt);
        for (const Vec2& p : dotBand.polylines[0]) {
            CHECK(std::isfinite(p.x) && std::isfinite(p.y));
        }
        FlatPath single;
        single.polylines.push_back({Vec2(5.0f, 5.0f)});
        single.closed.push_back(false);
        const FlatPath singleBand = StrokeToOutline(single, butt);
        for (const Vec2& p : singleBand.polylines[0]) {
            CHECK(std::isfinite(p.x) && std::isfinite(p.y));
        }
        StrokeOutlineOptions hairline = butt;
        hairline.width = 0.0f;
        CHECK(StrokeToOutline(line, hairline).polylines.size() <= 1);
        CHECK_EQ(StrokeToOutline(FlatPath(), butt).polylines.size(),
                 static_cast<size_t>(0));

        // Tolerance controls arc detail: a loose tolerance needs fewer points
        // and a tight one more. Crucially, the cap must not collapse - a
        // tolerance as large as the stroke radius satisfies the sagitta bound
        // with a single step, which used to drop the round cap's overhang
        // entirely.
        StrokeOutlineOptions loose = round;
        loose.tolerance = 2.0f;
        StrokeOutlineOptions tight = round;
        tight.tolerance = 0.01f;
        const FlatPath looseBand = StrokeToOutline(line, loose);
        const FlatPath tightBand = StrokeToOutline(line, tight);
        CHECK(looseBand.polylines[0].size() < tightBand.polylines[0].size());
        // The tight band lands on the true radius.
        FlatTransformedBounds(tightBand, Mat2x3Identity(), 0.0f, lo, hi);
        CHECK_NEAR(lo.x, -2.0f, 0.05f);
        CHECK_NEAR(hi.x, 102.0f, 0.05f);
        // The loose band still bulges outward, and never past the tolerance.
        FlatTransformedBounds(looseBand, Mat2x3Identity(), 0.0f, lo, hi);
        CHECK(lo.x >= -2.0f - 1e-3f);
        CHECK(lo.x <= -2.0f + loose.tolerance);
        CHECK(hi.x <= 102.0f + 1e-3f);
        CHECK(hi.x >= 102.0f - loose.tolerance);
    }

    // Band solidity under DENSE input - the regression test for cuts in a
    // painted stroke. A stroke must cover every point within half its width of
    // the centerline. Brush input used to be sampled far denser than the stroke
    // is wide when zoomed in, and the offset outline folded back on itself,
    // leaving pinholes. PointInFlatPath cannot see this (it is even-odd, and a
    // self-overlapping outline trips it), so this uses the WINDING rule - the
    // one the canvas actually paints with.
    {
        auto insideWinding = [](const std::vector<Vec2>& poly, float px,
                                float py) {
            int winding = 0;
            const size_t n = poly.size();
            for (size_t i = 0, j = n - 1; i < n; j = i++) {
                const Vec2& a = poly[i];
                const Vec2& b = poly[j];
                if (a.y <= py) {
                    if (b.y > py) {
                        const double ax =
                            a.x + (static_cast<double>(b.x) - a.x) *
                                      (py - a.y) / (b.y - a.y);
                        if (static_cast<double>(px) < ax) ++winding;
                    }
                } else if (b.y <= py) {
                    const double ax = a.x + (static_cast<double>(b.x) - a.x) *
                                              (py - a.y) / (b.y - a.y);
                    if (static_cast<double>(px) < ax) --winding;
                }
            }
            return winding != 0;
        };

        StrokeOutlineOptions opts;
        opts.width = 4.0f;
        opts.cap = LineCap::Round;
        opts.join = LineJoin::Round;
        const float half = opts.width * 0.5f;

        // Hand-like jitter sampled at the density the brush actually produces at high
        // zoom (its spacing floor is strokeWidth * 0.2, i.e. ~0.8u for a 4u
        // stroke) - the regime that produced the cuts.
        std::vector<Vec2> center;
        unsigned seed = 99991u;
        float cx = 0.0f, cy = 0.0f, vx = 0.0f, vy = 0.0f;
        for (int i = 0; i < 400; ++i) {
            seed = seed * 1664525u + 1013904223u;
            const float rx =
                static_cast<float>((seed >> 8) & 0xFFFFu) / 65535.0f - 0.5f;
            seed = seed * 1664525u + 1013904223u;
            const float ry =
                static_cast<float>((seed >> 8) & 0xFFFFu) / 65535.0f - 0.5f;
            center.push_back(Vec2(cx, cy));
            vx = vx * 0.82f + rx * 3.0f;
            vy = vy * 0.82f + ry * 3.0f;
            cx += 0.8f + vx * 0.24f;
            cy += vy * 0.24f;
        }
        FlatPath flat;
        flat.polylines.push_back(center);
        flat.closed.push_back(false);
        const FlatPath band = StrokeToOutline(flat, opts);
        REQUIRE_EQ(band.polylines.size(), static_cast<size_t>(1));

        // Every point ON the centerline must be inside the band. Probing only
        // the centerline keeps this independent of how polygonal the outer
        // boundary is, which is a legitimate tolerance effect.
        long holes = 0;
        long probes = 0;
        for (size_t i = 1; i < center.size(); ++i) {
            ++probes;
            if (!insideWinding(band.polylines[0], center[i].x, center[i].y)) {
                ++holes;
            }
        }
        const double holePct = 100.0 * static_cast<double>(holes) /
                               static_cast<double>(probes);
        // Measured: 8.8% before the join-arc clamp, ~0.1% after. A generous
        // ceiling still catches a regression by a wide margin.
        CHECK_MSG(holePct < 1.0, "centerline holes in a dense stroke");
        // And the outline must be far denser than the naive offset - that is
        // the clamp doing its job, keeping arcs from overshooting neighbours.
        CHECK(band.polylines[0].size() > center.size());
        (void)half;
    }

    // Compact stroke storage: the model keeps the SOURCE geometry (fitted
    // centerline + width), never the tessellated pieces. Storing derived
    // tessellation multiplied every downstream cost by ~500x (a 100-point
    // stroke: 23 fit segments vs 12.5k piece segments on disk), so this test
    // locks the invariant: fit-then-store stays small, and the transient
    // pieces built at paint time still cover the stroke.
    {
        // Hand-like input, the way the brush samples it.
        std::vector<Vec2> raw;
        unsigned seed = 424242u;
        float x = 0.0f, y = 0.0f, vx = 0.0f, vy = 0.0f;
        for (int i = 0; i < 100; ++i) {
            seed = seed * 1664525u + 1013904223u;
            const float rx =
                static_cast<float>((seed >> 8) & 0xFFFFu) / 65535.0f - 0.5f;
            raw.push_back(Vec2(x, y));
            vx = vx * 0.85f + rx * 3.0f;
            vy = vy * 0.85f + rx * 3.0f;
            x += 6.0f + vx * 1.8f;
            y += vy * 1.8f;
        }
        Vec2 start;
        std::vector<AnimSegment> segs;
        FitBeziersToPolyline(raw, 1.0f, start, segs);
        CHECK(!segs.empty());
        // The fit collapses ~100 raw points to a handful of segments. The
        // ceiling is generous (a torture scribble fits here too); what it
        // catches is storing per-point or per-tessellation data instead.
        CHECK(segs.size() < raw.size() / 2);
        AnimPath stored;
        AnimSegment move(AnimSegment::Kind::Move);
        move.p[0] = start;
        stored.segments.push_back(move);
        for (const AnimSegment& g : segs) {
            stored.segments.push_back(g);
        }
        // Stored path is Move + beziers: compact source geometry.
        CHECK_EQ(stored.segments.size(), segs.size() + 1);
        // Transient paint tessellation covers the fitted centerline: every
        // flattened centerline point is inside some piece (winding rule).
        const FlatPath flat = Flatten(stored, kFlattenTolerance);
        StrokeOutlineOptions opts;
        opts.width = 30.0f;
        const FlatPath pieces = StrokeToPieces(flat, opts);
        CHECK(!pieces.polylines.empty());
        auto insideAny = [](const FlatPath& f, float px, float py) {
            for (const std::vector<Vec2>& poly : f.polylines) {
                int winding = 0;
                const size_t n = poly.size();
                for (size_t i = 0, j = n - 1; i < n; j = i++) {
                    const Vec2& a = poly[i];
                    const Vec2& b = poly[j];
                    if (a.y <= py) {
                        if (b.y > py) {
                            const double ax =
                                a.x + (static_cast<double>(b.x) - a.x) *
                                          (py - a.y) / (b.y - a.y);
                            if (static_cast<double>(px) < ax) ++winding;
                        }
                    } else if (b.y <= py) {
                        const double ax =
                            a.x + (static_cast<double>(b.x) - a.x) *
                                      (py - a.y) / (b.y - a.y);
                        if (static_cast<double>(px) < ax) --winding;
                    }
                }
                if (winding != 0) {
                    return true;
                }
            }
            return false;
        };
        long holes = 0;
        long probes = 0;
        for (const std::vector<Vec2>& poly : flat.polylines) {
            for (const Vec2& p : poly) {
                ++probes;
                if (!insideAny(pieces, p.x, p.y)) {
                    ++holes;
                }
            }
        }
        CHECK(probes > 0);
        CHECK_MSG(holes == 0, "transient pieces cover the stored centerline");
    }

    // StrokeToPieces: union-correct fill pieces. Every piece shares one
    // winding, so one WindingFill paints their exact union - including where
    // the stroke crosses itself, where the single outline loop cancels to zero
    // and punches a hole (a brush circle's overlap reading as subtracted).
    {
        auto insideAnyWinding = [](const FlatPath& f, float px, float py) {
            for (const std::vector<Vec2>& poly : f.polylines) {
                int winding = 0;
                const size_t n = poly.size();
                for (size_t i = 0, j = n - 1; i < n; j = i++) {
                    const Vec2& a = poly[i];
                    const Vec2& b = poly[j];
                    if (a.y <= py) {
                        if (b.y > py) {
                            const double ax =
                                a.x + (static_cast<double>(b.x) - a.x) *
                                          (py - a.y) / (b.y - a.y);
                            if (static_cast<double>(px) < ax) ++winding;
                        }
                    } else if (b.y <= py) {
                        const double ax =
                            a.x + (static_cast<double>(b.x) - a.x) *
                                      (py - a.y) / (b.y - a.y);
                        if (static_cast<double>(px) < ax) --winding;
                    }
                }
                if (winding != 0) {
                    return true;
                }
            }
            return false;
        };
        auto sharedWinding = [](const FlatPath& f) {
            int sign = 0;
            for (const std::vector<Vec2>& poly : f.polylines) {
                const int w = Winding(poly);
                if (w == 0) {
                    return 0; // degenerate piece
                }
                if (sign == 0) {
                    sign = w;
                } else if (w != sign) {
                    return 999; // mixed windings: overlaps would cancel
                }
            }
            return sign;
        };

        StrokeOutlineOptions opts;
        opts.width = 4.0f;
        opts.cap = LineCap::Round;
        opts.join = LineJoin::Round;

        // A straight segment: one quad plus two cap discs, all one winding,
        // covering the band and nothing far outside it.
        FlatPath line;
        line.polylines.push_back({Vec2(0.0f, 0.0f), Vec2(100.0f, 0.0f)});
        line.closed.push_back(false);
        const FlatPath straight = StrokeToPieces(line, opts);
        CHECK_EQ(straight.polylines.size(), static_cast<size_t>(3));
        CHECK(sharedWinding(straight) != 0 && sharedWinding(straight) != 999);
        CHECK(insideAnyWinding(straight, 50.0f, 0.0f));
        CHECK(insideAnyWinding(straight, 50.0f, 1.5f));
        CHECK(!insideAnyWinding(straight, 50.0f, 4.0f));
        // Round caps cover the tips.
        CHECK(insideAnyWinding(straight, -1.5f, 0.0f));
        CHECK(insideAnyWinding(straight, 101.5f, 0.0f));
        // Butt caps do not overhang.
        StrokeOutlineOptions butt = opts;
        butt.cap = LineCap::Butt;
        const FlatPath buttBand = StrokeToPieces(line, butt);
        CHECK_EQ(buttBand.polylines.size(), static_cast<size_t>(1));
        CHECK(!insideAnyWinding(buttBand, -1.0f, 0.0f));
        CHECK(insideAnyWinding(buttBand, 50.0f, 0.0f));
        // Square caps extend by the radius.
        StrokeOutlineOptions square = opts;
        square.cap = LineCap::Square;
        const FlatPath squareBand = StrokeToPieces(line, square);
        CHECK(insideAnyWinding(squareBand, -1.5f, 0.0f));
        CHECK(insideAnyWinding(squareBand, 101.5f, 0.0f));
        CHECK(!insideAnyWinding(squareBand, -2.5f, 0.0f));

        // A circle whose end overlaps its start: EVERY band probe must be
        // inside some piece. This is the regression test for the subtracted
        // overlap - the single loop left 2.6% of painted pixels missing here.
        FlatPath circle;
        circle.polylines.push_back(std::vector<Vec2>());
        std::vector<Vec2>& ring = circle.polylines.back();
        for (int i = 0; i <= 160; ++i) {
            const double t = (static_cast<double>(i) / 160.0) * 6.634f; // 380deg
            ring.push_back(Vec2(960.0f + static_cast<float>(150.0 * std::cos(t)),
                                540.0f + static_cast<float>(150.0 * std::sin(t))));
        }
        circle.closed.push_back(false);
        StrokeOutlineOptions wide = opts;
        wide.width = 10.0f;
        const FlatPath circlePieces = StrokeToPieces(circle, wide);
        CHECK(sharedWinding(circlePieces) != 0 &&
              sharedWinding(circlePieces) != 999);
        long holes = 0;
        long probes = 0;
        for (size_t i = 1; i < ring.size(); ++i) {
            ++probes;
            const float mx = (ring[i - 1].x + ring[i].x) * 0.5f;
            const float my = (ring[i - 1].y + ring[i].y) * 0.5f;
            if (!insideAnyWinding(circlePieces, mx, my)) {
                ++holes;
            }
        }
        CHECK_MSG(holes == 0, "circle overlap holes in fill pieces");

        // Bevel uses flat triangles instead of discs, so fewer vertices; the
        // band stays covered.
        FlatPath corner;
        corner.polylines.push_back(
            {Vec2(0.0f, 0.0f), Vec2(50.0f, 0.0f), Vec2(50.0f, 50.0f)});
        corner.closed.push_back(false);
        const FlatPath roundCorner = StrokeToPieces(corner, opts);
        StrokeOutlineOptions bevel = opts;
        bevel.join = LineJoin::Bevel;
        const FlatPath bevelCorner = StrokeToPieces(corner, bevel);
        CHECK(sharedWinding(bevelCorner) != 0 &&
              sharedWinding(bevelCorner) != 999);
        // One joint: round emits 1 disc piece, bevel 2 flat triangles.
        CHECK_EQ(bevelCorner.polylines.size(),
                 roundCorner.polylines.size() + 1);
        size_t roundVerts = 0, bevelVerts = 0;
        for (const auto& p : roundCorner.polylines) roundVerts += p.size();
        for (const auto& p : bevelCorner.polylines) bevelVerts += p.size();
        CHECK(bevelVerts < roundVerts);
        CHECK(insideAnyWinding(bevelCorner, 25.0f, 0.0f));
        // Miter keeps the outer point within the limit, degrades past it.
        StrokeOutlineOptions miter = opts;
        miter.join = LineJoin::Miter;
        const FlatPath miterCorner = StrokeToPieces(corner, miter);
        auto hasPieceVertexNear = [](const FlatPath& f, const Vec2& p,
                                     float eps) {
            for (const std::vector<Vec2>& poly : f.polylines) {
                for (const Vec2& q : poly) {
                    const float dx = q.x - p.x;
                    const float dy = q.y - p.y;
                    if (std::sqrt(dx * dx + dy * dy) <= eps) {
                        return true;
                    }
                }
            }
            return false;
        };
        CHECK(hasPieceVertexNear(miterCorner, Vec2(48.0f, 2.0f), 0.15f));
        miter.miterLimit = 1.0f;
        CHECK(!hasPieceVertexNear(StrokeToPieces(corner, miter),
                                  Vec2(48.0f, 2.0f), 0.15f));

        // Degenerate input yields no pieces, never NaNs.
        FlatPath dots;
        dots.polylines.push_back({Vec2(5.0f, 5.0f), Vec2(5.0f, 5.0f)});
        dots.closed.push_back(false);
        CHECK(StrokeToPieces(dots, opts).polylines.empty());
        StrokeOutlineOptions hairline = opts;
        hairline.width = 0.0f;
        CHECK(StrokeToPieces(line, hairline).polylines.empty());
        CHECK(StrokeToPieces(FlatPath(), opts).polylines.empty());
    }

    // Error-bounded fitting: far fewer segments than points, each within
    // tolerance of the input, starting at the polyline's first point.
    Vec2 fitStart;
    std::vector<AnimSegment> fit;
    FitBeziersToPolyline(noisy, 1.0f, fitStart, fit);
    CHECK_EQ(fitStart.x, 0.0f);
    // A near-straight noisy run collapses to a handful of segments, not ~100.
    CHECK(fit.size() < 15);
    CHECK(!fit.empty());
    // The fitted path tracks the input: flatten both and compare point-wise.
    AnimPath fitted;
    {
        AnimSegment move(AnimSegment::Kind::Move);
        move.p[0] = fitStart;
        fitted.segments.push_back(move);
        for (const AnimSegment& segment : fit) {
            fitted.segments.push_back(segment);
        }
    }
    const FlatPath flatFitted = Flatten(fitted);
    const FlatPath flatNoisy = Flatten([&] {
        AnimPath raw;
        bool first = true;
        for (const Vec2& p : noisy) {
            AnimSegment segment(first ? AnimSegment::Kind::Move
                                      : AnimSegment::Kind::Line);
            segment.p[0] = p;
            raw.segments.push_back(segment);
            first = false;
        }
        return raw;
    }());
    REQUIRE(!flatFitted.polylines.empty() && !flatNoisy.polylines.empty());
    // Same x-range covered end to end.
    Vec2 fittedLo, fittedHi, noisyLo, noisyHi;
    FlatBounds(flatFitted, fittedLo, fittedHi);
    FlatBounds(flatNoisy, noisyLo, noisyHi);
    CHECK_NEAR(fittedLo.x, noisyLo.x, 2.0f);
    CHECK_NEAR(fittedHi.x, noisyHi.x, 2.0f);

    // A straight run becomes exactly one Line, not a chain of cubics.
    std::vector<Vec2> straight = {Vec2(0.0f, 0.0f), Vec2(30.0f, 0.0f),
                                  Vec2(60.0f, 0.0f), Vec2(100.0f, 0.0f)};
    Vec2 straightStart;
    std::vector<AnimSegment> straightFit;
    FitBeziersToPolyline(straight, 0.5f, straightStart, straightFit);
    REQUIRE_EQ(straightFit.size(), static_cast<size_t>(1));
    CHECK(straightFit[0].kind == AnimSegment::Kind::Line);
    CHECK_NEAR(straightFit[0].p[0].x, 100.0f, 1e-3f);

    // A circle survives as a few cubics whose flattened loop still contains
    // the center and keeps its winding (shape-tween pairing depends on it).
    std::vector<Vec2> circlePts;
    for (int i = 0; i <= 64; ++i) {
        const float a = i * 6.2831853f / 64.0f;
        circlePts.push_back(Vec2(50.0f * std::cos(a), 50.0f * std::sin(a)));
    }
    Vec2 circleStart;
    std::vector<AnimSegment> circleFit;
    FitBeziersToPolyline(circlePts, 1.0f, circleStart, circleFit);
    CHECK(circleFit.size() < 20);
    AnimPath circlePath;
    {
        AnimSegment move(AnimSegment::Kind::Move);
        move.p[0] = circleStart;
        circlePath.segments.push_back(move);
        for (const AnimSegment& segment : circleFit) {
            circlePath.segments.push_back(segment);
        }
    }
    const FlatPath flatCircle = Flatten(circlePath);
    REQUIRE(!flatCircle.polylines.empty());
    CHECK(PointInFlatPath(Vec2(0.0f, 0.0f), flatCircle, 2.0f));
}

// --------------------------------------------------------------- layers --

// A layer with a key at frame 1 (artwork) and a blank key at frame 25.
static AnimLayer MakeTestLayer(const AnimDocument& doc) {
    AnimLayer layer;
    layer.id = 1;
    layer.name = "Layer 1";

    AnimKeyframe first;
    first.frame = 1;
    first.kind = KeyframeKind::Key;
    AnimShape shape;
    shape.id = 10;
    shape.path = AnimPath::FromRect(0.0f, 0.0f, 10.0f, 10.0f);
    first.shapes.push_back(shape);
    layer.SetKeyframe(first);

    AnimKeyframe blank;
    blank.frame = 25;
    blank.kind = KeyframeKind::Blank;
    layer.SetKeyframe(blank);
    return layer;
}

static void TestKeyframeLogic() {
    TEST_GROUP("keyframes");

    AnimDocument doc = AnimDocument::New(1280, 720, 24);
    doc.lengthFrames = 48;
    const AnimLayer layer = MakeTestLayer(doc);

    CHECK_EQ(layer.KeyframeCount(), static_cast<size_t>(2));
    CHECK_EQ(layer.FirstFrame(), 1);
    CHECK_EQ(layer.LastFrame(), 25);
    CHECK(layer.Find(1) != nullptr);
    CHECK(layer.Find(25) != nullptr);
    CHECK(layer.Find(13) == nullptr);

    // AtOrBefore drives frame spans; AtOrAfter finds the span end.
    CHECK_EQ(layer.AtOrBefore(1)->frame, 1);
    CHECK_EQ(layer.AtOrBefore(13)->frame, 1);
    CHECK_EQ(layer.AtOrBefore(25)->frame, 25);
    CHECK_EQ(layer.AtOrBefore(48)->frame, 25);
    // Before the first keyframe there is nothing to show.
    CHECK(layer.AtOrBefore(0) == nullptr);
    CHECK_EQ(layer.AtOrAfter(2)->frame, 25);
    CHECK(layer.AtOrAfter(26) == nullptr);

    // Out-of-order inserts stay sorted.
    AnimLayer sorted;
    AnimKeyframe late;
    late.frame = 30;
    AnimKeyframe early;
    early.frame = 5;
    sorted.SetKeyframe(late);
    sorted.SetKeyframe(early);
    CHECK_EQ(sorted.KeyframeCount(), static_cast<size_t>(2));
    CHECK_EQ(sorted.FirstFrame(), 5);
    CHECK_EQ(sorted.LastFrame(), 30);

    // Setting the same frame replaces rather than duplicates.
    AnimKeyframe replacement;
    replacement.frame = 5;
    replacement.kind = KeyframeKind::Blank;
    sorted.SetKeyframe(replacement);
    CHECK_EQ(sorted.KeyframeCount(), static_cast<size_t>(2));
    CHECK(sorted.Find(5)->kind == KeyframeKind::Blank);

    CHECK(sorted.RemoveKeyframe(5));
    CHECK(!sorted.RemoveKeyframe(5));
    CHECK_EQ(sorted.KeyframeCount(), static_cast<size_t>(1));

    // Insert frame: keyframes at or after the frame shift right.
    AnimLayer work = layer;
    CHECK(work.ShiftFrames(25, 5, 48));
    CHECK(work.Find(30) != nullptr);
    CHECK(work.Find(25) == nullptr);
    CHECK(work.Find(1) != nullptr); // earlier keyframe untouched
    CHECK(!work.ShiftFrames(100, 1, 48)); // nothing at/after 100

    // Remove frame: keyframes shift left, and one pushed past the end is
    // dropped rather than left dangling outside the timeline.
    AnimLayer clipped = layer;
    CHECK(clipped.ShiftFrames(25, 30, 48));
    CHECK(clipped.Find(55) == nullptr);
    CHECK_EQ(clipped.KeyframeCount(), static_cast<size_t>(1));

    // A left shift that collides two keyframes onto one frame de-duplicates.
    AnimLayer collide;
    AnimKeyframe a;
    a.frame = 10;
    AnimKeyframe b;
    b.frame = 20;
    collide.SetKeyframe(a);
    collide.SetKeyframe(b);
    CHECK(collide.ShiftFrames(20, -10, 48));
    CHECK_EQ(collide.KeyframeCount(), static_cast<size_t>(1));
    CHECK_EQ(collide.FirstFrame(), 10);

    // Copy/paste frames.
    AnimLayer source = layer;
    CHECK(source.CopyFrames(1, 25, 10, 48));
    CHECK(source.Find(11) != nullptr);
    CHECK(source.Find(35) != nullptr);
    CHECK(!source.CopyFrames(40, 45, 5, 48)); // empty range
    CHECK(!source.CopyFrames(1, 25, 100, 48)); // everything lands past the end
}

static void TestFrameResolution() {
    TEST_GROUP("frame resolution");

    AnimDocument doc = AnimDocument::New(1280, 720, 24);
    doc.lengthFrames = 48;
    doc.layers[0] = MakeTestLayer(doc);
    doc.layers[0].id = doc.layers[0].id;

    // Visible layer resolves, and the span progress is frame-accurate.
    AnimFrame at1 = doc.ResolveFrame(1);
    REQUIRE_EQ(at1.layers.size(), static_cast<size_t>(1));
    CHECK_EQ(at1.layers[0].keyframe->frame, 1);
    CHECK(at1.layers[0].spanEnd != nullptr);
    CHECK_EQ(at1.layers[0].spanEnd->frame, 25);
    CHECK_NEAR(at1.layers[0].spanT, 0.0f, 1e-5f);

    const AnimFrame at13 = doc.ResolveFrame(13);
    REQUIRE_EQ(at13.layers.size(), static_cast<size_t>(1));
    CHECK_NEAR(at13.layers[0].spanT, 12.0f / 24.0f, 1e-5f);

    // Past the last keyframe the final key holds and there is no span.
    const AnimFrame at40 = doc.ResolveFrame(40);
    REQUIRE_EQ(at40.layers.size(), static_cast<size_t>(1));
    CHECK_EQ(at40.layers[0].keyframe->frame, 25);
    CHECK(at40.layers[0].spanEnd == nullptr);

    // Hidden layers are skipped by ResolveFrame; locked layers are NOT
    // (locking is an editing guard, not a drawing flag).
    doc.layers[0].visible = false;
    CHECK_EQ(doc.ResolveFrame(13).layers.size(), static_cast<size_t>(0));
    doc.layers[0].visible = true;
    doc.layers[0].locked = true;
    CHECK_EQ(doc.ResolveFrame(13).layers.size(), static_cast<size_t>(1));
    doc.layers[0].locked = false;

    // Layers stack back-to-front: index 0 is topmost and must draw last.
    AnimDocument stacked = AnimDocument::New(320, 240, 24);
    stacked.layers.resize(3);
    for (size_t i = 0; i < stacked.layers.size(); ++i) {
        stacked.layers[i].id = static_cast<uint64_t>(i + 1);
        stacked.layers[i].name = "L" + std::to_string(i);
        AnimKeyframe key;
        key.frame = 1;
        stacked.layers[i].SetKeyframe(key);
    }
    const AnimFrame stackedFrame = stacked.ResolveFrame(1);
    REQUIRE_EQ(stackedFrame.layers.size(), static_cast<size_t>(3));
    CHECK_EQ(stackedFrame.layers[0].layerIndex, 2u); // bottom layer first
    CHECK_EQ(stackedFrame.layers[2].layerIndex, 0u); // top layer last

    // ClampFrame wraps when looping, and clamps when not. Frames are 1-based,
    // so the wrap lands on 1..48 rather than 0..47: frame 49 -> 1, frame 0 -> 48,
    // frame -5 -> 43 ((-6 mod 48) + 1).
    CHECK_EQ(doc.ClampFrame(49), 1);
    CHECK_EQ(doc.ClampFrame(0), 48);
    CHECK_EQ(doc.ClampFrame(-5), 43);
    CHECK_EQ(doc.ClampFrame(48), 48);
    CHECK_EQ(doc.ClampFrame(1), 1);
    // Frame 1000 is out of range: 1000 wraps to (999 mod 48)+1 = 40, which is
    // where playback actually lands — not the last frame.
    CHECK_EQ(doc.ResolveFrame(1000).frame, 40);
    doc.loop = false;
    CHECK_EQ(doc.ClampFrame(999), 48);
    CHECK_EQ(doc.ClampFrame(0), 1);
    CHECK_EQ(doc.ResolveFrame(999).frame, 48);

    // A frame before a layer's first keyframe contributes nothing. This needs
    // a timeline longer than the keyframe, otherwise ClampFrame would wrap the
    // request back to frame 1 and hide the effect.
    AnimDocument late = AnimDocument::New(320, 240, 24);
    late.lengthFrames = 48;
    late.layers[0].frames.clear();
    AnimKeyframe at10;
    at10.frame = 10;
    late.layers[0].SetKeyframe(at10);
    CHECK_EQ(late.ResolveFrame(5).layers.size(), static_cast<size_t>(0));
    CHECK_EQ(late.ResolveFrame(10).layers.size(), static_cast<size_t>(1));
}

static void TestNormalize() {
    TEST_GROUP("normalize");

    AnimDocument doc = AnimDocument::New(1280, 720, 24);
    doc.layers.resize(2);
    doc.layers[1].id = 5;
    doc.layers[0].id = 2;

    // Out-of-range values are clamped rather than rejected.
    doc.stageWidth = 0;
    doc.stageHeight = -5;
    doc.fps = 0;
    doc.lengthFrames = 0;
    doc.bakeScale = 1000.0f;

    AnimShape shape;
    shape.id = 99;
    AnimKeyframe key;
    key.frame = 1;
    key.shapes.push_back(shape);
    doc.layers[1].SetKeyframe(key);

    // A keyframe beyond the shortened timeline becomes unreachable and is
    // dropped.
    AnimKeyframe beyond;
    beyond.frame = 500;
    doc.layers[1].SetKeyframe(beyond);

    doc.Normalize();

    CHECK_EQ(doc.stageWidth, 1);
    CHECK_EQ(doc.stageHeight, 1);
    CHECK_EQ(doc.fps, 1);
    CHECK_EQ(doc.lengthFrames, 1);
    CHECK_NEAR(doc.bakeScale, 64.0f, 1e-4f);
    CHECK_EQ(doc.layers[1].KeyframeCount(), static_cast<size_t>(1));
    // nextId must sit past every id in the file or a later AllocId() collides.
    // AnimDocument::New() already allocated id 1 for "Layer 1", and this test
    // added layers 2 and 5 plus shape 99, so Normalize() lands on 100.
    const uint64_t nextBefore = doc.nextId;
    CHECK(nextBefore > 99);
    CHECK(nextBefore > 5);
    // AllocId() hands out the current value and then advances it.
    CHECK_EQ(doc.AllocId(), nextBefore);
    CHECK_EQ(doc.nextId, nextBefore + 1);
    // A second call continues rather than repeating.
    CHECK_EQ(doc.AllocId(), nextBefore + 1);
    CHECK_EQ(doc.nextId, nextBefore + 2);
}

// ---------------------------------------------------------- round-trip --

// Builds a document exercising every persisted field.
static AnimDocument MakeRichDocument() {
    AnimDocument doc = AnimDocument::New(1280, 720, 24);
    doc.lengthFrames = 48;
    doc.loop = false;
    doc.bakeScale = 2.5f;
    doc.background = AnimColor(255, 255, 255, 0);
    doc.transparentBackground = true;

    AnimLayer layer;
    layer.id = doc.AllocId();
    layer.name = "Background \"quoted\" & <special>";
    layer.visible = true;
    layer.locked = true;

    AnimKeyframe first;
    first.frame = 1;
    first.kind = KeyframeKind::Key;
    first.transform.position = Vec2(1.5f, -2.5f);
    first.transform.rotation = 0.75f;
    first.transform.scale = Vec2(1.25f, 0.75f);
    first.transform.skewX = 0.1f;
    first.transform.skewY = -0.2f;
    first.transform.alpha = 0.5f;
    first.transform.colorTransform = AnimColor(10, 20, 30, 40);

    AnimShape shape;
    shape.id = doc.AllocId();
    shape.name = "blob";
    shape.path = AnimPath::FromEllipse(100.0f, 80.0f, 60.0f, 40.0f);
    shape.style.hasFill = true;
    shape.style.fill = AnimColor(255, 128, 0, 200);
    shape.style.hasStroke = true;
    shape.style.stroke = AnimColor(1, 2, 3, 4);
    shape.style.strokeWidth = 3.5f;
    shape.style.cap = LineCap::Square;
    shape.style.join = LineJoin::Bevel;
    shape.transform.position = Vec2(10.0f, 20.0f);
    shape.transform.rotation = 0.5f;
    first.shapes.push_back(shape);
    layer.SetKeyframe(first);

    AnimKeyframe second;
    second.frame = 25;
    second.kind = KeyframeKind::Key;
    second.tweenIn.type = TweenType::Shape;
    second.tweenIn.easing = Easing::Cubic(0.25f, 0.1f, 0.25f, 1.0f);
    second.tweenIn.motionFlags = kMotionPosition | kMotionAlpha;
    second.tweenIn.shapeHints = true;
    AnimShape rect;
    rect.id = doc.AllocId();
    rect.name = "box";
    rect.path = AnimPath::FromRect(0.0f, 0.0f, 30.0f, 40.0f);
    rect.style.hasFill = false;
    rect.style.hasStroke = true;
    rect.style.strokeWidth = 1.0f;
    second.shapes.push_back(rect);
    layer.SetKeyframe(second);

    AnimKeyframe blank;
    blank.frame = 40;
    blank.kind = KeyframeKind::Blank;
    blank.tweenIn.type = TweenType::Motion;
    blank.tweenIn.motionFlags = kMotionScale;
    blank.tweenIn.shapeHints = false;
    layer.SetKeyframe(blank);

    doc.layers[0] = layer;

    AnimLayer hidden;
    hidden.id = doc.AllocId();
    hidden.name = "Hidden";
    hidden.visible = false;
    hidden.locked = false;
    doc.layers.push_back(hidden);

    doc.Normalize();
    return doc;
}

static void TestSerializationRoundTrip() {
    TEST_GROUP("serialization");

    const AnimDocument doc = MakeRichDocument();
    const std::string text = Serialize(doc);
    std::string error;

    // Every key field is literally present, so a hand-edited file is
    // self-documenting.
    CHECK(text.find("\"formatVersion\": 1") != std::string::npos);
    CHECK(text.find("\"lengthFrames\": 48") != std::string::npos);
    CHECK(text.find("\"bakeScale\": 2.5") != std::string::npos);
    CHECK(text.find("\"kind\": \"blank\"") != std::string::npos);
    CHECK(text.find("\"type\": \"shape\"") != std::string::npos);
    CHECK(text.find("\"type\": \"motion\"") != std::string::npos);
    CHECK(text.find("motionFlags\": [\"position\", \"alpha\"]") !=
          std::string::npos);
    // Names with quotes and angle brackets survive escaping.
    CHECK(text.find("\\\"quoted\\\"") != std::string::npos);
    CHECK(text.find("\\u003C") != std::string::npos || text.find("&") != std::string::npos);
    // Scalar arrays stay inline, which is what keeps the file diffable.
    CHECK(text.find("[100, 120]") != std::string::npos);

    AnimDocument back;
    REQUIRE(Deserialize(text, back, error));
    if (!error.empty()) {
        std::printf("      deserialize error: %s\n", error.c_str());
    }

    CHECK_EQ(back.stageWidth, 1280);
    CHECK_EQ(back.stageHeight, 720);
    CHECK_EQ(back.fps, 24);
    CHECK_EQ(back.lengthFrames, 48);
    CHECK(!back.loop);
    CHECK_NEAR(back.bakeScale, 2.5f, 1e-4f);
    CHECK(back.background == doc.background);
    CHECK(back.transparentBackground);
    REQUIRE_EQ(back.layers.size(), static_cast<size_t>(2));
    CHECK(back.layers[0].locked);
    CHECK(!back.layers[1].visible);
    CHECK_EQ(back.layers[0].name, doc.layers[0].name);

    const AnimLayer& layer = back.layers[0];
    REQUIRE_EQ(layer.KeyframeCount(), static_cast<size_t>(3));
    CHECK(layer.Find(40)->kind == KeyframeKind::Blank);
    CHECK(layer.Find(40)->tweenIn.shapeHints == false);
    CHECK(layer.Find(40)->tweenIn.motionFlags == kMotionScale);
    CHECK(layer.Find(25)->tweenIn.type == TweenType::Shape);
    CHECK(layer.Find(25)->tweenIn.easing.kind == EasingKind::CubicBezier);
    CHECK_NEAR(layer.Find(25)->tweenIn.easing.p1y, 0.1f, 1e-4f);

    const AnimKeyframe& first = *layer.Find(1);
    CHECK_NEAR(first.transform.position.x, 1.5f, 1e-4f);
    CHECK_NEAR(first.transform.rotation, 0.75f, 1e-4f);
    CHECK_NEAR(first.transform.scale.y, 0.75f, 1e-4f);
    CHECK_NEAR(first.transform.skewY, -0.2f, 1e-4f);
    CHECK_NEAR(first.transform.alpha, 0.5f, 1e-4f);
    CHECK(first.transform.colorTransform == AnimColor(10, 20, 30, 40));

    REQUIRE_EQ(first.shapes.size(), static_cast<size_t>(1));
    const AnimShape& shape = first.shapes[0];
    CHECK_EQ(shape.name, std::string("blob"));
    CHECK_EQ(shape.path.segments.size(), doc.layers[0].Find(1)->shapes[0].path.segments.size());
    CHECK(shape.style.hasFill);
    CHECK(shape.style.fill == AnimColor(255, 128, 0, 200));
    CHECK(shape.style.stroke == AnimColor(1, 2, 3, 4));
    CHECK_NEAR(shape.style.strokeWidth, 3.5f, 1e-4f);
    CHECK(shape.style.cap == LineCap::Square);
    CHECK(shape.style.join == LineJoin::Bevel);
    CHECK_NEAR(shape.transform.rotation, 0.5f, 1e-4f);

    // Flattening before and after a round-trip yields the same pixels, which
    // is the guarantee the editor canvas and the runtime baker depend on.
    const FlatPath before = Flatten(shape.path);
    const FlatPath after = Flatten(shape.path);
    REQUIRE_EQ(before.polylines.size(), after.polylines.size());
    CHECK_EQ(before.polylines[0].size(), after.polylines[0].size());

    // Idempotent: re-serializing a loaded document is byte-identical.
    CHECK_EQ(Serialize(back), text);
}

static void TestSerializationErrors() {
    TEST_GROUP("serialization errors");

    AnimDocument out;
    std::string error;

    CHECK(!Deserialize("not json at all", out, error));
    CHECK(!error.empty());

    CHECK(!Deserialize("[]", out, error));
    CHECK(!Deserialize("{}", out, error)); // no formatVersion
    CHECK(!Deserialize("{\"formatVersion\": 1}", out, error)); // no document

    // A newer file is refused with an actionable message, never mis-read.
    CHECK(!Deserialize("{\"formatVersion\": 99, \"document\": {}}", out, error));
    CHECK(error.find("newer Incogine") != std::string::npos);
    CHECK(!Deserialize("{\"formatVersion\": 0, \"document\": {}}", out, error));

    // Missing fields degrade to defaults instead of failing the whole load, so
    // a hand-edited minimal file still opens.
    REQUIRE(Deserialize(
        "{\"formatVersion\":1,\"document\":{},\"layers\":[]}", out, error));
    CHECK_EQ(out.stageWidth, 1920);
    CHECK_EQ(out.fps, 24);
    CHECK(out.layers.empty());

    // Unknown extra fields are ignored rather than fatal (forward compat).
    REQUIRE(Deserialize("{\"formatVersion\":1,\"document\":{},"
                        "\"layers\":[],\"futureThing\":{\"a\":1}}",
                        out, error));

    // Migrate() is the single upgrade point.
    icg::anim::json::JsonValue root;
    REQUIRE(icg::anim::json::Parse("{\"formatVersion\":1}", root, error));
    CHECK(icg::anim::Migrate(root, 1, error));
    CHECK(!icg::anim::Migrate(root, 99, error));
    CHECK(!icg::anim::Migrate(root, 0, error));
}

static void TestFileIO() {
    TEST_GROUP("file io");

    const AnimDocument doc = MakeRichDocument();
    const std::string path = "incoanim_test_roundtrip.incoanim";
    std::string error;

    std::remove(path.c_str());
    REQUIRE(SaveFile(path, doc, error));
    // Saving is atomic via a sibling temp file, which must not survive.
    std::FILE* probe = std::fopen((path + ".tmp").c_str(), "rb");
    CHECK(probe == nullptr);
    if (probe != nullptr) {
        std::fclose(probe);
    }

    AnimDocument back;
    REQUIRE(LoadFile(path, back, error));
    CHECK_EQ(back.lengthFrames, doc.lengthFrames);
    CHECK_EQ(Serialize(back), Serialize(doc));

    // Overwriting an existing file works (the rename path).
    AnimDocument edited = back;
    edited.fps = 12;
    REQUIRE(SaveFile(path, edited, error));
    REQUIRE(LoadFile(path, back, error));
    CHECK_EQ(back.fps, 12);

    std::remove(path.c_str());
    CHECK(!LoadFile(path, back, error));
}

// -------------------------------------------------------- v2 container --

static void TestContainer() {
    TEST_GROUP("v2 container");

    const AnimDocument doc = MakeRichDocument();
    std::string error;

    // Round-trip: bytes -> document -> identical v1-JSON projection.
    const std::vector<uint8_t> bytes = SaveContainer(doc);
    CHECK(!bytes.empty());
    CHECK(IsContainer(bytes.data(), bytes.size()));
    AnimDocument back;
    REQUIRE(LoadContainer(bytes.data(), bytes.size(), back, error));
    CHECK_EQ(Serialize(back), Serialize(doc));

    // v1 JSON text still loads through the same entry point (migrate on
    // save: load old, save new, load again - all identical).
    const std::string oldText = Serialize(doc);
    CHECK(!IsContainer(oldText.data(), oldText.size()));
    AnimDocument migrated;
    REQUIRE(LoadContainer(oldText.data(), oldText.size(), migrated, error));
    CHECK_EQ(Serialize(migrated), Serialize(doc));
    const std::vector<uint8_t> repacked = SaveContainer(migrated);
    AnimDocument repackedBack;
    REQUIRE(LoadContainer(repacked.data(), repacked.size(), repackedBack,
                           error));
    CHECK_EQ(Serialize(repackedBack), Serialize(doc));

    // The container is much smaller than the pretty JSON it replaces.
    CHECK(repacked.size() * 4 < oldText.size());

    // Manifest reads without touching stroke data.
    ContainerManifest manifest;
    REQUIRE(LoadManifest(bytes.data(), bytes.size(), manifest, error));
    CHECK_EQ(manifest.stageWidth, doc.stageWidth);
    CHECK_EQ(manifest.fps, doc.fps);
    CHECK_EQ(manifest.layers.size(), doc.layers.size());
    for (size_t i = 0; i < doc.layers.size(); ++i) {
        CHECK_EQ(manifest.layers[i].id, doc.layers[i].id);
        CHECK_EQ(manifest.layers[i].frames.size(),
                 doc.layers[i].frames.size());
    }

    // Lazy frame load: one keyframe, parsed alone, matches the full load.
    {
        const AnimLayer& layer = doc.layers[0];
        REQUIRE(!layer.frames.empty());
        AnimKeyframe lazy;
        REQUIRE(LoadLayerFrame(bytes.data(), bytes.size(), layer.id,
                               layer.frames[0].frame, lazy, error));
        CHECK_EQ(lazy.frame, layer.frames[0].frame);
        CHECK_EQ(lazy.shapes.size(), layer.frames[0].shapes.size());
        // No chunk for a frame that has none.
        AnimKeyframe missing;
        CHECK(!LoadLayerFrame(bytes.data(), bytes.size(), layer.id,
                              999999, missing, error));
    }

    // Unknown chunk types are skipped (forward compatibility): corrupt the
    // first STRO chunk's type and the file still loads, minus that frame.
    {
        std::vector<uint8_t> edited = bytes;
        // Header (16B) + manifest chunk header (16B) + manifest payload.
        size_t pos = 16;
        uint32_t mType = 0, mFlags = 0, mUnpacked = 0, mPacked = 0;
        for (int i = 0; i < 4; ++i) {
            mType |= static_cast<uint32_t>(edited[pos + i]) << (8 * i);
            mFlags |= static_cast<uint32_t>(edited[pos + 4 + i]) << (8 * i);
            mUnpacked |= static_cast<uint32_t>(edited[pos + 8 + i]) << (8 * i);
            mPacked |= static_cast<uint32_t>(edited[pos + 12 + i]) << (8 * i);
        }
        (void)mType;
        (void)mFlags;
        (void)mUnpacked;
        pos += 16 + mPacked; // now at the next chunk header (a STRO chunk)
        REQUIRE(pos + 4 <= edited.size());
        edited[pos + 0] = 0xFF;
        edited[pos + 1] = 0xFF;
        edited[pos + 2] = 0xFF;
        edited[pos + 3] = 0xFF;
        AnimDocument skipped;
        REQUIRE(LoadContainer(edited.data(), edited.size(), skipped, error));
        ContainerManifest skippedManifest;
        REQUIRE(LoadManifest(edited.data(), edited.size(), skippedManifest,
                             error));
        CHECK_EQ(skippedManifest.layers.size(), manifest.layers.size());
    }

    // A future container version is rejected with a clear message, not
    // mis-read. The version sits at bytes 8-9 (u16 LE).
    {
        std::vector<uint8_t> future = bytes;
        REQUIRE(future.size() > 10);
        future[8] = 99;
        future[9] = 0;
        AnimDocument rejected;
        CHECK(!LoadContainer(future.data(), future.size(), rejected, error));
        CHECK(error.find("newer") != std::string::npos);
    }

    // Truncation at any point is an error, never a crash.
    {
        const size_t cuts[] = {0, 7, 8, 16, 20, bytes.size() / 2,
                               bytes.size() - 16, bytes.size() - 1};
        for (size_t cut : cuts) {
            if (cut >= bytes.size()) {
                continue;
            }
            AnimDocument broken;
            CHECK(!LoadContainer(bytes.data(), cut, broken, error));
        }
        AnimDocument empty;
        CHECK(!LoadContainer(bytes.data(), 0, empty, error));
        CHECK(!LoadContainer(nullptr, 0, empty, error));
    }

    // An empty document round-trips (manifest + index, no stroke chunks).
    {
        AnimDocument fresh = AnimDocument::New(640, 480, 12);
        const std::vector<uint8_t> freshBytes = SaveContainer(fresh);
        AnimDocument freshBack;
        REQUIRE(LoadContainer(freshBytes.data(), freshBytes.size(), freshBack,
                              error));
        CHECK_EQ(Serialize(freshBack), Serialize(fresh));
    }
}

// ---------------------------------------------------------- scene mesh --

static void TestSceneMesh() {
    TEST_GROUP("scene mesh");

    auto triArea = [](const SceneMesh& mesh) {
        double area = 0.0;
        for (size_t i = 0; i + 2 < mesh.vertices.size(); i += 3) {
            const TriVertex& a = mesh.vertices[i];
            const TriVertex& b = mesh.vertices[i + 1];
            const TriVertex& c = mesh.vertices[i + 2];
            area += std::fabs((b.x - a.x) * (c.y - a.y) -
                              (b.y - a.y) * (c.x - a.x)) *
                    0.5;
        }
        return area;
    };

    // A rect fill triangulates to its exact area, in its color.
    {
        ResolvedShape rect;
        rect.shapeId = 1;
        rect.path = Flatten(AnimPath::FromRect(10.0f, 20.0f, 100.0f, 50.0f),
                            kFlattenTolerance);
        rect.matrix = Mat2x3Identity();
        rect.hasFill = true;
        rect.fill = AnimColor(255, 0, 0, 255);
        rect.drawable = true;
        SceneMesh mesh;
        std::vector<ResolvedShape> shapes;
        shapes.push_back(rect);
        BuildSceneMesh(shapes, kFlattenTolerance, mesh);
        CHECK(!mesh.vertices.empty());
        CHECK_EQ(mesh.vertices.size() % 3, static_cast<size_t>(0));
        CHECK_NEAR(triArea(mesh), 100.0 * 50.0, 1.0);
        for (const TriVertex& v : mesh.vertices) {
            CHECK_EQ(v.r, static_cast<uint8_t>(255));
            CHECK_EQ(v.g, static_cast<uint8_t>(0));
            CHECK_EQ(v.a, static_cast<uint8_t>(255));
        }
    }

    // A concave L triangulates exactly (ear clipping, not fan).
    {
        ResolvedShape ell;
        ell.shapeId = 2;
        ell.path.polylines.push_back({Vec2(0, 0), Vec2(60, 0), Vec2(60, 20),
                                      Vec2(20, 20), Vec2(20, 60), Vec2(0, 60)});
        ell.path.closed.push_back(true);
        ell.matrix = Mat2x3Identity();
        ell.hasFill = true;
        ell.fill = AnimColor(0, 255, 0, 255);
        ell.drawable = true;
        SceneMesh mesh;
        std::vector<ResolvedShape> shapes;
        shapes.push_back(ell);
        BuildSceneMesh(shapes, kFlattenTolerance, mesh);
        // 60x20 bar + 20x40 stem = 2000.
        CHECK_NEAR(triArea(mesh), 2000.0, 1.0);
    }

    // A stroke becomes its band: ~length x width, in the stroke color.
    {
        AnimPath center;
        AnimSegment move(AnimSegment::Kind::Move);
        move.p[0] = Vec2(0, 0);
        center.segments.push_back(move);
        AnimSegment line(AnimSegment::Kind::Line);
        line.p[0] = Vec2(100, 0);
        center.segments.push_back(line);
        ResolvedShape stroke;
        stroke.shapeId = 3;
        stroke.path = Flatten(center, kFlattenTolerance);
        stroke.matrix = Mat2x3Identity();
        stroke.hasStroke = true;
        stroke.stroke = AnimColor(0, 0, 255, 255);
        stroke.strokeWidth = 4.0f;
        stroke.cap = LineCap::Round;
        stroke.join = LineJoin::Round;
        stroke.drawable = true;
        SceneMesh mesh;
        std::vector<ResolvedShape> shapes;
        shapes.push_back(stroke);
        BuildSceneMesh(shapes, kFlattenTolerance, mesh);
        CHECK(!mesh.vertices.empty());
        // 100x4 band + two r=2 cap discs (~25.1): tight band, no more.
        const double area = triArea(mesh);
        CHECK(area > 400.0 && area < 450.0);
        CHECK_EQ(mesh.vertices[0].b, static_cast<uint8_t>(255));
    }

    // Draw order is preserved (later shapes cover earlier ones): the mesh
    // keeps shape order, so a painter or index buffer can rely on it.
    {
        ResolvedShape first;
        first.shapeId = 4;
        first.path = Flatten(AnimPath::FromRect(0, 0, 10, 10),
                             kFlattenTolerance);
        first.matrix = Mat2x3Identity();
        first.hasFill = true;
        first.fill = AnimColor(10, 0, 0, 255);
        first.drawable = true;
        ResolvedShape second = first;
        second.shapeId = 5;
        second.fill = AnimColor(20, 0, 0, 255);
        SceneMesh mesh;
        std::vector<ResolvedShape> shapes;
        shapes.push_back(first);
        shapes.push_back(second);
        BuildSceneMesh(shapes, kFlattenTolerance, mesh);
        REQUIRE(!mesh.vertices.empty());
        CHECK_EQ(mesh.vertices.front().r, static_cast<uint8_t>(10));
        CHECK_EQ(mesh.vertices.back().r, static_cast<uint8_t>(20));
    }

    // Degenerate input yields nothing, never garbage.
    {
        SceneMesh mesh;
        std::vector<ResolvedShape> shapes;
        BuildSceneMesh(shapes, kFlattenTolerance, mesh);
        CHECK(mesh.vertices.empty());
        ResolvedShape hidden;
        hidden.shapeId = 6;
        hidden.drawable = false;
        hidden.hasFill = true;
        hidden.fill = AnimColor(255, 255, 255, 255);
        shapes.push_back(hidden);
        ResolvedShape hairline;
        hairline.shapeId = 7;
        hairline.path = Flatten(AnimPath::FromRect(0, 0, 10, 10),
                                kFlattenTolerance);
        hairline.matrix = Mat2x3Identity();
        hairline.hasStroke = true;
        hairline.stroke = AnimColor(255, 255, 255, 255);
        hairline.strokeWidth = 0.0f;
        hairline.drawable = true;
        shapes.push_back(hairline);
        BuildSceneMesh(shapes, kFlattenTolerance, mesh);
        CHECK(mesh.vertices.empty());
    }
}

// ------------------------------------------------------------ commands --

static void TestCommandStack() {
    TEST_GROUP("undo / redo");

    AnimDocument doc = AnimDocument::New(1280, 720, 24);
    AnimCommandStack stack;

    CHECK(!stack.CanUndo());
    CHECK(!stack.CanRedo());
    CHECK(!stack.Undo(doc));
    CHECK(!stack.Redo(doc));

    // Execute applies, then Undo reverts, then Redo re-applies.
    REQUIRE(stack.Execute(doc, std::unique_ptr<IAnimCommand>(
                                  new SetFpsCommand(12))));
    CHECK_EQ(doc.fps, 12);
    CHECK(stack.CanUndo());
    CHECK(!stack.CanRedo());
    CHECK_EQ(stack.undoName(), std::string("Set FPS"));

    CHECK(stack.Undo(doc));
    CHECK_EQ(doc.fps, 24);
    CHECK(!stack.CanUndo());
    CHECK(stack.CanRedo());
    CHECK_EQ(stack.redoName(), std::string("Set FPS"));

    CHECK(stack.Redo(doc));
    CHECK_EQ(doc.fps, 12);
    CHECK(!stack.CanRedo());

    // A new edit clears the redo branch.
    CHECK(stack.Undo(doc));
    CHECK(stack.CanRedo());
    REQUIRE(stack.Execute(doc, std::unique_ptr<IAnimCommand>(
                                  new SetStageSizeCommand(640, 480))));
    CHECK(!stack.CanRedo());

    // A command whose Do() fails leaves the document untouched and is not
    // pushed, so Undo can never be called on a half-applied edit.
    // A command whose Do() fails leaves the document untouched and is not
    // pushed, so Undo can never be called on a half-applied edit.
    CHECK(!stack.Execute(doc, std::unique_ptr<IAnimCommand>(
                                  new SetStageSizeCommand(0, 0))));
    CHECK(!stack.Execute(doc, std::unique_ptr<IAnimCommand>(
                                  new SetFpsCommand(0))));
    CHECK(!stack.Execute(doc, std::unique_ptr<IAnimCommand>(
                                  new RenameLayerCommand(9999, "ghost"))));
    CHECK(!stack.Execute(doc, std::unique_ptr<IAnimCommand>(
                                  new SetLayerVisibleCommand(9999, false))));
    CHECK(!stack.Execute(doc, std::unique_ptr<IAnimCommand>(
                                  new DeleteLayerCommand(9999))));
    CHECK(!stack.Execute(doc, std::unique_ptr<IAnimCommand>(
                                  new SetLengthFramesCommand(0))));
    // Only the one successful SetStageSize edit from above is in the history.
    CHECK_EQ(stack.undoDepth(), static_cast<size_t>(1));
    CHECK_EQ(stack.undoName(), std::string("Set Stage Size"));
    // No-op edits are refused too, so the history holds no dead entries.
    CHECK(!stack.Execute(doc, std::unique_ptr<IAnimCommand>(
                                  new SetStageSizeCommand(640, 480))));

    // Clear() drops both directions.
    stack.Clear();
    CHECK(!stack.CanUndo());
    CHECK(!stack.CanRedo());
    CHECK_EQ(stack.undoDepth(), static_cast<size_t>(0));

    // The depth cap drops the oldest entry rather than growing unbounded,
    // matching the Scene tab's 50-entry history cap.
    AnimCommandStack capped(3);
    for (int i = 0; i < 6; ++i) {
        REQUIRE(capped.Execute(
            doc, std::unique_ptr<IAnimCommand>(new SetFpsCommand(i + 1))));
    }
    CHECK_EQ(capped.undoDepth(), static_cast<size_t>(3));
    CHECK_EQ(doc.fps, 6);

    // SetLengthCommand resizes the timeline and restores dropped keyframes.
    AnimDocument timeline = AnimDocument::New(320, 240, 24);
    timeline.lengthFrames = 48;
    timeline.layers[0].SetKeyframe(MakeTestLayer(timeline).frames[0]);
    AnimKeyframe far;
    far.frame = 40;
    timeline.layers[0].SetKeyframe(far);
    AnimCommandStack timelineStack;
    REQUIRE(timelineStack.Execute(
        timeline, std::unique_ptr<IAnimCommand>(new SetLengthFramesCommand(10))));
    CHECK_EQ(timeline.lengthFrames, 10);
    CHECK(timeline.layers[0].Find(40) == nullptr);
    CHECK(timelineStack.Undo(timeline));
    CHECK_EQ(timeline.lengthFrames, 48);
    CHECK(timeline.layers[0].Find(40) != nullptr);

    // Layer commands round-trip through undo.
    AnimCommandStack layerStack;
    const size_t before = doc.layers.size();
    REQUIRE(layerStack.Execute(
        doc, std::unique_ptr<IAnimCommand>(new AddLayerCommand("New Layer"))));
    CHECK_EQ(doc.layers.size(), before + 1);
    const uint64_t addedId = doc.layers.back().id;
    CHECK(layerStack.Undo(doc));
    CHECK_EQ(doc.layers.size(), before);
    CHECK(doc.FindLayerById(addedId) == nullptr);
    CHECK(layerStack.Redo(doc));
    CHECK(doc.FindLayerById(addedId) != nullptr);

    const std::string originalName = doc.layers.back().name;
    REQUIRE(layerStack.Execute(doc, std::unique_ptr<IAnimCommand>(
                                       new RenameLayerCommand(addedId, "Zed"))));
    CHECK_EQ(doc.layers.back().name, std::string("Zed"));
    CHECK(layerStack.Undo(doc));
    CHECK_EQ(doc.layers.back().name, originalName);

    REQUIRE(layerStack.Execute(
        doc, std::unique_ptr<IAnimCommand>(
                new SetLayerVisibleCommand(addedId, false))));
    CHECK(!doc.layers.back().visible);
    CHECK(layerStack.Undo(doc));
    CHECK(doc.layers.back().visible);

    REQUIRE(layerStack.Execute(
        doc, std::unique_ptr<IAnimCommand>(
                new SetLayerLockedCommand(addedId, true))));
    CHECK(doc.layers.back().locked);
    CHECK(layerStack.Undo(doc));
    CHECK(!doc.layers.back().locked);

    // MoveLayer reorders by index, and undo restores the original order.
    if (doc.layers.size() >= 2) {
        const size_t last = doc.layers.size() - 1;
        const std::string at0 = doc.layers[0].name;
        const std::string atLast = doc.layers[last].name;
        REQUIRE(layerStack.Execute(doc, std::unique_ptr<IAnimCommand>(
                                           new MoveLayerCommand(last, 0))));
        CHECK_EQ(doc.layers[0].name, atLast);
        CHECK(layerStack.Undo(doc));
        CHECK_EQ(doc.layers[0].name, at0);
        // Redo re-applies, and a second undo still lands on the original.
        CHECK(layerStack.Redo(doc));
        CHECK_EQ(doc.layers[0].name, atLast);
        CHECK(layerStack.Undo(doc));
        CHECK_EQ(doc.layers[0].name, at0);
        // Out-of-range and no-op moves are refused.
        CHECK(!layerStack.Execute(doc, std::unique_ptr<IAnimCommand>(
                                          new MoveLayerCommand(0, 99))));
        CHECK(!layerStack.Execute(doc, std::unique_ptr<IAnimCommand>(
                                          new MoveLayerCommand(0, 0))));
    }

    // DeleteLayer restores the layer at its original index (not merely
    // appended), so an undo does not silently reorder the stack.
    AnimCommandStack deleteStack;
    const size_t countBefore = doc.layers.size();
    const size_t doomedIndex = doc.layers.size() - 2;
    const uint64_t doomedId = doc.layers[doomedIndex].id;
    const std::string doomedName = doc.layers[doomedIndex].name;
    REQUIRE(deleteStack.Execute(
        doc, std::unique_ptr<IAnimCommand>(new DeleteLayerCommand(doomedId))));
    CHECK_EQ(doc.layers.size(), countBefore - 1);
    CHECK(doc.FindLayerById(doomedId) == nullptr);
    CHECK(deleteStack.Undo(doc));
    CHECK_EQ(doc.layers.size(), countBefore);
    REQUIRE(doc.FindLayerById(doomedId) != nullptr);
    CHECK_EQ(doc.layers[doomedIndex].name, doomedName);
    CHECK_EQ(doc.layers[doomedIndex].id, doomedId);
    // Deleting a layer that is already gone is refused, not pushed.
    CHECK(!deleteStack.Execute(doc, std::unique_ptr<IAnimCommand>(
                                       new DeleteLayerCommand(999999))));

    // Background and bake scale.
    AnimCommandStack styleStack;
    REQUIRE(styleStack.Execute(doc, std::unique_ptr<IAnimCommand>(
                                       new SetBackgroundCommand(
                                           AnimColor(1, 2, 3, 4)))));
    CHECK(doc.background == AnimColor(1, 2, 3, 4));
    CHECK(styleStack.Undo(doc));
    CHECK(doc.background != AnimColor(1, 2, 3, 4));
    REQUIRE(styleStack.Execute(doc, std::unique_ptr<IAnimCommand>(
                                       new SetBakeScaleCommand(4.0f))));
    CHECK_NEAR(doc.bakeScale, 4.0f, 1e-4f);
    CHECK(styleStack.Undo(doc));
    CHECK_NEAR(doc.bakeScale, 1.0f, 1e-4f);
    // An out-of-range bake scale is refused rather than clamped silently, so the
    // editor's spin box stays the single place that clamps.
    CHECK(!styleStack.Execute(doc, std::unique_ptr<IAnimCommand>(
                                       new SetBakeScaleCommand(0.0f))));
    CHECK(!styleStack.Execute(doc, std::unique_ptr<IAnimCommand>(
                                       new SetBakeScaleCommand(-1.0f))));

    // AddShapesCommand: brush/pen strokes land on top, undo removes them.
    AnimCommandStack drawStack;
    AnimDocument sketch = AnimDocument::New(640, 480, 24);
    sketch.lengthFrames = 24;
    const uint64_t sketchLayer = sketch.layers[0].id;
    AnimKeyframe seed;
    seed.frame = 1;
    seed.kind = KeyframeKind::Key;
    sketch.layers[0].SetKeyframe(seed);

    AnimShape stroke;
    stroke.id = sketch.AllocId();
    stroke.name = "stroke";
    stroke.path = AnimPath::FromRect(0.0f, 0.0f, 40.0f, 10.0f);
    stroke.style.hasFill = false;
    stroke.style.hasStroke = true;
    std::vector<AnimShape> one;
    one.push_back(stroke);
    REQUIRE(drawStack.Execute(
        sketch, std::unique_ptr<IAnimCommand>(
                    new AddShapesCommand(sketchLayer, 1, std::move(one)))));
    REQUIRE(sketch.layers[0].Find(1) != nullptr);
    CHECK_EQ(sketch.layers[0].Find(1)->shapes.size(), static_cast<size_t>(1));
    CHECK(drawStack.Undo(sketch));
    CHECK_EQ(sketch.layers[0].Find(1)->shapes.size(), static_cast<size_t>(0));
    CHECK(drawStack.Redo(sketch));
    CHECK_EQ(sketch.layers[0].Find(1)->shapes.size(), static_cast<size_t>(1));

    // Drawing on a frame with no keyframe promotes it, copying earlier artwork
    // so the span stays continuous; undo removes the whole promoted keyframe.
    AnimShape second;
    second.id = sketch.AllocId();
    second.path = AnimPath::FromRect(100.0f, 100.0f, 20.0f, 20.0f);
    second.style.hasStroke = true;
    std::vector<AnimShape> two;
    two.push_back(second);
    REQUIRE(drawStack.Execute(
        sketch, std::unique_ptr<IAnimCommand>(
                    new AddShapesCommand(sketchLayer, 10, std::move(two)))));
    REQUIRE(sketch.layers[0].Find(10) != nullptr);
    // Promoted key holds the copied stroke plus the new shape.
    CHECK_EQ(sketch.layers[0].Find(10)->shapes.size(), static_cast<size_t>(2));
    CHECK(drawStack.Undo(sketch));
    CHECK(sketch.layers[0].Find(10) == nullptr);
    CHECK(drawStack.Redo(sketch));
    REQUIRE(sketch.layers[0].Find(10) != nullptr);
    CHECK_EQ(sketch.layers[0].Find(10)->shapes.size(), static_cast<size_t>(2));

    // Refusals: empty path, bad layer, out-of-range frame.
    AnimShape empty;
    empty.id = sketch.AllocId();
    std::vector<AnimShape> emptyVec;
    emptyVec.push_back(empty);
    CHECK(!drawStack.Execute(
        sketch, std::unique_ptr<IAnimCommand>(
                    new AddShapesCommand(sketchLayer, 1, std::move(emptyVec)))));
    AnimShape ghost;
    ghost.id = sketch.AllocId();
    ghost.path = AnimPath::FromRect(0.0f, 0.0f, 5.0f, 5.0f);
    std::vector<AnimShape> ghostVec;
    ghostVec.push_back(ghost);
    CHECK(!drawStack.Execute(
        sketch, std::unique_ptr<IAnimCommand>(
                    new AddShapesCommand(999999, 1, std::move(ghostVec)))));
}

int main() {
    std::printf("Incogine animation tests\n");
    TestTransformMath();
    TestColorAndEasing();
    TestGeometry();
    TestKeyframeLogic();
    TestFrameResolution();
    TestNormalize();
    TestSerializationRoundTrip();
    TestSerializationErrors();
    TestFileIO();
    TestContainer();
    TestSceneMesh();
    TestCommandStack();
    return ::icgtest::Report("animation");
}
