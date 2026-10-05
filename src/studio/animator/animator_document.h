// Incogine Animator — document controller (Qt side, thin).
// Part of Incogine by leafstudiosDot (MPL-2.0). See LICENSE.
//
// Owns the engine-side AnimDocument plus its AnimCommandStack and wraps them in
// the editor concerns the model does not know about: a file path, dirty state,
// autosave, and change notifications. Deliberately thin — every mutation goes
// through a command on the stack, so undo/redo, dirty tracking, and the
// save-changes prompt all key off the same signal and cannot drift apart.
#pragma once

#include <QObject>
#include <QString>
#include <QTimer>

#include <memory>

#include "animation/anim_commands.h"
#include "animation/anim_document.h"

class AnimatorDocument : public QObject {
    Q_OBJECT

public:
    explicit AnimatorDocument(QObject* parent = nullptr);
    ~AnimatorDocument() override;

    // --- document lifecycle ---
    // Replaces the document with a fresh one. `path` empty = unsaved.
    void reset(const QString& path = QString());
    // Loads from disk. Returns false with `errorOut` on failure, leaving the
    // previous document untouched so a failed open cannot lose work.
    bool load(const QString& path, QString* errorOut = nullptr);
    // Writes to the current path (empty -> false). Returns false + error.
    bool save(QString* errorOut = nullptr);
    // Always writes to `path` and adopts it as the current path.
    bool saveAs(const QString& path, QString* errorOut = nullptr);

    // --- state ---
    const icg::anim::AnimDocument& document() const { return document_; }
    icg::anim::AnimDocument& document() { return document_; }
    const icg::anim::AnimCommandStack& stack() const { return stack_; }

    QString path() const { return path_; }
    QString displayName() const; // file name, or "Untitled" when unsaved
    bool isDirty() const { return dirty_; }
    bool hasPath() const { return !path_.isEmpty(); }
    bool canUndo() const { return stack_.CanUndo(); }
    bool canRedo() const { return stack_.CanRedo(); }
    QString undoName() const { return QString::fromStdString(stack_.undoName()); }
    QString redoName() const { return QString::fromStdString(stack_.redoName()); }

    // --- undo / redo ---
    bool undo();
    bool redo();

    // --- edits (each goes through the command stack) ---
    bool setStageSize(int width, int height);
    bool setFps(int fps);
    bool setLengthFrames(int frames);
    bool setLoop(bool loop);
    bool setBakeScale(float scale);
    bool addLayer(const QString& name);
    bool deleteLayer(uint64_t layerId);
    bool renameLayer(uint64_t layerId, const QString& name);
    bool setLayerVisible(uint64_t layerId, bool visible);
    bool setLayerLocked(uint64_t layerId, bool locked);
    bool moveLayer(size_t from, size_t to);

    // --- autosave ---
    // Arms a timer that saves after `intervalMs` of inactivity. Editing while
    // dirty re-arms it, so a long drawing session writes periodically instead
    // of only at exit.
    void setAutosaveInterval(int intervalMs);
    int autosaveInterval() const { return autosaveIntervalMs_; }
    QString autosavePath() const;

signals:
    // Any change that alters the document, from an edit, undo/redo, or load.
    void documentChanged();
    // Dirty state flipped.
    void dirtyChanged(bool dirty);
    void pathChanged(const QString& path);
    void autosaved(const QString& path);

private slots:
    void onAutosaveTimeout();

private:
    // Central mutation path: push a command, and on success mark dirty and
    // notify. Returns whether the edit landed.
    bool apply(std::unique_ptr<icg::anim::IAnimCommand> command);
    void setDirty(bool dirty);
    void armAutosave();

    icg::anim::AnimDocument document_;
    icg::anim::AnimCommandStack stack_;
    QString path_;
    bool dirty_ = false;
    int autosaveIntervalMs_ = 0;
    QTimer autosaveTimer_;
};