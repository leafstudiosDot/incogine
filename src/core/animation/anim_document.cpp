#include "anim_document.h"

#include <algorithm>
#include <cmath>

#include "anim_geometry.h"

namespace icg {
namespace anim {

// ----------------------------------------------------------------- paths --

AnimPath AnimPath::FromRect(float x, float y, float w, float h) {
    AnimPath path;
    path.segments.reserve(5);

    AnimSegment move(AnimSegment::Kind::Move);
    move.p[0] = Vec2(x, y);
    path.segments.push_back(move);

    float pts[4][2] = {{x + w, y}, {x + w, y + h}, {x, y + h}};
    for (auto& pt : pts) {
        AnimSegment line(AnimSegment::Kind::Line);
        line.p[0] = Vec2(pt[0], pt[1]);
        path.segments.push_back(line);
    }

    path.segments.push_back(AnimSegment(AnimSegment::Kind::Close));
    return path;
}

AnimPath AnimPath::FromEllipse(float cx, float cy, float rx, float ry) {
    // Magic constant for approximating a quarter circle with a cubic Bezier.
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

// -------------------------------------------------------------- transform --

Mat2x3 AnimTransform::ToMatrix() const {
    // Order matters: scale/skew first, then rotation, then translation.
    Mat2x3 m = Mat2x3Scale(scale.x, scale.y);
    if (skewX != 0.0f || skewY != 0.0f) {
        m = m * Mat2x3Skew(skewX, skewY);
    }
    if (rotation != 0.0f) {
        m = m * Mat2x3Rotate(rotation);
    }
    if (position.x != 0.0f || position.y != 0.0f) {
        m = m * Mat2x3Translate(position.x, position.y);
    }
    return m;
}

void AnimShape::LocalBounds(Vec2& minOut, Vec2& maxOut) const {
    PathBounds(path, minOut, maxOut);
}

// ----------------------------------------------------------------- layer --

const AnimKeyframe* AnimLayer::Find(int frame) const {
    for (const AnimKeyframe& key : frames) {
        if (key.frame == frame) {
            return &key;
        }
    }
    return nullptr;
}

AnimKeyframe* AnimLayer::FindMutable(int frame) {
    for (AnimKeyframe& key : frames) {
        if (key.frame == frame) {
            return &key;
        }
    }
    return nullptr;
}

const AnimKeyframe* AnimLayer::AtOrBefore(int frame) const {
    const AnimKeyframe* found = nullptr;
    for (const AnimKeyframe& key : frames) {
        if (key.frame <= frame) {
            found = &key;
        } else {
            break; // frames is sorted ascending
        }
    }
    return found;
}

const AnimKeyframe* AnimLayer::AtOrAfter(int frame) const {
    for (const AnimKeyframe& key : frames) {
        if (key.frame >= frame) {
            return &key;
        }
    }
    return nullptr;
}

const AnimKeyframe* AnimLayer::First() const {
    return frames.empty() ? nullptr : &frames.front();
}

const AnimKeyframe* AnimLayer::Last() const {
    return frames.empty() ? nullptr : &frames.back();
}

int AnimLayer::FirstFrame() const {
    const AnimKeyframe* key = First();
    return key ? key->frame : 1;
}

int AnimLayer::LastFrame() const {
    const AnimKeyframe* key = Last();
    return key ? key->frame : 1;
}

AnimKeyframe& AnimLayer::SetKeyframe(AnimKeyframe key) {
    for (AnimKeyframe& existing : frames) {
        if (existing.frame == key.frame) {
            existing = std::move(key);
            return existing;
        }
    }
    frames.push_back(std::move(key));
    std::stable_sort(frames.begin(), frames.end(),
                     [](const AnimKeyframe& a, const AnimKeyframe& b) {
                         return a.frame < b.frame;
                     });
    return frames.back();
}

bool AnimLayer::RemoveKeyframe(int frame) {
    for (size_t i = 0; i < frames.size(); ++i) {
        if (frames[i].frame == frame) {
            frames.erase(frames.begin() + static_cast<long>(i));
            return true;
        }
    }
    return false;
}

bool AnimLayer::ShiftFrames(int frame, int delta, int lengthFrames) {
    bool touched = false;
    for (AnimKeyframe& key : frames) {
        if (key.frame < frame) {
            continue;
        }
        const int shifted = key.frame + delta;
        key.frame = shifted;
        touched = true;
    }
    if (!touched) {
        return false;
    }
    // Drop anything pushed off either end, then re-sort and de-duplicate
    // (a left shift can collide two keyframes onto one frame).
    frames.erase(std::remove_if(frames.begin(), frames.end(),
                                [lengthFrames](const AnimKeyframe& key) {
                                    return key.frame < 1 ||
                                           key.frame > lengthFrames;
                                }),
                 frames.end());
    std::stable_sort(frames.begin(), frames.end(),
                     [](const AnimKeyframe& a, const AnimKeyframe& b) {
                         return a.frame < b.frame;
                     });
    frames.erase(std::unique(frames.begin(), frames.end(),
                             [](const AnimKeyframe& a, const AnimKeyframe& b) {
                                 return a.frame == b.frame;
                             }),
                 frames.end());
    return true;
}

bool AnimLayer::CopyFrames(int from, int to, int delta, int lengthFrames) {
    std::vector<AnimKeyframe> copies;
    for (const AnimKeyframe& key : frames) {
        if (key.frame < from || key.frame > to) {
            continue;
        }
        AnimKeyframe copy = key;
        copy.frame = key.frame + delta;
        if (copy.frame < 1 || copy.frame > lengthFrames) {
            continue;
        }
        copies.push_back(std::move(copy));
    }
    if (copies.empty()) {
        return false;
    }
    for (AnimKeyframe& copy : copies) {
        SetKeyframe(std::move(copy));
    }
    return true;
}

// -------------------------------------------------------------- document --

AnimDocument AnimDocument::New(int width, int height, int frameRate) {
    AnimDocument document;
    document.stageWidth = width > 0 ? width : 1920;
    document.stageHeight = height > 0 ? height : 1080;
    document.fps = frameRate > 0 ? frameRate : 24;
    document.lengthFrames = 1;
    document.loop = true;
    document.bakeScale = 1.0f;
    document.background = AnimColor(255, 255, 255, 255);
    document.transparentBackground = false;

    AnimLayer layer;
    layer.id = document.AllocId();
    layer.name = "Layer 1";
    document.layers.push_back(layer);
    return document;
}

AnimLayer* AnimDocument::FindLayerById(uint64_t id) {
    for (AnimLayer& layer : layers) {
        if (layer.id == id) {
            return &layer;
        }
    }
    return nullptr;
}

const AnimLayer* AnimDocument::FindLayerById(uint64_t id) const {
    for (const AnimLayer& layer : layers) {
        if (layer.id == id) {
            return &layer;
        }
    }
    return nullptr;
}

uint64_t AnimDocument::AllocId() {
    const uint64_t id = nextId;
    ++nextId;
    return id;
}

int AnimDocument::ClampFrame(int frame) const {
    const int count = FrameCount();
    if (loop) {
        int wrapped = (frame - 1) % count;
        if (wrapped < 0) {
            wrapped += count;
        }
        return wrapped + 1;
    }
    if (frame < 1) {
        return 1;
    }
    if (frame > count) {
        return count;
    }
    return frame;
}

AnimFrame AnimDocument::ResolveFrame(int frame) const {
    AnimFrame resolved;
    resolved.frame = ClampFrame(frame);
    resolved.layers.reserve(layers.size());

    // Index 0 is the topmost layer and must draw last, so walk back to front.
    for (size_t i = layers.size(); i-- > 0;) {
        const AnimLayer& layer = layers[i];
        if (!layer.visible || layer.frames.empty()) {
            continue;
        }
        const AnimKeyframe* start = layer.AtOrBefore(resolved.frame);
        if (start == nullptr) {
            // The frame is before this layer's first keyframe: nothing to draw.
            continue;
        }

        ResolvedLayer entry;
        entry.layerIndex = static_cast<uint32_t>(i);
        entry.layerId = layer.id;
        entry.keyframe = start;
        entry.span = start->tweenIn;

        // The span runs from `start` to the next keyframe, and belongs to the
        // END keyframe — so look for the successor of whatever actually
        // provided the artwork (which is `start` itself for the common case of
        // a layer whose first key is before this frame).
        const AnimKeyframe* end = layer.AtOrAfter(start->frame + 1);
        entry.spanEnd = end;
        if (end != nullptr && end->frame > start->frame) {
            const float span = static_cast<float>(end->frame - start->frame);
            const float into = static_cast<float>(resolved.frame - start->frame);
            entry.spanT = span > 0.0f ? std::min(1.0f, std::max(0.0f, into / span)) : 0.0f;
        }
        resolved.layers.push_back(entry);
    }
    return resolved;
}

void AnimDocument::ContentBounds(Vec2& minOut, Vec2& maxOut) const {
    bool any = false;
    for (const AnimLayer& layer : layers) {
        for (const AnimKeyframe& key : layer.frames) {
            const Mat2x3 layerMatrix = key.transform.ToMatrix();
            for (const AnimShape& shape : key.shapes) {
                Vec2 lo, hi;
                shape.LocalBounds(lo, hi);
                if (hi.x < lo.x || hi.y < lo.y) {
                    continue; // empty path
                }
                const Mat2x3 matrix = layerMatrix * shape.transform.ToMatrix();
                const Vec2 corners[4] = {
                    TransformPoint(matrix, lo),
                    TransformPoint(matrix, Vec2(hi.x, lo.y)),
                    TransformPoint(matrix, hi),
                    TransformPoint(matrix, Vec2(lo.x, hi.y)),
                };
                for (const Vec2& corner : corners) {
                    if (!any) {
                        minOut = corner;
                        maxOut = corner;
                        any = true;
                        continue;
                    }
                    minOut.x = std::min(minOut.x, corner.x);
                    minOut.y = std::min(minOut.y, corner.y);
                    maxOut.x = std::max(maxOut.x, corner.x);
                    maxOut.y = std::max(maxOut.y, corner.y);
                }
            }
        }
    }
    if (!any) {
        minOut = Vec2();
        maxOut = Vec2();
    }
}

void AnimDocument::Normalize() {
    if (stageWidth < 1) {
        stageWidth = 1;
    }
    if (stageHeight < 1) {
        stageHeight = 1;
    }
    if (fps < 1) {
        fps = 1;
    }
    if (fps > 240) {
        fps = 240;
    }
    if (lengthFrames < 1) {
        lengthFrames = 1;
    }
    if (bakeScale < 0.01f) {
        bakeScale = 0.01f;
    }
    if (bakeScale > 64.0f) {
        bakeScale = 64.0f;
    }

    for (AnimLayer& layer : layers) {
        std::stable_sort(layer.frames.begin(), layer.frames.end(),
                         [](const AnimKeyframe& a, const AnimKeyframe& b) {
                             return a.frame < b.frame;
                         });
        // Keyframes past the shortened timeline are unreachable, so drop them.
        layer.frames.erase(
            std::remove_if(layer.frames.begin(), layer.frames.end(),
                           [this](const AnimKeyframe& key) {
                               return key.frame < 1 || key.frame > lengthFrames;
                           }),
            layer.frames.end());
        // nextId must stay ahead of every id we hold, or a later AllocId()
        // could collide with artwork loaded from disk.
        nextId = std::max(nextId, layer.id + 1);
        for (const AnimKeyframe& key : layer.frames) {
            for (const AnimShape& shape : key.shapes) {
                nextId = std::max(nextId, shape.id + 1);
            }
        }
    }
    if (nextId < 1) {
        nextId = 1;
    }
}

} // namespace anim
} // namespace icg