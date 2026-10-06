// Incogine Animator - the animation editor window (Qt Widgets).
// Part of Incogine by leafstudiosDot (MPL-2.0). See LICENSE.
//
// Incogine Animator is a QMainWindow that edits `.incoanim` vector animations:
// a stage canvas, a document-properties dock, and (Milestone 4) a timeline.
// It is a SEPARATE top-level application, not a Studio child window, so
// closing Incogine Studio does not close an animation the user is working on.
// Studio launches it with the file path as an argument.
//
// Milestone 1 scope: window geometry memory, document lifecycle
// (new/open/save/save-as), dirty tracking with a close prompt, autosave, and
// the document-properties dock driving the command stack. The canvas is a
// placeholder until Milestone 2.
#pragma once

#include <QMainWindow>
#include <QString>

#include <memory>

class AnimatorCanvas;
class AnimatorChannel;
class AnimatorDocument;
class QAction;
class QActionGroup;
class QCheckBox;
class QDoubleSpinBox;
class QLabel;
class QLineEdit;
class QSpinBox;
class QWidget;

// The editor window.
class AnimatorWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit AnimatorWindow(QWidget* parent = nullptr);
    ~AnimatorWindow() override;

    // Opens `path` (or a fresh document when empty) and shows the window.
    void openPath(const QString& path);

    // True when there are unsaved edits. Used by --self-test.
    bool isDirty() const;
    QString documentName() const;

    // The document controller, for --self-test and for the properties dock.
    AnimatorDocument* controller() const { return document_.get(); }

protected:
    void closeEvent(QCloseEvent* event) override;

private slots:
    void onNewDocument();
    void onOpenDocument();
    void onSaveDocument();
    void onSaveDocumentAs();
    void onUndo();
    void onRedo();
    void onDocumentChanged();
    void onDirtyChanged(bool dirty);
    void onPathChanged(const QString& path);
    void onAutosaved(const QString& path);
    void onChannelOpenRequested(const QString& path);
    void onZoomChanged(double percent);
    void onCanvasStatus(const QString& text);
    void onToolTriggered();
    void onFitStage();
    void onZoomIn();
    void onZoomOut();
    void onDeleteSelection();

private:
    void buildMenus();
    void buildToolBar();
    void buildDocks();
    void buildCentral();
    // Restores geometry/window state from QSettings; false on first run.
    bool restoreGeometryFromSettings();
    void refreshWindowTitle();
    void refreshUndoRedo();
    void refreshProperties();
    // Save-changes prompt. Returns false when the user cancels.
    bool confirmDiscardChanges();
    // Pulls the property controls back from the model (after undo/redo/load).
    bool updatingControls() const { return updatingControls_; }

    std::unique_ptr<AnimatorDocument> document_;
    // Single-instance channel: another launch of this same file is routed here
    // instead of starting a second editor on the same document.
    AnimatorChannel* channel_ = nullptr;
    AnimatorCanvas* canvas_ = nullptr;
    QActionGroup* toolGroup_ = nullptr;
    QLabel* zoomLabel_ = nullptr;

    QWidget* propertiesDock_ = nullptr;
    QSpinBox* widthSpin_ = nullptr;
    QSpinBox* heightSpin_ = nullptr;
    QSpinBox* fpsSpin_ = nullptr;
    QSpinBox* lengthSpin_ = nullptr;
    QCheckBox* loopBox_ = nullptr;
    QDoubleSpinBox* bakeScaleSpin_ = nullptr;
    QLabel* stageInfo_ = nullptr;

    QAction* newAction_ = nullptr;
    QAction* openAction_ = nullptr;
    QAction* saveAction_ = nullptr;
    QAction* saveAsAction_ = nullptr;
    QAction* quitAction_ = nullptr;
    QAction* undoAction_ = nullptr;
    QAction* redoAction_ = nullptr;

    // Guards against a control's change handler re-entering refreshProperties()
    // while that same function is writing the control's value.
    bool updatingControls_ = false;
};

