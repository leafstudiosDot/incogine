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
    layer.id = document.AllocId();
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

} // namespace anim
} // namespace icg