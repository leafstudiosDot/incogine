// Incogine — 2D vector animation document model.
// Part of Incogine by leafstudiosDot (MPL-2.0). See LICENSE.
//
// The authoritative, Qt-free data behind an `.incoanim` file. Studio edits it
// through AnimCommandStack (see anim_commands.h); the engine reads it to bake
// frames (see anim_raster.h / anim_sheet.h). Nothing here depends on a window,
// a renderer, or Qt, so the whole model is testable headlessly.
//
// Keyframe model (Flash/Animate-like):
//   * A layer stores keyframes ONLY. Frame spans fall out of "nearest
//     keyframe at or before N" — there is no separate span list to keep in
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

#include "anim_types.h"

namespace icg {
namespace anim {

// ----------------------------------------------------------------- paths --

// A path is a segment list. Segment kinds map 1:1 onto QPainterPath calls in
// the editor and onto the software rasterizer, so all three consume the same
// geometry.
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

    static AnimPath FromRect(float x, float y, float w, float h);
    static AnimPath FromEllipse(float cx, float cy, float rx, float ry);
};

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
    // Nearest keyframe at or before `frame` — the active keyframe for a span.
    const AnimKeyframe* AtOrBefore(int frame) const;
    // First keyframe at or after `frame` — the span's end.
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

} // namespace anim
} // namespace icg