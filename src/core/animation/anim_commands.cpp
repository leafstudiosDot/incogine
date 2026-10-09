#include "anim_commands.h"

#include <algorithm>

namespace icg {
namespace anim {

// ---------------------------------------------------------------- stack --

bool AnimCommandStack::Execute(AnimDocument& document,
                               std::unique_ptr<IAnimCommand> command) {
    if (!command) {
        return false;
    }
    // Do() may refuse; a refused command must not enter the history, otherwise
    // Undo() would try to revert something that never happened.
    if (!command->Do(document)) {
        return false;
    }
    // A fresh edit invalidates the redo branch.
    redo_.clear();
    undo_.push_back(std::move(command));
    // Oldest entries fall off the bottom once the cap is reached.
    while (undo_.size() > capacity_) {
        undo_.erase(undo_.begin());
    }
    return true;
}

bool AnimCommandStack::Undo(AnimDocument& document) {
    if (undo_.empty()) {
        return false;
    }
    std::unique_ptr<IAnimCommand> command = std::move(undo_.back());
    undo_.pop_back();
    command->Undo(document);
    redo_.push_back(std::move(command));
    return true;
}

bool AnimCommandStack::Redo(AnimDocument& document) {
    if (redo_.empty()) {
        return false;
    }
    std::unique_ptr<IAnimCommand> command = std::move(redo_.back());
    redo_.pop_back();
    // A redo that can no longer apply (the document drifted underneath it)
    // is dropped rather than retried, so the history never desynchronizes.
    if (!command->Do(document)) {
        return false;
    }
    undo_.push_back(std::move(command));
    return true;
}

std::string AnimCommandStack::undoName() const {
    return undo_.empty() ? std::string() : std::string(undo_.back()->name());
}

std::string AnimCommandStack::redoName() const {
    return redo_.empty() ? std::string() : std::string(redo_.back()->name());
}

void AnimCommandStack::Clear() {
    undo_.clear();
    redo_.clear();
}

// ------------------------------------------------------ document settings --

bool SetStageSizeCommand::Do(AnimDocument& document) {
    if (width_ < 1 || height_ < 1) {
        return false;
    }
    if (width_ == document.stageWidth && height_ == document.stageHeight) {
        return false; // no-op: keep the history free of dead entries
    }
    oldWidth_ = document.stageWidth;
    oldHeight_ = document.stageHeight;
    document.stageWidth = width_;
    document.stageHeight = height_;
    return true;
}

void SetStageSizeCommand::Undo(AnimDocument& document) {
    document.stageWidth = oldWidth_;
    document.stageHeight = oldHeight_;
}

bool SetFpsCommand::Do(AnimDocument& document) {
    if (fps_ < 1 || fps_ > 240) {
        return false;
    }
    if (fps_ == document.fps) {
        return false;
    }
    oldFps_ = document.fps;
    document.fps = fps_;
    return true;
}

void SetFpsCommand::Undo(AnimDocument& document) {
    document.fps = oldFps_;
}

bool SetLoopCommand::Do(AnimDocument& document) {
    if (loop_ == document.loop) {
        return false;
    }
    oldLoop_ = document.loop;
    document.loop = loop_;
    return true;
}

void SetLoopCommand::Undo(AnimDocument& document) {
    document.loop = oldLoop_;
}

bool SetBackgroundCommand::Do(AnimDocument& document) {
    if (color_ == document.background) {
        return false;
    }
    oldColor_ = document.background;
    oldTransparent_ = document.transparentBackground;
    document.background = color_;
    // Alpha 0 is the sensible reading of "transparent background", so the flag
    // follows the color instead of becoming a second source of truth.
    document.transparentBackground = color_.a == 0;
    return true;
}

void SetBackgroundCommand::Undo(AnimDocument& document) {
    document.background = oldColor_;
    document.transparentBackground = oldTransparent_;
}

bool SetBakeScaleCommand::Do(AnimDocument& document) {
    // Refuse rather than clamp: the editor's spin box is the single place that
    // clamps, so a bad value here means the caller is wrong.
    if (!(scale_ > 0.0f) || scale_ > 64.0f) {
        return false;
    }
    if (scale_ == document.bakeScale) {
        return false;
    }
    oldScale_ = document.bakeScale;
    document.bakeScale = scale_;
    return true;
}

void SetBakeScaleCommand::Undo(AnimDocument& document) {
    document.bakeScale = oldScale_;
}

bool SetLengthFramesCommand::Do(AnimDocument& document) {
    if (lengthFrames_ < 1 || lengthFrames_ > 1000000) {
        return false;
    }
    if (lengthFrames_ == document.lengthFrames) {
        return false;
    }
    oldLengthFrames_ = document.lengthFrames;
    oldLayers_ = document.layers;

    document.lengthFrames = lengthFrames_;
    for (AnimLayer& layer : document.layers) {
        // Drop keyframes the new end makes unreachable; undo restores them.
        layer.frames.erase(
            std::remove_if(layer.frames.begin(), layer.frames.end(),
                           [this](const AnimKeyframe& key) {
                               return key.frame < 1 ||
                                      key.frame > lengthFrames_;
                           }),
            layer.frames.end());
    }
    newLayers_ = document.layers;
    return true;
}

void SetLengthFramesCommand::Undo(AnimDocument& document) {
    document.lengthFrames = oldLengthFrames_;
    document.layers = oldLayers_;
}

// ---------------------------------------------------------------- layers --

bool AddLayerCommand::Do(AnimDocument& document) {
    AnimLayer layer;
    // Allocate once: redo must restore the SAME id undo removed, not mint a
    // fresh one. Stale ids break selection and canvas caches, which key on
    // them (id 0 is never valid - AllocId starts at 1 - so it marks "fresh").
    // Redo can only run on the post-undo document (any new edit clears the
    // redo branch), where this id is guaranteed free.
    if (layerId_ == 0) {
        layerId_ = document.AllocId();
    }
    layer.id = layerId_;
    layer.name = name_;
    layer.visible = true;
    layer.locked = false;

    // New layers go on top, matching Flash.
    document.layers.insert(document.layers.begin(), layer);
    layerId_ = layer.id;
    index_ = 0;
    return true;
}

void AddLayerCommand::Undo(AnimDocument& document) {
    AnimLayer* layer = document.FindLayerById(layerId_);
    if (layer == nullptr) {
        return;
    }
    const auto position = std::find_if(
        document.layers.begin(), document.layers.end(),
        [layerId = layerId_](const AnimLayer& candidate) {
            return candidate.id == layerId;
        });
    if (position != document.layers.end()) {
        document.layers.erase(position);
    }
}

bool DeleteLayerCommand::Do(AnimDocument& document) {
    for (size_t i = 0; i < document.layers.size(); ++i) {
        if (document.layers[i].id == layerId_) {
            removed_ = document.layers[i];
            index_ = i;
            document.layers.erase(document.layers.begin() +
                                  static_cast<long>(i));
            return true;
        }
    }
    return false; // layer is gone: refuse rather than push a dead undo
}

void DeleteLayerCommand::Undo(AnimDocument& document) {
    const size_t at = std::min(index_, document.layers.size());
    document.layers.insert(document.layers.begin() + static_cast<long>(at),
                           removed_);
}

bool RenameLayerCommand::Do(AnimDocument& document) {
    AnimLayer* layer = document.FindLayerById(layerId_);
    if (layer == nullptr || layer->name == name_) {
        return false;
    }
    oldName_ = layer->name;
    layer->name = name_;
    return true;
}

void RenameLayerCommand::Undo(AnimDocument& document) {
    AnimLayer* layer = document.FindLayerById(layerId_);
    if (layer != nullptr) {
        layer->name = oldName_;
    }
}

bool SetLayerVisibleCommand::Do(AnimDocument& document) {
    AnimLayer* layer = document.FindLayerById(layerId_);
    if (layer == nullptr || layer->visible == visible_) {
        return false;
    }
    oldVisible_ = layer->visible;
    layer->visible = visible_;
    return true;
}

void SetLayerVisibleCommand::Undo(AnimDocument& document) {
    AnimLayer* layer = document.FindLayerById(layerId_);
    if (layer != nullptr) {
        layer->visible = oldVisible_;
    }
}

bool SetLayerLockedCommand::Do(AnimDocument& document) {
    AnimLayer* layer = document.FindLayerById(layerId_);
    if (layer == nullptr || layer->locked == locked_) {
        return false;
    }
    oldLocked_ = layer->locked;
    layer->locked = locked_;
    return true;
}

void SetLayerLockedCommand::Undo(AnimDocument& document) {
    AnimLayer* layer = document.FindLayerById(layerId_);
    if (layer != nullptr) {
        layer->locked = oldLocked_;
    }
}

bool SetLayerColorCommand::Do(AnimDocument& document) {
    AnimLayer* layer = document.FindLayerById(layerId_);
    if (layer == nullptr || layer->color == color_) {
        return false;
    }
    oldColor_ = layer->color;
    layer->color = color_;
    applied_ = true;
    return true;
}

void SetLayerColorCommand::Undo(AnimDocument& document) {
    if (!applied_) {
        return;
    }
    if (AnimLayer* layer = document.FindLayerById(layerId_)) {
        layer->color = oldColor_;
    }
    applied_ = false;
}

bool SetLayerOutlineCommand::Do(AnimDocument& document) {
    AnimLayer* layer = document.FindLayerById(layerId_);
    if (layer == nullptr || layer->outline == outline_) {
        return false;
    }
    oldOutline_ = layer->outline;
    layer->outline = outline_;
    applied_ = true;
    return true;
}

void SetLayerOutlineCommand::Undo(AnimDocument& document) {
    if (!applied_) {
        return;
    }
    if (AnimLayer* layer = document.FindLayerById(layerId_)) {
        layer->outline = oldOutline_;
    }
    applied_ = false;
}

bool MoveLayerCommand::Do(AnimDocument& document) {
    if (fromIndex_ >= document.layers.size() ||
        toIndex_ >= document.layers.size()) {
        return false;
    }
    if (fromIndex_ == toIndex_) {
        return false;
    }
    // Remember where this particular attempt started, so a redo after an
    // unrelated reorder still swaps the right pair.
    fromIndexBefore_ = fromIndex_;
    toIndexBefore_ = toIndex_;

    AnimLayer moved = document.layers[fromIndex_];
    document.layers.erase(document.layers.begin() +
                          static_cast<long>(fromIndex_));
    document.layers.insert(document.layers.begin() +
                               static_cast<long>(toIndex_),
                           moved);
    return true;
}

void MoveLayerCommand::Undo(AnimDocument& document) {
    if (fromIndexBefore_ >= document.layers.size() ||
        toIndexBefore_ >= document.layers.size()) {
        return;
    }
    AnimLayer moved = document.layers[toIndexBefore_];
    document.layers.erase(document.layers.begin() +
                          static_cast<long>(toIndexBefore_));
    document.layers.insert(document.layers.begin() +
                               static_cast<long>(fromIndexBefore_),
                           moved);
}

// ---------------------------------------------------------------- shapes --

AnimKeyframe* MoveShapesCommand::FindKeyframe(AnimDocument& document,
                                              uint64_t layerId, int frame) {
    AnimLayer* layer = document.FindLayerById(layerId);
    return layer != nullptr ? layer->FindMutable(frame) : nullptr;
}

bool MoveShapesCommand::Apply(AnimDocument& document, bool useEnd) {
    AnimKeyframe* key = FindKeyframe(document, layerId_, frame_);
    if (key == nullptr || moves_.empty()) {
        return false;
    }
    for (const ShapeTransformSnapshot& move : moves_) {
        for (AnimShape& shape : key->shapes) {
            if (shape.id != move.shapeId) {
                continue;
            }
            shape.transform = useEnd ? move.end : move.start;
            break;
        }
    }
    return true;
}

bool MoveShapesCommand::Do(AnimDocument& document) {
    return Apply(document, true);
}

void MoveShapesCommand::Undo(AnimDocument& document) {
    Apply(document, false);
}

AnimKeyframe* DeleteShapesCommand::FindKeyframe(AnimDocument& document,
                                                uint64_t layerId, int frame) {
    AnimLayer* layer = document.FindLayerById(layerId);
    return layer != nullptr ? layer->FindMutable(frame) : nullptr;
}

bool DeleteShapesCommand::Do(AnimDocument& document) {
    AnimKeyframe* key = FindKeyframe(document, layerId_, frame_);
    if (key == nullptr || shapeIds_.empty()) {
        return false;
    }
    removed_.clear();
    for (uint64_t shapeId : shapeIds_) {
        for (size_t i = 0; i < key->shapes.size(); ++i) {
            if (key->shapes[i].id == shapeId) {
                removed_.emplace_back(i, key->shapes[i]);
                key->shapes.erase(key->shapes.begin() + static_cast<long>(i));
                break;
            }
        }
    }
    if (removed_.empty()) {
        return false; // nothing matched (already deleted): refuse the edit
    }
    // Restore in ascending index order so earlier insertions do not shift the
    // positions of the later ones.
    std::sort(removed_.begin(), removed_.end(),
              [](const std::pair<size_t, AnimShape>& a,
                 const std::pair<size_t, AnimShape>& b) {
                  return a.first < b.first;
              });
    return true;
}

void DeleteShapesCommand::Undo(AnimDocument& document) {
    AnimKeyframe* key = FindKeyframe(document, layerId_, frame_);
    if (key == nullptr) {
        return;
    }
    for (const auto& entry : removed_) {
        const size_t at = std::min(entry.first, key->shapes.size());
        key->shapes.insert(key->shapes.begin() + static_cast<long>(at),
                           entry.second);
    }
}

bool AddShapesCommand::Do(AnimDocument& document) {
    AnimLayer* layer = document.FindLayerById(layerId_);
    if (layer == nullptr || shapes_.empty()) {
        return false;
    }
    if (frame_ < 1 || frame_ > document.FrameCount()) {
        return false;
    }
    for (const AnimShape& shape : shapes_) {
        if (shape.path.IsEmpty()) {
            return false; // refuse rather than push an invisible shape
        }
    }

    AnimKeyframe* key = layer->FindMutable(frame_);
    createdKeyframe_ = false;
    if (key == nullptr) {
        // Promote this frame: copy the nearest earlier keyframe so existing
        // artwork stays visible, then append the new shapes on top.
        AnimKeyframe fresh;
        fresh.frame = frame_;
        fresh.kind = KeyframeKind::Key;
        const AnimKeyframe* source = layer->AtOrBefore(frame_);
        if (source != nullptr) {
            fresh.shapes = source->shapes;
            fresh.transform = source->transform;
        }
        layer->SetKeyframe(std::move(fresh));
        key = layer->FindMutable(frame_);
        if (key == nullptr) {
            return false;
        }
        createdKeyframe_ = true;
    }

    addedIds_.clear();
    for (AnimShape& shape : shapes_) {
        if (shape.id == 0) {
            shape.id = document.AllocId();
        }
        addedIds_.push_back(shape.id);
        key->shapes.push_back(shape);
    }
    return true;
}

void AddShapesCommand::Undo(AnimDocument& document) {
    AnimLayer* layer = document.FindLayerById(layerId_);
    if (layer == nullptr) {
        return;
    }
    if (createdKeyframe_) {
        // Whole keyframe is ours: removing it restores the exact prior state
        // (the copied shapes live on in the earlier keyframe).
        layer->RemoveKeyframe(frame_);
        return;
    }
    AnimKeyframe* key = layer->FindMutable(frame_);
    if (key == nullptr) {
        return;
    }
    for (uint64_t id : addedIds_) {
        for (size_t i = 0; i < key->shapes.size(); ++i) {
            if (key->shapes[i].id == id) {
                key->shapes.erase(key->shapes.begin() + static_cast<long>(i));
                break;
            }
        }
    }
}

// ---------------------------------------------------------------- frames --

bool InsertFramesCommand::Do(AnimDocument& document) {
    if (frame_ < 1 || count_ < 1) {
        return false;
    }
    const int frame = std::min(frame_, document.FrameCount() + 1);
    oldLength_ = document.FrameCount();
    const int newLength = oldLength_ + count_;
    for (AnimLayer& layer : document.layers) {
        layer.ShiftFrames(frame, count_, newLength);
    }
    document.lengthFrames = newLength;
    document.Normalize();
    frame_ = frame; // remember the clamped frame so Undo mirrors this Do
    return true;
}

void InsertFramesCommand::Undo(AnimDocument& document) {
    for (AnimLayer& layer : document.layers) {
        layer.ShiftFrames(frame_, -count_, oldLength_);
    }
    document.lengthFrames = oldLength_;
    document.Normalize();
}

bool RemoveFramesCommand::Do(AnimDocument& document) {
    if (frame_ < 1 || count_ < 1 || frame_ > document.FrameCount()) {
        return false;
    }
    oldLength_ = document.FrameCount();
    const int newLength = std::max(1, oldLength_ - count_);
    removed_.clear();
    for (AnimLayer& layer : document.layers) {
        bool touched = false;
        for (const AnimKeyframe& key : layer.frames) {
            if (key.frame >= frame_) {
                touched = true;
                break;
            }
        }
        if (!touched) {
            continue;
        }
        removed_.emplace_back(layer.id, layer.frames);
        // Content in the removed range is deleted (Flash semantics); only the
        // tail shifts left. Deleting first keeps the shift bijective, so no
        // merge can ever eat a keyframe silently.
        for (int f = frame_; f < frame_ + count_; ++f) {
            layer.RemoveKeyframe(f);
        }
        layer.ShiftFrames(frame_ + count_, -count_, newLength);
    }
    document.lengthFrames = newLength;
    document.Normalize();
    return true;
}

void RemoveFramesCommand::Undo(AnimDocument& document) {
    for (auto& entry : removed_) {
        if (AnimLayer* layer = document.FindLayerById(entry.first)) {
            layer->frames = std::move(entry.second);
        }
    }
    removed_.clear();
    document.lengthFrames = oldLength_;
    document.Normalize();
}

bool InsertKeyframeCommand::Do(AnimDocument& document) {
    AnimLayer* layer = document.FindLayerById(layerId_);
    if (layer == nullptr || frame_ < 1 || frame_ > document.FrameCount()) {
        return false;
    }
    if (layer->Find(frame_) != nullptr) {
        return false; // never silently replace: paste owns replacement
    }
    AnimKeyframe key;
    key.frame = frame_;
    key.kind = kind_;
    if (kind_ == KeyframeKind::Key) {
        // F6 copies what the span currently shows.
        if (const AnimKeyframe* source = layer->AtOrBefore(frame_)) {
            key.shapes = source->shapes;
            key.transform = source->transform;
        }
    }
    layer->SetKeyframe(std::move(key));
    document.Normalize();
    return true;
}

void InsertKeyframeCommand::Undo(AnimDocument& document) {
    if (AnimLayer* layer = document.FindLayerById(layerId_)) {
        layer->RemoveKeyframe(frame_);
    }
    document.Normalize();
}

bool ClearKeyframeCommand::Do(AnimDocument& document) {
    AnimLayer* layer = document.FindLayerById(layerId_);
    if (layer == nullptr) {
        return false;
    }
    const AnimKeyframe* key = layer->Find(frame_);
    if (key == nullptr) {
        return false;
    }
    removed_ = *key;
    hasRemoved_ = true;
    layer->RemoveKeyframe(frame_);
    document.Normalize();
    return true;
}

void ClearKeyframeCommand::Undo(AnimDocument& document) {
    if (!hasRemoved_) {
        return;
    }
    if (AnimLayer* layer = document.FindLayerById(layerId_)) {
        layer->SetKeyframe(std::move(removed_));
        hasRemoved_ = false;
    }
    document.Normalize();
}

bool PasteFramesCommand::Do(AnimDocument& document) {
    AnimLayer* layer = document.FindLayerById(layerId_);
    if (layer == nullptr || clipboard_.empty()) {
        return false;
    }
    if (!remapped_) {
        // Fresh ids once: pasted shapes must never duplicate an id already in
        // the document (selection and the canvas caches key on ids).
        for (AnimKeyframe& key : clipboard_) {
            for (AnimShape& shape : key.shapes) {
                shape.id = document.AllocId();
            }
        }
        std::stable_sort(clipboard_.begin(), clipboard_.end(),
                         [](const AnimKeyframe& a, const AnimKeyframe& b) {
                             return a.frame < b.frame;
                         });
        remapped_ = true;
    }
    const int offset = targetFrame_ - clipboard_.front().frame;
    inserted_.clear();
    replaced_.clear();
    bool wrote = false;
    for (const AnimKeyframe& key : clipboard_) {
        const int dest = key.frame + offset;
        if (dest < 1 || dest > document.FrameCount()) {
            continue;
        }
        if (const AnimKeyframe* existing = layer->Find(dest)) {
            replaced_.emplace_back(dest, *existing);
        }
        AnimKeyframe staged = key;
        staged.frame = dest;
        layer->SetKeyframe(std::move(staged));
        inserted_.push_back(dest);
        wrote = true;
    }
    if (!wrote) {
        return false;
    }
    document.Normalize();
    return true;
}

void PasteFramesCommand::Undo(AnimDocument& document) {
    AnimLayer* layer = document.FindLayerById(layerId_);
    if (layer == nullptr) {
        return;
    }
    for (int frame : inserted_) {
        layer->RemoveKeyframe(frame);
    }
    inserted_.clear();
    for (auto& entry : replaced_) {
        layer->SetKeyframe(std::move(entry.second));
    }
    replaced_.clear();
    document.Normalize();
}

// ------------------------------------------------- raster footprints --

namespace {

// Display-only edits touch no pixels: empty range (first > last).
void EmptyRange(uint64_t& layerId, int& firstFrame, int& lastFrame) {
    layerId = 0;
    firstFrame = 1;
    lastFrame = 0;
}

// Whole document (structural changes: stage size, length, full-layer ops).
void AllFrames(uint64_t& layerId, int& firstFrame, int& lastFrame) {
    layerId = 0;
    firstFrame = 1;
    lastFrame = 2147483647;
}

// Last frame the keyframe at `frame` can affect: up to (not including) the
// next keyframe on the layer, else the timeline end. Resolved live from the
// passed document so spans stay correct even when other edits land between
// do and undo (a stored span end would go stale both ways).
int SpanEndFor(const AnimDocument& document, uint64_t layerId, int frame) {
    int end = document.FrameCount();
    if (const AnimLayer* layer = document.FindLayerById(layerId)) {
        for (const AnimKeyframe& key : layer->frames) {
            if (key.frame > frame && key.frame - 1 < end) {
                end = key.frame - 1;
            }
        }
    }
    return end;
}

} // namespace

void SetStageSizeCommand::rasterRange(const AnimDocument& document,
                                     uint64_t& layerId, int& firstFrame,
                                     int& lastFrame) const {
    (void)document;
    AllFrames(layerId, firstFrame, lastFrame);
}

void SetFpsCommand::rasterRange(const AnimDocument& document,
                               uint64_t& layerId, int& firstFrame,
                               int& lastFrame) const {
    (void)document;
    EmptyRange(layerId, firstFrame, lastFrame);
}

void SetLoopCommand::rasterRange(const AnimDocument& document,
                                 uint64_t& layerId, int& firstFrame,
                                 int& lastFrame) const {
    (void)document;
    EmptyRange(layerId, firstFrame, lastFrame);
}

void SetBackgroundCommand::rasterRange(const AnimDocument& document,
                                       uint64_t& layerId, int& firstFrame,
                                       int& lastFrame) const {
    (void)document;
    EmptyRange(layerId, firstFrame, lastFrame);
}

void SetBakeScaleCommand::rasterRange(const AnimDocument& document,
                                      uint64_t& layerId, int& firstFrame,
                                      int& lastFrame) const {
    (void)document;
    EmptyRange(layerId, firstFrame, lastFrame);
}

void RenameLayerCommand::rasterRange(const AnimDocument& document,
                                     uint64_t& layerId, int& firstFrame,
                                     int& lastFrame) const {
    (void)document;
    EmptyRange(layerId, firstFrame, lastFrame);
}

void SetLayerColorCommand::rasterRange(const AnimDocument& document,
                                      uint64_t& layerId, int& firstFrame,
                                      int& lastFrame) const {
    (void)document;
    EmptyRange(layerId, firstFrame, lastFrame);
}

void SetLayerLockedCommand::rasterRange(const AnimDocument& document,
                                       uint64_t& layerId, int& firstFrame,
                                       int& lastFrame) const {
    (void)document;
    EmptyRange(layerId, firstFrame, lastFrame);
}

void SetLayerOutlineCommand::rasterRange(const AnimDocument& document,
                                        uint64_t& layerId, int& firstFrame,
                                        int& lastFrame) const {
    (void)document;
    layerId = layerId_;
    firstFrame = 1;
    lastFrame = 2147483647;
}

void AddLayerCommand::rasterRange(const AnimDocument& document,
                                  uint64_t& layerId, int& firstFrame,
                                  int& lastFrame) const {
    (void)document;
    layerId = layerId_;
    firstFrame = 1;
    lastFrame = 2147483647;
}

void DeleteLayerCommand::rasterRange(const AnimDocument& document,
                                     uint64_t& layerId, int& firstFrame,
                                     int& lastFrame) const {
    (void)document;
    layerId = layerId_;
    firstFrame = 1;
    lastFrame = 2147483647;
}

void MoveShapesCommand::rasterRange(const AnimDocument& document,
                                    uint64_t& layerId, int& firstFrame,
                                    int& lastFrame) const {
    layerId = layerId_;
    firstFrame = frame_;
    lastFrame = SpanEndFor(document, layerId_, frame_);
}

void DeleteShapesCommand::rasterRange(const AnimDocument& document,
                                      uint64_t& layerId, int& firstFrame,
                                      int& lastFrame) const {
    layerId = layerId_;
    firstFrame = frame_;
    lastFrame = SpanEndFor(document, layerId_, frame_);
}

void AddShapesCommand::rasterRange(const AnimDocument& document,
                                   uint64_t& layerId, int& firstFrame,
                                   int& lastFrame) const {
    layerId = layerId_;
    firstFrame = frame_;
    lastFrame = SpanEndFor(document, layerId_, frame_);
}

void InsertFramesCommand::rasterRange(const AnimDocument& document,
                                      uint64_t& layerId, int& firstFrame,
                                      int& lastFrame) const {
    (void)document;
    layerId = 0;
    firstFrame = frame_;
    lastFrame = 2147483647;
}

void RemoveFramesCommand::rasterRange(const AnimDocument& document,
                                      uint64_t& layerId, int& firstFrame,
                                      int& lastFrame) const {
    (void)document;
    layerId = 0;
    firstFrame = frame_;
    lastFrame = 2147483647;
}

void InsertKeyframeCommand::rasterRange(const AnimDocument& document,
                                       uint64_t& layerId, int& firstFrame,
                                       int& lastFrame) const {
    layerId = layerId_;
    firstFrame = frame_;
    lastFrame = SpanEndFor(document, layerId_, frame_);
}

void ClearKeyframeCommand::rasterRange(const AnimDocument& document,
                                      uint64_t& layerId, int& firstFrame,
                                      int& lastFrame) const {
    layerId = layerId_;
    firstFrame = frame_;
    lastFrame = SpanEndFor(document, layerId_, frame_);
}

void PasteFramesCommand::rasterRange(const AnimDocument& document,
                                     uint64_t& layerId, int& firstFrame,
                                     int& lastFrame) const {
    layerId = layerId_;
    if (inserted_.empty()) {
        AllFrames(layerId, firstFrame, lastFrame);
        return;
    }
    int lo = inserted_.front();
    int hi = inserted_.front();
    for (int frame : inserted_) {
        lo = std::min(lo, frame);
        hi = std::max(hi, frame);
    }
    firstFrame = lo;
    lastFrame = SpanEndFor(document, layerId_, hi);
}

} // namespace anim
} // namespace icg