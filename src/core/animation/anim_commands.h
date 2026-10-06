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

private:
    uint64_t layerId_;
    bool locked_;
    bool oldLocked_ = false;
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

} // namespace anim
} // namespace icg
