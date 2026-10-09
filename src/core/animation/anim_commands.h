// Incogine - undo/redo command pattern for the 2D animation model.
// Part of Incogine by leafstudiosDot (MPL-2.0). See LICENSE.
//
// Qt-free on purpose: the command objects must be creatable and testable
// headlessly, so the model's history does not depend on the editor. Studio
// drives this stack directly from its Undo/Redo actions (no QUndoStack needed),
// which keeps one history for the whole document instead of one per view.
//
// Contract for implementors:
//   * Do() returns false and changes NOTHING when the edit cannot be applied.
//     The stack then refuses to push it, so Undo() is never called on a
//     half-applied change.
//   * Undo() restores exactly what Do() replaced, including the original index
//     or id, so undo/redo is lossless rather than approximate.
//
// Where an edit structurally rewrites keyframes (SetLengthFramesCommand) the
// command snapshots the affected state instead of computing a delta: the edit
// is rare, correctness matters more than memory, and a hand-rolled delta would
// be the more error-prone code.

#pragma once

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "anim_document.h"
#include "anim_types.h"

namespace icg {
namespace anim {

class IAnimCommand {
public:
    virtual ~IAnimCommand() = default;

    // Human-readable, shown as the Undo/Redo action text in the editor.
    virtual const char* name() const = 0;

    virtual bool Do(AnimDocument& document) = 0;
    virtual void Undo(AnimDocument& document) = 0;

    // Raster footprint of this edit AND its undo: the (layer, frame) range
    // whose rendered pixels can change. layerId 0 = all layers; first > last
    // = none (display-only edits like rename or color). The RAM cache (and
    // any future renderer) invalidates exactly this, never the whole project.
    // Computed from the LIVE document on every query (not stored), so spans
    // stay correct even when other edits land between do and undo. Default is
    // everything: overriding precisely is optional but expected for content
    // edits.
    virtual void rasterRange(const AnimDocument& document, uint64_t& layerId,
                             int& firstFrame, int& lastFrame) const {
        (void)document;
        layerId = 0;
        firstFrame = 1;
        lastFrame = 2147483647;
    }

    // True when only compositing changed (reorder, visibility): per-layer
    // pixels stay valid, only assembled frames drop. Lets a layer reorder
    // skip re-rasterizing a single shape.
    virtual bool compositeOnly() const { return false; }
};

// Bounded undo/redo history. Executing a new command clears the redo branch,
// like every editor the user already knows.
class AnimCommandStack {
public:
    static constexpr size_t kDefaultCapacity = 50;

    explicit AnimCommandStack(size_t capacity = kDefaultCapacity)
        : capacity_(capacity == 0 ? 1 : capacity) {}

    // Applies `command`. On failure nothing changes and nothing is pushed.
    bool Execute(AnimDocument& document, std::unique_ptr<IAnimCommand> command);

    bool Undo(AnimDocument& document);
    bool Redo(AnimDocument& document);

    bool CanUndo() const { return !undo_.empty(); }
    bool CanRedo() const { return !redo_.empty(); }
    // The command an Undo()/Redo() call would run (null when unavailable).
    // Lets callers query a command (e.g. its raster footprint) around the
    // stack operation without touching ownership.
    const IAnimCommand* peekUndo() const {
        return undo_.empty() ? nullptr : undo_.back().get();
    }
    const IAnimCommand* peekRedo() const {
        return redo_.empty() ? nullptr : redo_.back().get();
    }
    // Empty when unavailable.
    std::string undoName() const;
    std::string redoName() const;

    size_t undoDepth() const { return undo_.size(); }
    size_t redoDepth() const { return redo_.size(); }
    size_t capacity() const { return capacity_; }

    void Clear();

private:
    std::vector<std::unique_ptr<IAnimCommand>> undo_;
    std::vector<std::unique_ptr<IAnimCommand>> redo_;
    size_t capacity_;
};

// ---- document settings ----

class SetStageSizeCommand : public IAnimCommand {
public:
    SetStageSizeCommand(int width, int height)
        : width_(width), height_(height) {}
    const char* name() const override { return "Set Stage Size"; }
    bool Do(AnimDocument& document) override;
    void Undo(AnimDocument& document) override;
    // Cached images are stage-sized: any stage change drops them all.
    void rasterRange(const AnimDocument& document, uint64_t& layerId,
                     int& firstFrame, int& lastFrame) const override;

private:
    int width_;
    int height_;
    int oldWidth_ = 0;
    int oldHeight_ = 0;
};

class SetFpsCommand : public IAnimCommand {
public:
    explicit SetFpsCommand(int fps) : fps_(fps) {}
    const char* name() const override { return "Set FPS"; }
    bool Do(AnimDocument& document) override;
    void Undo(AnimDocument& document) override;
    // No pixels: playback rate only.
    void rasterRange(const AnimDocument& document, uint64_t& layerId,
                     int& firstFrame, int& lastFrame) const override;

private:
    int fps_;
    int oldFps_ = 0;
};

class SetLoopCommand : public IAnimCommand {
public:
    explicit SetLoopCommand(bool loop) : loop_(loop) {}
    const char* name() const override { return "Set Loop"; }
    bool Do(AnimDocument& document) override;
    void Undo(AnimDocument& document) override;
    // No pixels: playback behavior only.
    void rasterRange(const AnimDocument& document, uint64_t& layerId,
                     int& firstFrame, int& lastFrame) const override;

private:
    bool loop_;
    bool oldLoop_ = false;
};

class SetBackgroundCommand : public IAnimCommand {
public:
    explicit SetBackgroundCommand(AnimColor color) : color_(color) {}
    const char* name() const override { return "Set Background"; }
    bool Do(AnimDocument& document) override;
    void Undo(AnimDocument& document) override;
    // No cached pixels: the background composites live, never baked.
    void rasterRange(const AnimDocument& document, uint64_t& layerId,
                     int& firstFrame, int& lastFrame) const override;

private:
    AnimColor color_;
    AnimColor oldColor_;
    bool oldTransparent_ = false;
};

class SetBakeScaleCommand : public IAnimCommand {
public:
    explicit SetBakeScaleCommand(float scale) : scale_(scale) {}
    const char* name() const override { return "Set Bake Scale"; }
    bool Do(AnimDocument& document) override;
    void Undo(AnimDocument& document) override;
    // No cached pixels: export-only setting.
    void rasterRange(const AnimDocument& document, uint64_t& layerId,
                     int& firstFrame, int& lastFrame) const override;

private:
    float scale_;
    float oldScale_ = 1.0f;
};

// Resizes the timeline. Keyframes past the new end are dropped; undo restores
// them (see the file comment on why this snapshots).
class SetLengthFramesCommand : public IAnimCommand {
public:
    explicit SetLengthFramesCommand(int lengthFrames) : lengthFrames_(lengthFrames) {}
    const char* name() const override { return "Set Length"; }
    bool Do(AnimDocument& document) override;
    void Undo(AnimDocument& document) override;

private:
    int lengthFrames_;
    int oldLengthFrames_ = 0;
    // One snapshot of the whole layer list per direction.
    std::vector<AnimLayer> oldLayers_;
    std::vector<AnimLayer> newLayers_;
};

// ---- layers ----

class AddLayerCommand : public IAnimCommand {
public:
    explicit AddLayerCommand(std::string name) : name_(std::move(name)) {}
    const char* name() const override { return "Add Layer"; }
    bool Do(AnimDocument& document) override;
    void Undo(AnimDocument& document) override;
    // A new (empty) layer: nothing to rasterize, but composites covering it
    // must reassemble. Uses the post-Do id (allocated on first run).
    void rasterRange(const AnimDocument& document, uint64_t& layerId,
                     int& firstFrame, int& lastFrame) const override;

private:
    std::string name_;
    uint64_t layerId_ = 0;
    size_t index_ = 0;
};

class DeleteLayerCommand : public IAnimCommand {
public:
    explicit DeleteLayerCommand(uint64_t layerId) : layerId_(layerId) {}
    const char* name() const override { return "Delete Layer"; }
    bool Do(AnimDocument& document) override;
    void Undo(AnimDocument& document) override;
    void rasterRange(const AnimDocument& document, uint64_t& layerId,
                     int& firstFrame, int& lastFrame) const override;

private:
    uint64_t layerId_;
    AnimLayer removed_;
    size_t index_ = 0;
};

class RenameLayerCommand : public IAnimCommand {
public:
    RenameLayerCommand(uint64_t layerId, std::string name)
        : layerId_(layerId), name_(std::move(name)) {}
    const char* name() const override { return "Rename Layer"; }
    bool Do(AnimDocument& document) override;
    void Undo(AnimDocument& document) override;
    // No pixels: timeline label only.
    void rasterRange(const AnimDocument& document, uint64_t& layerId,
                     int& firstFrame, int& lastFrame) const override;

private:
    uint64_t layerId_;
    std::string name_;
    std::string oldName_;
};

class SetLayerVisibleCommand : public IAnimCommand {
public:
    SetLayerVisibleCommand(uint64_t layerId, bool visible)
        : layerId_(layerId), visible_(visible) {}
    const char* name() const override { return "Toggle Layer Visibility"; }
    bool Do(AnimDocument& document) override;
    void Undo(AnimDocument& document) override;
    // Layer pixels unchanged: only assembled frames drop.
    bool compositeOnly() const override { return true; }

private:
    uint64_t layerId_;
    bool visible_;
    bool oldVisible_ = true;
};

class SetLayerLockedCommand : public IAnimCommand {
public:
    SetLayerLockedCommand(uint64_t layerId, bool locked)
        : layerId_(layerId), locked_(locked) {}
    const char* name() const override { return "Toggle Layer Lock"; }
    bool Do(AnimDocument& document) override;
    void Undo(AnimDocument& document) override;
    // No pixels: editing guard only.
    void rasterRange(const AnimDocument& document, uint64_t& layerId,
                     int& firstFrame, int& lastFrame) const override;

private:
    uint64_t layerId_;
    bool locked_;
    bool oldLocked_ = false;
};

// Recolors a timeline row chip. Display only: never touches rendered pixels,
// so undo restores the old color exactly with no bake implications.
class SetLayerColorCommand : public IAnimCommand {
public:
    SetLayerColorCommand(uint64_t layerId, AnimColor color)
        : layerId_(layerId), color_(color) {}
    const char* name() const override { return "Set Layer Color"; }
    bool Do(AnimDocument& document) override;
    void Undo(AnimDocument& document) override;
    // No pixels: row-chip display only.
    void rasterRange(const AnimDocument& document, uint64_t& layerId,
                     int& firstFrame, int& lastFrame) const override;

private:
    uint64_t layerId_;
    AnimColor color_;
    AnimColor oldColor_;
    bool applied_ = false;
};

// Toggles a layer's outline (wireframe) mode. This DOES change rendered
// pixels (fills skipped, thin centerlines), so the canvas bake cache keys on
// it via ResolvedShape::layerOutline - toggling repaints, it never goes stale.
class SetLayerOutlineCommand : public IAnimCommand {
public:
    SetLayerOutlineCommand(uint64_t layerId, bool outline)
        : layerId_(layerId), outline_(outline) {}
    const char* name() const override { return "Toggle Layer Outline"; }
    bool Do(AnimDocument& document) override;
    void Undo(AnimDocument& document) override;
    // Whole layer, all frames: wireframe changes every one of its pixels.
    void rasterRange(const AnimDocument& document, uint64_t& layerId,
                     int& firstFrame, int& lastFrame) const override;

private:
    uint64_t layerId_;
    bool outline_;
    bool oldOutline_ = false;
    bool applied_ = false;
};

// Moves the layer at `fromIndex` to `toIndex` (0 = topmost), preserving every
// other layer's relative order.
class MoveLayerCommand : public IAnimCommand {
public:
    MoveLayerCommand(size_t fromIndex, size_t toIndex)
        : fromIndex_(fromIndex), toIndex_(toIndex) {}
    const char* name() const override { return "Reorder Layer"; }
    bool Do(AnimDocument& document) override;
    void Undo(AnimDocument& document) override;
    // Layer pixels unchanged: only assembled frames drop.
    bool compositeOnly() const override { return true; }

private:
    size_t fromIndex_;
    size_t toIndex_;
    // A move is its own inverse when the stack swaps the two indices.
    size_t fromIndexBefore_ = 0;
    size_t toIndexBefore_ = 0;
};

// ---- shapes (Cursor tool edits) ----

// Where one shape was before and after a drag. Storing the resolved transforms
// rather than a delta is deliberate: a delta is only correct while nothing else
// has touched the shape, and a redo after an unrelated edit would drift.
struct ShapeTransformSnapshot {
    uint64_t shapeId = 0;
    AnimTransform start;
    AnimTransform end;
};

// Translates shapes on one keyframe. Undo restores the recorded start
// transforms exactly.
class MoveShapesCommand : public IAnimCommand {
public:
    MoveShapesCommand(uint64_t layerId, int frame,
                      std::vector<ShapeTransformSnapshot> moves)
        : layerId_(layerId), frame_(frame), moves_(std::move(moves)) {}
    const char* name() const override { return "Move"; }
    bool Do(AnimDocument& document) override;
    void Undo(AnimDocument& document) override;
    // The edited keyframe's span, resolved live (never stored: interleaved
    // edits can move span boundaries between do and undo).
    void rasterRange(const AnimDocument& document, uint64_t& layerId,
                     int& firstFrame, int& lastFrame) const override;

private:
    // Finds the keyframe, or nullptr when the layer/frame is gone.
    static AnimKeyframe* FindKeyframe(AnimDocument& document, uint64_t layerId,
                                      int frame);
    bool Apply(AnimDocument& document, bool useEnd);

    uint64_t layerId_;
    int frame_;
    std::vector<ShapeTransformSnapshot> moves_;
};

// Removes shapes from one keyframe. The removed shapes are captured on each Do()
// (with their indices) so undo restores them at their original positions, not
// appended at the end of the list.
class DeleteShapesCommand : public IAnimCommand {
public:
    DeleteShapesCommand(uint64_t layerId, int frame,
                        std::vector<uint64_t> shapeIds)
        : layerId_(layerId), frame_(frame), shapeIds_(std::move(shapeIds)) {}
    const char* name() const override { return "Delete"; }
    bool Do(AnimDocument& document) override;
    void Undo(AnimDocument& document) override;
    // The edited keyframe's span, resolved live (see Move).
    void rasterRange(const AnimDocument& document, uint64_t& layerId,
                     int& firstFrame, int& lastFrame) const override;

private:
    static AnimKeyframe* FindKeyframe(AnimDocument& document, uint64_t layerId,
                                      int frame);
    uint64_t layerId_;
    int frame_;
    std::vector<uint64_t> shapeIds_;
    // (original index, shape) captured during Do(), highest index first so
    // erases do not invalidate the remaining ones.
    std::vector<std::pair<size_t, AnimShape>> removed_;
};

// ---- frames (timeline editing) ----

// Inserts `count` blank frames at `frame` in EVERY layer (Flash F5): keys at
// or after `frame` shift right and the timeline grows. Always succeeds for a
// valid frame (extending the timeline is itself the edit, even when no layer
// has a key to shift). Exactly invertible, so undo stores only the old
// length, never geometry.
class InsertFramesCommand : public IAnimCommand {
  public:
    InsertFramesCommand(int frame, int count = 1)
        : frame_(frame), count_(count) {}
    const char* name() const override { return "Insert Frame"; }
    bool Do(AnimDocument& document) override;
    void Undo(AnimDocument& document) override;
    // Everything at/after the insertion point, on all layers.
    void rasterRange(const AnimDocument& document, uint64_t& layerId,
                     int& firstFrame, int& lastFrame) const override;

  private:
    int frame_;
    int count_;
    int oldLength_ = 1;
};

// Removes `count` frames at `frame` in EVERY layer (Flash Shift+F5): keys at
// or after `frame` shift left and the timeline shrinks (never below 1). Lossy
// by nature - keys pushed out of range are dropped and left-shift collisions
// merge - so undo snapshots the touched layers' keyframe lists plus the old
// length. Only touched layers are snapshotted, not the whole document.
class RemoveFramesCommand : public IAnimCommand {
  public:
    RemoveFramesCommand(int frame, int count = 1)
        : frame_(frame), count_(count) {}
    const char* name() const override { return "Remove Frame"; }
    bool Do(AnimDocument& document) override;
    void Undo(AnimDocument& document) override;
    // Everything at/after the removal point, on all layers.
    void rasterRange(const AnimDocument& document, uint64_t& layerId,
                     int& firstFrame, int& lastFrame) const override;

  private:
    int frame_;
    int count_;
    int oldLength_ = 1;
    // (layerId, keyframes before the shift) for layers the shift touched.
    std::vector<std::pair<uint64_t, std::vector<AnimKeyframe>>> removed_;
};

// Inserts one keyframe (Flash F6 for a key, F7 for a blank). A Key copies the
// artwork the span currently shows (nearest keyframe at or before `frame`,
// if any); a Blank is empty. Fails when the layer is missing, the frame is
// off the timeline, or a keyframe already sits there. Re-running Do()
// (redo) rebuilds the identical key: undo removed exactly what Do added, so
// the copy source is stable across the pair.
class InsertKeyframeCommand : public IAnimCommand {
  public:
    InsertKeyframeCommand(uint64_t layerId, int frame, KeyframeKind kind)
        : layerId_(layerId), frame_(frame), kind_(kind) {}
    const char* name() const override { return "Insert Keyframe"; }
    bool Do(AnimDocument& document) override;
    void Undo(AnimDocument& document) override;
    // The new keyframe's span, resolved live (see Move).
    void rasterRange(const AnimDocument& document, uint64_t& layerId,
                     int& firstFrame, int& lastFrame) const override;

  private:
    uint64_t layerId_;
    int frame_;
    KeyframeKind kind_;
};

// Removes the keyframe at `frame`, so the span falls back to the previous
// key (or to nothing). Fails when there is none. Undo restores the removed
// keyframe exactly (order comes back via the sorted insert).
class ClearKeyframeCommand : public IAnimCommand {
  public:
    ClearKeyframeCommand(uint64_t layerId, int frame)
        : layerId_(layerId), frame_(frame) {}
    const char* name() const override { return "Clear Keyframe"; }
    bool Do(AnimDocument& document) override;
    void Undo(AnimDocument& document) override;
    // The cleared keyframe's span, resolved live (see Move).
    void rasterRange(const AnimDocument& document, uint64_t& layerId,
                     int& firstFrame, int& lastFrame) const override;

  private:
    uint64_t layerId_;
    int frame_;
    AnimKeyframe removed_;
    bool hasRemoved_ = false;
};

// Pastes copied keyframes at `targetFrame`, preserving their relative
// offsets (Flash paste-at-playhead). Keyframes landing outside the timeline
// are skipped; the rest REPLACE whatever sits at their destination frames.
// Pasted shapes get fresh ids (a paste must never duplicate an id already in
// the document), remapped once on the first Do() so redo is deterministic.
// Undo removes the pasted keys and restores every replaced one.
class PasteFramesCommand : public IAnimCommand {
  public:
    PasteFramesCommand(uint64_t layerId, int targetFrame,
                       std::vector<AnimKeyframe> clipboard)
        : layerId_(layerId),
          targetFrame_(targetFrame),
          clipboard_(std::move(clipboard)) {}
    const char* name() const override { return "Paste Frames"; }
    bool Do(AnimDocument& document) override;
    void Undo(AnimDocument& document) override;
    // The pasted range's span, resolved live (see Move).
    void rasterRange(const AnimDocument& document, uint64_t& layerId,
                     int& firstFrame, int& lastFrame) const override;

  private:
    uint64_t layerId_;
    int targetFrame_;
    std::vector<AnimKeyframe> clipboard_;
    bool remapped_ = false;
    // Destination frames this paste wrote.
    std::vector<int> inserted_;
    // (frame, keyframe) overwritten by the paste, restored on undo.
    std::vector<std::pair<int, AnimKeyframe>> replaced_;
};

// Adds shapes to one keyframe, creating the keyframe when there is none.
// When the keyframe does not exist it is created as a Key holding a copy of
// the nearest keyframe at or before `frame` (Flash behavior: drawing on a
// regular frame promotes it, keeping earlier artwork visible). Undo removes
// the added shapes, and removes the whole keyframe when this command created
// it, restoring the exact prior state.
class AddShapesCommand : public IAnimCommand {
public:
    AddShapesCommand(uint64_t layerId, int frame,
                     std::vector<AnimShape> shapes)
        : layerId_(layerId), frame_(frame), shapes_(std::move(shapes)) {}
    const char* name() const override { return "Draw"; }
    bool Do(AnimDocument& document) override;
    void Undo(AnimDocument& document) override;
    // The edited keyframe's span, resolved live (see Move).
    void rasterRange(const AnimDocument& document, uint64_t& layerId,
                     int& firstFrame, int& lastFrame) const override;

private:
    uint64_t layerId_;
    int frame_;
    std::vector<AnimShape> shapes_;
    std::vector<uint64_t> addedIds_;
    bool createdKeyframe_ = false;
};

} // namespace anim
} // namespace icg
