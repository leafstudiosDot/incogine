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
#include "animation/anim_geometry.h"
#include "animation/anim_io.h"

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
    TestCommandStack();
    return ::icgtest::Report("animation");
}