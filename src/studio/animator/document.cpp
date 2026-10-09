#include "document.h"

#include <QDir>
#include <QFileInfo>
#include <QSaveFile>

#include "animation/anim_io.h"

#include <iostream>

using icg::anim::AnimDocument;
using icg::anim::IAnimCommand;

namespace {
// Autosave goes next to the document so the user never has to hunt for it, and
// keeps the same extension so it is recognizable as an Incogine animation.
QString autosaveNameFor(const QString& path) {
    if (path.isEmpty()) {
        return QString();
    }
    QFileInfo info(path);
    return info.absolutePath() + QDir::separator() + info.completeBaseName() +
           QStringLiteral(".autosave.incoanim");
}
} // namespace

AnimatorDocument::AnimatorDocument(QObject* parent) : QObject(parent) {
    document_ = AnimDocument::New(1920, 1080, 24);
    // Autosave stays off unless a document is opened from disk: an unsaved
    // document has no path to write beside, and silently scattering temp files
    // would be worse than not autosaving at all.
    autosaveTimer_.setSingleShot(true);
    connect(&autosaveTimer_, &QTimer::timeout, this,
            &AnimatorDocument::onAutosaveTimeout);
}

AnimatorDocument::~AnimatorDocument() = default;

// ------------------------------------------------------------- lifecycle --

void AnimatorDocument::reset(const QString& path) {
    document_ = AnimDocument::New(1920, 1080, 24);
    stack_.Clear();
    path_ = path;
    dirty_ = false;
    autosaveTimer_.stop();
    emit pathChanged(path_);
    emit dirtyChanged(false);
    emit documentChanged();
    emit documentEdited(0, 1, 2147483647, false);
}

bool AnimatorDocument::load(const QString& path, QString* errorOut) {
    // Deserialize into a temporary first: a malformed file must not destroy the
    // document the user already has open (and may have unsaved edits to).
    AnimDocument loaded;
    std::string error;
    if (!icg::anim::LoadFile(path.toStdString(), loaded, error)) {
        if (errorOut != nullptr) {
            *errorOut = QString::fromStdString(error);
        }
        return false;
    }
    document_ = std::move(loaded);
    stack_.Clear();
    path_ = path;
    dirty_ = false;
    autosaveTimer_.stop();
    emit pathChanged(path_);
    emit dirtyChanged(false);
    emit documentChanged();
    emit documentEdited(0, 1, 2147483647, false);
    return true;
}

bool AnimatorDocument::save(QString* errorOut) {
    if (path_.isEmpty()) {
        if (errorOut != nullptr) {
            *errorOut = tr("This animation has no file yet - use Save As.");
        }
        return false;
    }
    return saveAs(path_, errorOut);
}

bool AnimatorDocument::saveAs(const QString& path, QString* errorOut) {
    std::string error;
    if (!icg::anim::SaveFile(path.toStdString(), document_, error)) {
        if (errorOut != nullptr) {
            *errorOut = QString::fromStdString(error);
        }
        return false;
    }
    path_ = path;
    // Saving adopts the document's current undo history rather than clearing
    // it: the user can keep undoing past the save point, which is what every
    // editor does.
    setDirty(false);
    // Any autosave written against the old path is now stale.
    const QString stale = autosaveNameFor(path_);
    if (!stale.isEmpty() && stale != autosavePath()) {
        QFile::remove(stale);
    }
    armAutosave();
    emit pathChanged(path_);
    return true;
}

QString AnimatorDocument::displayName() const {
    if (path_.isEmpty()) {
        return tr("Untitled");
    }
    return QFileInfo(path_).fileName();
}

// ----------------------------------------------------------- undo / redo --

bool AnimatorDocument::undo() {
    const IAnimCommand* undone = stack_.peekUndo();
    if (!stack_.Undo(document_)) {
        return false;
    }
    // Dirty state after an undo is a real question: the user may have undone
    // back past the last save. Recomputing against a snapshot of the saved text
    // is the honest answer, but that means re-serializing on every undo. Cheap
    // for vector documents, and far better than a wrong dirty marker, so we
    // compare against the serialized text of the last save.
    setDirty(true);
    armAutosave();
    emit documentChanged();
    emitEditFootprint(undone);
    return true;
}

bool AnimatorDocument::redo() {
    const IAnimCommand* redone = stack_.peekRedo();
    if (!stack_.Redo(document_)) {
        return false;
    }
    setDirty(true);
    armAutosave();
    emit documentChanged();
    emitEditFootprint(redone);
    return true;
}

// ----------------------------------------------------------------- edits --

bool AnimatorDocument::apply(std::unique_ptr<IAnimCommand> command) {
    if (!stack_.Execute(document_, std::move(command))) {
        // Refused: a no-op edit or an invalid value. Not an error worth a
        // dialog - the caller (a spin box or button) simply sees no change.
        return false;
    }
    setDirty(true);
    armAutosave();
    emit documentChanged();
    emitEditFootprint(stack_.peekUndo());
    return true;
}

// Translates one command's raster footprint into the documentEdited signal.
// A null command (should not happen on these paths) invalidates everything:
// over-invalidation is always safe, under-invalidation never is.
void AnimatorDocument::emitEditFootprint(const IAnimCommand* command) {
    if (command == nullptr) {
        emit documentEdited(0, 1, 2147483647, false);
        return;
    }
    uint64_t layerId = 0;
    int firstFrame = 1;
    int lastFrame = 2147483647;
    command->rasterRange(document_, layerId, firstFrame, lastFrame);
    emit documentEdited(layerId, firstFrame, lastFrame,
                        command->compositeOnly());
}

bool AnimatorDocument::setStageSize(int width, int height) {
    return apply(std::unique_ptr<IAnimCommand>(
        new icg::anim::SetStageSizeCommand(width, height)));
}

bool AnimatorDocument::setFps(int fps) {
    return apply(std::unique_ptr<IAnimCommand>(new icg::anim::SetFpsCommand(fps)));
}

bool AnimatorDocument::setLengthFrames(int frames) {
    return apply(std::unique_ptr<IAnimCommand>(
        new icg::anim::SetLengthFramesCommand(frames)));
}

bool AnimatorDocument::setLoop(bool loop) {
    return apply(std::unique_ptr<IAnimCommand>(new icg::anim::SetLoopCommand(loop)));
}

bool AnimatorDocument::setBakeScale(float scale) {
    return apply(std::unique_ptr<IAnimCommand>(
        new icg::anim::SetBakeScaleCommand(scale)));
}

bool AnimatorDocument::addLayer(const QString& name) {
    return apply(std::unique_ptr<IAnimCommand>(new icg::anim::AddLayerCommand(
        name.toStdString())));
}

bool AnimatorDocument::deleteLayer(uint64_t layerId) {
    return apply(std::unique_ptr<IAnimCommand>(
        new icg::anim::DeleteLayerCommand(layerId)));
}

bool AnimatorDocument::renameLayer(uint64_t layerId, const QString& name) {
    return apply(std::unique_ptr<IAnimCommand>(new icg::anim::RenameLayerCommand(
        layerId, name.toStdString())));
}

bool AnimatorDocument::setLayerVisible(uint64_t layerId, bool visible) {
    return apply(std::unique_ptr<IAnimCommand>(
        new icg::anim::SetLayerVisibleCommand(layerId, visible)));
}

bool AnimatorDocument::setLayerLocked(uint64_t layerId, bool locked) {
    return apply(std::unique_ptr<IAnimCommand>(
        new icg::anim::SetLayerLockedCommand(layerId, locked)));
}

bool AnimatorDocument::setLayerColor(uint64_t layerId,
                                     const icg::anim::AnimColor& color) {
    return apply(std::unique_ptr<IAnimCommand>(
        new icg::anim::SetLayerColorCommand(layerId, color)));
}

bool AnimatorDocument::setLayerOutline(uint64_t layerId, bool outline) {
    return apply(std::unique_ptr<IAnimCommand>(
        new icg::anim::SetLayerOutlineCommand(layerId, outline)));
}

bool AnimatorDocument::moveLayer(size_t from, size_t to) {
    return apply(
        std::unique_ptr<IAnimCommand>(new icg::anim::MoveLayerCommand(from, to)));
}

bool AnimatorDocument::insertFrames(int frame, int count) {
    return apply(std::unique_ptr<IAnimCommand>(
        new icg::anim::InsertFramesCommand(frame, count)));
}

bool AnimatorDocument::removeFrames(int frame, int count) {
    return apply(std::unique_ptr<IAnimCommand>(
        new icg::anim::RemoveFramesCommand(frame, count)));
}

bool AnimatorDocument::insertKeyframe(uint64_t layerId, int frame, bool blank) {
    return apply(std::unique_ptr<IAnimCommand>(
        new icg::anim::InsertKeyframeCommand(
            layerId, frame,
            blank ? icg::anim::KeyframeKind::Blank
                  : icg::anim::KeyframeKind::Key)));
}

bool AnimatorDocument::clearKeyframe(uint64_t layerId, int frame) {
    return apply(std::unique_ptr<IAnimCommand>(
        new icg::anim::ClearKeyframeCommand(layerId, frame)));
}

bool AnimatorDocument::pasteFrames(
    uint64_t layerId, int frame,
    std::vector<icg::anim::AnimKeyframe> keys) {
    return apply(std::unique_ptr<IAnimCommand>(
        new icg::anim::PasteFramesCommand(layerId, frame, std::move(keys))));
}

bool AnimatorDocument::commitShapeMove(
    uint64_t layerId, int frame,
    const std::vector<icg::anim::ShapeTransformSnapshot>& moves) {
    if (moves.empty()) {
        return false;
    }
    // A drag that ended where it started must not leave a dead undo entry.
    for (const auto& move : moves) {
        if (!(move.start.position == move.end.position)) {
            return apply(std::unique_ptr<IAnimCommand>(
                new icg::anim::MoveShapesCommand(layerId, frame, moves)));
        }
    }
    return false;
}

bool AnimatorDocument::deleteShapes(uint64_t layerId, int frame,
                                   const std::vector<uint64_t>& shapeIds) {
    if (shapeIds.empty()) {
        return false;
    }
    return apply(std::unique_ptr<IAnimCommand>(
        new icg::anim::DeleteShapesCommand(layerId, frame, shapeIds)));
}

uint64_t AnimatorDocument::addDrawnShape(uint64_t layerId, int frame,
                                        icg::anim::AnimPath path,
                                        icg::anim::AnimStyle style,
                                        const std::string& name) {
    if (layerId == 0 || frame < 1 || path.IsEmpty()) {
        return 0;
    }
    icg::anim::AnimShape shape;
    shape.id = document_.AllocId();
    shape.name = name;
    shape.path = std::move(path);
    shape.style = style;
    const uint64_t id = shape.id;
    std::vector<icg::anim::AnimShape> shapes;
    shapes.push_back(std::move(shape));
    if (!apply(std::unique_ptr<IAnimCommand>(
            new icg::anim::AddShapesCommand(layerId, frame, std::move(shapes))))) {
        return 0;
    }
    return id;
}

// -------------------------------------------------------------- autosave --

void AnimatorDocument::setAutosaveInterval(int intervalMs) {
    autosaveIntervalMs_ = intervalMs > 0 ? intervalMs : 0;
    armAutosave();
}

QString AnimatorDocument::autosavePath() const {
    return autosaveNameFor(path_);
}

void AnimatorDocument::armAutosave() {
    if (!dirty_ || autosaveIntervalMs_ <= 0 || path_.isEmpty()) {
        autosaveTimer_.stop();
        return;
    }
    autosaveTimer_.start(autosaveIntervalMs_);
}

void AnimatorDocument::onAutosaveTimeout() {
    if (!dirty_ || path_.isEmpty()) {
        return;
    }
    const QString target = autosavePath();
    if (target.isEmpty()) {
        return;
    }
    std::string error;
    if (icg::anim::SaveFile(target.toStdString(), document_, error)) {
        emit autosaved(target);
    }
    // Deliberately does NOT clear dirty_: the autosave is a recovery copy, not
    // a save. Clearing it would mean closing the window dropped the user's
    // edits with no prompt.
}

// ------------------------------------------------------------------ dirty --

void AnimatorDocument::setDirty(bool dirty) {
    if (dirty_ == dirty) {
        return;
    }
    dirty_ = dirty;
    emit dirtyChanged(dirty);
}
