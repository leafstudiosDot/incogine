// Incogine - 2D vector animation document model.
// Part of Incogine by leafstudiosDot (MPL-2.0). See LICENSE.
//
// The authoritative, Qt-free data behind an `.incoanim` file. Studio edits it
// through AnimCommandStack (see anim_commands.h); the engine reads it to bake
// frames (see anim_raster.h / anim_sheet.h). Nothing here depends on a window,
// a renderer, or Qt, so the whole model is testable headlessly.
//
// Keyframe model (Flash/Animate-like):
//   * A layer stores keyframes ONLY. Frame spans fall out of "nearest
//     keyframe at or before N" - there is no separate span list to keep in
//     sync.
//   * A blank keyframe is an explicit keyframe whose `kind` is Blank: it holds
//     the timing but no artwork, so deleting an object's keyframe leaves a
//     hole rather than freezing the previous artwork.
//   * A keyframe carries a snapshot of its layer's shapes. That duplication is
//     deliberate: it makes Motion Tween (interpolate transforms between two
//     snapshots) and Shape Tween (morph snapshot[i] across two keys) fall out
//     for free later, at the cost of repeating identical shape data per
//     keyframe. A shared symbol/instance layer is the escape hatch if files
//     grow too large; not built speculatively.

#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "anim_geometry.h"
#include "anim_path.h"
#include "anim_types.h"

namespace icg {
namespace anim {

// ---------------------------------------------------------------- styles --

enum class LineCap { Butt, Round, Square };
enum class LineJoin { Miter, Round, Bevel };

struct AnimStyle {
    bool hasFill = false;
    AnimColor fill = AnimColor(255, 255, 255, 255);

    bool hasStroke = true;
    AnimColor stroke = AnimColor(0, 0, 0, 255);
    float strokeWidth = 2.0f;
    LineCap cap = LineCap::Round;
    LineJoin join = LineJoin::Round;
};

// Authoring transform, stored decomposed so the inspector can edit individual
// fields; composed to a matrix on demand. Angles in radians.
struct AnimTransform {
    Vec2 position;
    float rotation = 0.0f;
    Vec2 scale = Vec2(1.0f, 1.0f);
    float skewX = 0.0f;
    float skewY = 0.0f;
    float alpha = 1.0f;
    // Multiplied into fill and stroke colors (Flash-style color transform).
    AnimColor colorTransform = AnimColor(255, 255, 255, 255);

    Mat2x3 ToMatrix() const;
};

// One drawable vector object on a layer.
struct AnimShape {
    uint64_t id = 0;
    std::string name;
    AnimPath path;
    AnimStyle style;
    AnimTransform transform;

    // Bounds in shape-local space (the path itself, before its transform).
    void LocalBounds(Vec2& minOut, Vec2& maxOut) const;
};

// -------------------------------------------------------------- keyframes --

enum class KeyframeKind {
    Blank, // holds timing, no artwork
    Key,   // holds artwork
};

struct AnimKeyframe {
    // 1-based frame index.
    int frame = 1;
    KeyframeKind kind = KeyframeKind::Key;
    std::vector<AnimShape> shapes;
    AnimTransform transform;
    // Span from the previous keyframe on this layer to this one.
    TweenSpan tweenIn;
};

// ------------------------------------------------------------- documents --

struct AnimLayer {
    uint64_t id = 0;
    std::string name;
    bool visible = true;
    bool locked = false;
    // Sorted ascending by `frame`, no duplicates. Always sorted by mutators.
    std::vector<AnimKeyframe> frames;

    // Keyframe exactly at `frame`, or nullptr.
    const AnimKeyframe* Find(int frame) const;
    AnimKeyframe* FindMutable(int frame);
    // Nearest keyframe at or before `frame` - the active keyframe for a span.
    const AnimKeyframe* AtOrBefore(int frame) const;
    // First keyframe at or after `frame` - the span's end.
    const AnimKeyframe* AtOrAfter(int frame) const;
    const AnimKeyframe* First() const;
    const AnimKeyframe* Last() const;
    int FirstFrame() const;
    int LastFrame() const;

    // Inserts or replaces the keyframe for `key.frame`, keeping `frames`
    // sorted. Returns the stored keyframe.
    AnimKeyframe& SetKeyframe(AnimKeyframe key);
    // Removes the keyframe at `frame`. False when there is none.
    bool RemoveKeyframe(int frame);
    // Shifts keyframes at or after `frame` by `delta`, dropping ones pushed
    // past `lengthFrames`. Used by insert/remove frame. False when the layer
    // has no keyframe at or after `frame` (nothing to shift).
    bool ShiftFrames(int frame, int delta, int lengthFrames);
    // Copies keyframes in [from, to] by `delta`. False when the range is empty.
    bool CopyFrames(int from, int to, int delta, int lengthFrames);

    // Keyframe indices the timeline should draw a span between: every adjacent
    // pair (previous, next).
    size_t KeyframeCount() const { return frames.size(); }
};

// One layer's contribution to a resolved frame.
struct ResolvedLayer {
    uint32_t layerIndex = 0;
    uint64_t layerId = 0;
    // Keyframe providing the artwork (nearest at or before the frame).
    const AnimKeyframe* keyframe = nullptr;
    // Keyframe the span ends at, or nullptr when this is the last keyframe on
    // the layer (the span holds to the end of the timeline).
    const AnimKeyframe* spanEnd = nullptr;
    // Progress 0..1 across the span; 0 when there is no span.
    float spanT = 0.0f;
    // Convenience copy of the span that produced `spanT`.
    TweenSpan span;
};

// Everything needed to rasterize one frame, in back-to-front layer order.
struct AnimFrame {
    int frame = 1;
    std::vector<ResolvedLayer> layers;

    bool IsEmpty() const { return layers.empty(); }
};

struct AnimDocument {
    // --- stage ---
    int stageWidth = 1920;
    int stageHeight = 1080;
    int fps = 24;
    AnimColor background = AnimColor(0, 0, 0, 0);
    bool transparentBackground = true;

    // --- timeline ---
    int lengthFrames = 1;
    bool loop = true;

    // --- baking ---
    // Default rasterization scale for the runtime sprite sheet. Kept in the
    // file so a document can be re-baked at a different resolution; the vector
    // data is always retained.
    float bakeScale = 1.0f;

    // Index 0 is the topmost layer (drawn last, like Flash's layer stack).
    std::vector<AnimLayer> layers;

    // Next id to hand out. Always greater than every id in the document, so
    // loading a document can never collide with a later AddShape.
    uint64_t nextId = 1;

    // --- document-level queries ---
    int FrameCount() const { return lengthFrames < 1 ? 1 : lengthFrames; }
    AnimLayer* FindLayerById(uint64_t id);
    const AnimLayer* FindLayerById(uint64_t id) const;
    uint64_t AllocId();

    // Clamps `frame` into [1, FrameCount()], wrapping when `loop`.
    int ClampFrame(int frame) const;

    // Flattens visible layers into a draw list, back to front. Hidden layers
    // are skipped; the caller filters locked layers for editing only.
    AnimFrame ResolveFrame(int frame) const;

    // Document extents across all keyframes and layers (not just the stage).
    void ContentBounds(Vec2& minOut, Vec2& maxOut) const;

    // A fresh document with one empty layer named "Layer 1".
    static AnimDocument New(int stageWidth, int stageHeight, int fps);

    // Normalizes after edits: clamps stage/fps/length into sane ranges, re-sorts
    // keyframes, drops out-of-range keyframes, and advances nextId past every
    // id in use. Cheap; call after any bulk operation.
    void Normalize();
};

// -------------------------------------------------------------- drawing --

// Resolved drawing state for one shape on one keyframe.
//
// The editor canvas and the runtime rasterizer BOTH call these, so a preview
// cannot drift from the baked sprite sheet. Nothing here is view-specific.

struct ResolvedShape {
    uint64_t shapeId = 0;
    std::string name;
    FlatPath path; // in shape-local space, before the transform
    // Keyframe transform composed with the shape transform.
    Mat2x3 matrix;
    bool hasFill = false;
    AnimColor fill;
    bool hasStroke = false;
    AnimColor stroke;
    // Stroke width in stage units (the authored width scaled by the transform).
    float strokeWidth = 0.0f;
    LineCap cap = LineCap::Round;
    LineJoin join = LineJoin::Round;
    // False when the path is empty (nothing to draw or pick).
    bool drawable = false;
};

// Multiplies two straight-alpha colors, channel-wise including alpha
// (Flash-style color transform).
AnimColor MultiplyColors(const AnimColor& a, const AnimColor& b);

// Scales a color's alpha by `factor` (0..1), leaving RGB untouched.
AnimColor ScaleAlpha(const AnimColor& color, float factor);

// Combined keyframe-then-shape transform. Order matters: the shape transform
// applies in shape-local space, then the keyframe transform on top, so
// `key * shape` puts a shape-local offset under the layer's offset.
Mat2x3 ResolveShapeMatrix(const AnimShape& shape, const AnimKeyframe& key);

// Fill color: style color x shape color transform x key color transform,
// alpha scaled by both transform alphas.
AnimColor ResolveFillColor(const AnimShape& shape, const AnimKeyframe& key);

// Stroke color, same composition as the fill.
AnimColor ResolveStrokeColor(const AnimShape& shape, const AnimKeyframe& key);

// Full resolved drawing state, ready for the canvas or the rasterizer.
// `tolerance` is the Bezier flattening tolerance in shape-local units.
ResolvedShape ResolveShape(const AnimShape& shape, const AnimKeyframe& key,
                           float tolerance = kFlattenTolerance);

} // namespace anim
} // namespace icg
