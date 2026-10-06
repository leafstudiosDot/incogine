#include "window.h"

#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QCheckBox>
#include <QCloseEvent>
#include <QDir>
#include <QDockWidget>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenuBar>
#include <QMessageBox>
#include <QSettings>
#include <QSpinBox>
#include <QStandardPaths>
#include <QStatusBar>
#include <QToolBar>
#include <QVBoxLayout>

#include "canvas.h"
#include "channel.h"
#include "document.h"
#include "widgets/options_bar.h"

using icg::anim::AnimDocument;

namespace {
constexpr int kDefaultWindowWidth = 1400;
constexpr int kDefaultWindowHeight = 900;

// Autosave cadence. Long enough not to thrash the disk while drawing, short
// enough that a crash loses little work.
constexpr int kAutosaveIntervalMs = 60000;

// Free function in an anonymous namespace, so there is no QObject to translate
// through --use literal strings here.
QString stageSummary(const AnimDocument& document) {
    const icg::anim::AnimLayer* top =
        document.layers.empty() ? nullptr : &document.layers.front();
    return QStringLiteral("%1 x %2  -  %3 fps  -  %4 frames  -  %5 layer(s)%6")
        .arg(document.stageWidth)
        .arg(document.stageHeight)
        .arg(document.fps)
        .arg(document.lengthFrames)
        .arg(document.layers.size())
        .arg(top != nullptr && !top->name.empty()
                 ? QStringLiteral("  -  top: %1")
                       .arg(QString::fromStdString(top->name))
                 : QString());
}
} // namespace

// ------------------------------------------------------------- the window --

AnimatorWindow::AnimatorWindow(QWidget* parent) : QMainWindow(parent) {
    document_ = std::unique_ptr<AnimatorDocument>(new AnimatorDocument());
    document_->setAutosaveInterval(kAutosaveIntervalMs);

    setWindowTitle(tr("Incogine Animator"));
    if (!restoreGeometryFromSettings()) {
        resize(kDefaultWindowWidth, kDefaultWindowHeight);
    }
    buildCentral();
    buildDocks();
    buildToolBar();
    buildOptionsBar();
    buildMenus();

    connect(document_.get(), &AnimatorDocument::documentChanged, this,
            &AnimatorWindow::onDocumentChanged);
    connect(document_.get(), &AnimatorDocument::dirtyChanged, this,
            &AnimatorWindow::onDirtyChanged);
    connect(document_.get(), &AnimatorDocument::pathChanged, this,
            &AnimatorWindow::onPathChanged);
    connect(document_.get(), &AnimatorDocument::autosaved, this,
            &AnimatorWindow::onAutosaved);

    // Announce this window so a later launch of the same file comes back here
    // instead of starting a second editor on the same document. Re-bound
    // whenever the document path changes (New / Open / Save As).
    channel_ = new AnimatorChannel(this);
    connect(channel_, &AnimatorChannel::openPathRequested, this,
            &AnimatorWindow::onChannelOpenRequested);
    connect(document_.get(), &AnimatorDocument::pathChanged, channel_,
            [this](const QString& path) { channel_->listen(path); });
    channel_->listen(QString());

    refreshProperties();
    refreshUndoRedo();
    refreshWindowTitle();
    statusBar()->showMessage(tr("Ready"));
}

AnimatorWindow::~AnimatorWindow() = default;

void AnimatorWindow::buildCentral() {
    // Real pan/zoom canvas (Milestone 2). Paints the stage outline and the
    // artwork, and routes input through the active ITool.
    canvas_ = new AnimatorCanvas(document_.get(), this);
    setCentralWidget(canvas_);
    connect(canvas_, &AnimatorCanvas::zoomChanged, this,
            &AnimatorWindow::onZoomChanged);
    connect(canvas_, &AnimatorCanvas::statusMessage, this,
            &AnimatorWindow::onCanvasStatus);
}

void AnimatorWindow::buildToolBar() {
    QToolBar* tools = addToolBar(tr("Tools"));
    tools->setObjectName(QStringLiteral("animatorToolBar"));
    // Reclaim the space the toolbar would otherwise steal from the canvas.
    tools->setMovable(false);

    // One checkable action per ITool, driven straight from the tool set. Adding
    // a tool later (Pen, Rect, ...) only adds a line in ToolSet's constructor.
    toolGroup_ = new QActionGroup(this);
    toolGroup_->setExclusive(true);

    QSettings settings;
    const QString savedTool =
        settings.value(QStringLiteral("animator/tool")).toString();

    const AnimatorCanvas* canvas = canvas_;
    QHash<QString, QAction*> byId;
    if (canvas != nullptr && canvas->toolSet() != nullptr) {
        for (const auto& tool : canvas->toolSet()->all()) {
            const QString id = QString::fromStdString(tool->id());
            auto* action = new QAction(tool->label(), this);
            action->setCheckable(true);
            const QString hint = tool->shortcutHint();
            action->setToolTip(hint.isEmpty() ? tool->label()
                                              : tr("%1  (%2)").arg(tool->label(), hint));
            action->setData(id);
            // Per-tool shortcut (V/H/B/P). Falls back to the id's first letter
            // so a future tool without an explicit key still gets one.
            const QString key = tool->keyShortcut();
            if (!key.isEmpty()) {
                action->setShortcut(QKeySequence(key));
            } else if (!id.isEmpty()) {
                action->setShortcut(QKeySequence(id.at(0).toUpper()));
            }
            connect(action, &QAction::triggered, this,
                    &AnimatorWindow::onToolTriggered);
            toolGroup_->addAction(action);
            tools->addAction(action);
            byId.insert(id, action);
        }
    }

    // Restore the previously active tool, falling back to the first one.
    QAction* restore =
        byId.value(savedTool, toolGroup_->actions().isEmpty()
                                  ? nullptr
                                  : toolGroup_->actions().first());
    if (restore != nullptr) {
        restore->setChecked(true);
        onToolTriggered();
    }

    tools->addSeparator();

    auto addButton = [this, tools](const QString& text, const QString& tip,
                                   void (AnimatorWindow::*slot)()) {
        auto* action = new QAction(text, this);
        action->setToolTip(tip);
        connect(action, &QAction::triggered, this, slot);
        tools->addAction(action);
        return action;
    };
    addButton(tr("Fit"), tr("Fit the stage in the view  (Ctrl+0)"),
              &AnimatorWindow::onFitStage);
    addButton(tr("Zoom +"), tr("Zoom in  (Ctrl+=)"), &AnimatorWindow::onZoomIn);
    addButton(tr("Zoom -"), tr("Zoom out  (Ctrl+-)"), &AnimatorWindow::onZoomOut);
    tools->addSeparator();
    addButton(tr("Delete"), tr("Delete the selection  (Del)"),
              &AnimatorWindow::onDeleteSelection);

    // Live zoom readout, always visible like Studio's scene canvas does.
    zoomLabel_ = new QLabel(this);
    zoomLabel_->setMinimumWidth(64);
    zoomLabel_->setAlignment(Qt::AlignCenter);
    statusBar()->addPermanentWidget(zoomLabel_);
    onZoomChanged(100.0);
}

void AnimatorWindow::buildOptionsBar() {
    optionsBar_ = new OptionsBar(canvas_, this);
    addToolBar(optionsBar_);
    refreshOptionsVisibility();
}

void AnimatorWindow::refreshOptionsVisibility() {
    if (optionsBar_ == nullptr || canvas_ == nullptr ||
        canvas_->activeTool() == nullptr) {
        return;
    }
    const std::string id = canvas_->activeTool()->id();
    const bool drawing = id == "brush" || id == "pen";
    optionsBar_->setVisible(drawing);
}

void AnimatorWindow::buildDocks() {
    propertiesDock_ = new QWidget(this);
    propertiesDock_->setObjectName(QStringLiteral("animatorProperties"));

    QVBoxLayout* layout = new QVBoxLayout(propertiesDock_);
    layout->setContentsMargins(8, 8, 8, 8);

    // Captures the dock too: each label is parented to it so the dock owns the
    // whole row widget tree when it is torn down.
    auto addRow = [layout, this](const QString& label, QWidget* control) {
        QHBoxLayout* row = new QHBoxLayout();
        row->addWidget(new QLabel(label, propertiesDock_));
        row->addStretch(1);
        row->addWidget(control);
        layout->addLayout(row);
    };

    widthSpin_ = new QSpinBox(propertiesDock_);
    widthSpin_->setRange(1, 16384);
    widthSpin_->setSuffix(tr(" px"));
    heightSpin_ = new QSpinBox(propertiesDock_);
    heightSpin_->setRange(1, 16384);
    heightSpin_->setSuffix(tr(" px"));
    fpsSpin_ = new QSpinBox(propertiesDock_);
    fpsSpin_->setRange(1, 240);
    lengthSpin_ = new QSpinBox(propertiesDock_);
    lengthSpin_->setRange(1, 1000000);
    lengthSpin_->setSuffix(tr(" frames"));
    loopBox_ = new QCheckBox(tr("Loop playback"), propertiesDock_);
    bakeScaleSpin_ = new QDoubleSpinBox(propertiesDock_);
    bakeScaleSpin_->setRange(0.01, 64.0);
    bakeScaleSpin_->setSingleStep(0.25);
    bakeScaleSpin_->setDecimals(2);

    addRow(tr("Stage width"), widthSpin_);
    addRow(tr("Stage height"), heightSpin_);
    addRow(tr("Frame rate"), fpsSpin_);
    addRow(tr("Length"), lengthSpin_);
    addRow(tr("Bake scale"), bakeScaleSpin_);
    layout->addWidget(loopBox_);
    layout->addStretch(1);

    stageInfo_ = new QLabel(propertiesDock_);
    stageInfo_->setWordWrap(true);
    layout->addWidget(stageInfo_);

    // Every property control routes through the command stack, so undo/redo,
    // dirty tracking, and autosave all apply without special cases. Spin boxes
    // commit on editingFinished rather than valueChanged, so typing does not
    // push a command per keystroke.
    connect(widthSpin_, &QSpinBox::editingFinished, this, [this] {
        if (!updatingControls_) {
            document_->setStageSize(widthSpin_->value(), heightSpin_->value());
        }
    });
    connect(heightSpin_, &QSpinBox::editingFinished, this, [this] {
        if (!updatingControls_) {
            document_->setStageSize(widthSpin_->value(), heightSpin_->value());
        }
    });
    connect(fpsSpin_, &QSpinBox::editingFinished, this, [this] {
        if (!updatingControls_) {
            document_->setFps(fpsSpin_->value());
        }
    });
    connect(lengthSpin_, &QSpinBox::editingFinished, this, [this] {
        if (!updatingControls_) {
            document_->setLengthFrames(lengthSpin_->value());
        }
    });
    connect(loopBox_, &QCheckBox::toggled, this, [this](bool on) {
        if (!updatingControls_) {
            document_->setLoop(on);
        }
    });
    connect(bakeScaleSpin_, &QDoubleSpinBox::editingFinished, this, [this] {
        if (!updatingControls_) {
            document_->setBakeScale(static_cast<float>(bakeScaleSpin_->value()));
        }
    });

    // Docked right: the document properties sit beside the stage, like every
    // art tool's properties panel.
    auto* dock = new QDockWidget(tr("Document"), this);
    dock->setObjectName(QStringLiteral("animatorDocumentDock"));
    dock->setWidget(propertiesDock_);
    addDockWidget(Qt::RightDockWidgetArea, dock);
}

void AnimatorWindow::buildMenus() {
    newAction_ = new QAction(tr("&New Animation"), this);
    newAction_->setShortcut(QKeySequence::New);
    connect(newAction_, &QAction::triggered, this, &AnimatorWindow::onNewDocument);

    openAction_ = new QAction(tr("&Open..."), this);
    openAction_->setShortcut(QKeySequence::Open);
    connect(openAction_, &QAction::triggered, this, &AnimatorWindow::onOpenDocument);

    saveAction_ = new QAction(tr("&Save"), this);
    saveAction_->setShortcut(QKeySequence::Save);
    connect(saveAction_, &QAction::triggered, this, &AnimatorWindow::onSaveDocument);

    saveAsAction_ = new QAction(tr("Save &As..."), this);
    saveAsAction_->setShortcut(QKeySequence::SaveAs);
    connect(saveAsAction_, &QAction::triggered, this,
            &AnimatorWindow::onSaveDocumentAs);

    quitAction_ = new QAction(tr("&Close"), this);
    quitAction_->setShortcut(QKeySequence::Close);
    connect(quitAction_, &QAction::triggered, this, &QWidget::close);

    undoAction_ = new QAction(tr("&Undo"), this);
    undoAction_->setShortcut(QKeySequence::Undo);
    connect(undoAction_, &QAction::triggered, this, &AnimatorWindow::onUndo);

    redoAction_ = new QAction(tr("&Redo"), this);
    redoAction_->setShortcut(QKeySequence::Redo);
    connect(redoAction_, &QAction::triggered, this, &AnimatorWindow::onRedo);

    auto* fitAction = new QAction(tr("&Fit Stage in View"), this);
    fitAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_0));
    connect(fitAction, &QAction::triggered, this, &AnimatorWindow::onFitStage);

    auto* zoomInAction = new QAction(tr("Zoom &In"), this);
    zoomInAction->setShortcut(QKeySequence::ZoomIn);
    connect(zoomInAction, &QAction::triggered, this, &AnimatorWindow::onZoomIn);

    auto* zoomOutAction = new QAction(tr("Zoom &Out"), this);
    zoomOutAction->setShortcut(QKeySequence::ZoomOut);
    connect(zoomOutAction, &QAction::triggered, this, &AnimatorWindow::onZoomOut);

    auto* deleteAction = new QAction(tr("De&lete Selection"), this);
    deleteAction->setShortcut(QKeySequence::Delete);
    connect(deleteAction, &QAction::triggered, this,
            &AnimatorWindow::onDeleteSelection);

    QMenu* fileMenu = menuBar()->addMenu(tr("&File"));
    fileMenu->addAction(newAction_);
    fileMenu->addAction(openAction_);
    fileMenu->addSeparator();
    fileMenu->addAction(saveAction_);
    fileMenu->addAction(saveAsAction_);
    fileMenu->addSeparator();
    fileMenu->addAction(quitAction_);

    QMenu* editMenu = menuBar()->addMenu(tr("&Edit"));
    editMenu->addAction(undoAction_);
    editMenu->addAction(redoAction_);
    editMenu->addSeparator();
    editMenu->addAction(deleteAction);

    QMenu* viewMenu = menuBar()->addMenu(tr("&View"));
    viewMenu->addAction(fitAction);
    viewMenu->addAction(zoomInAction);
    viewMenu->addAction(zoomOutAction);
    viewMenu->addSeparator();
    // Tool shortcuts are already on the toolbar actions, but repeating them here
    // keeps them discoverable before the toolbar is noticed.
    if (toolGroup_ != nullptr) {
        for (QAction* action : toolGroup_->actions()) {
            viewMenu->addAction(action);
        }
    }

    saveAction_->setEnabled(document_->hasPath());
}

bool AnimatorWindow::restoreGeometryFromSettings() {
    const QSettings settings;
    if (!settings.contains(QStringLiteral("animator/geometry"))) {
        return false;
    }
    restoreGeometry(
        settings.value(QStringLiteral("animator/geometry")).toByteArray());
    const QByteArray state =
        settings.value(QStringLiteral("animator/windowState")).toByteArray();
    if (!state.isEmpty()) {
        restoreState(state);
    }
    return true;
}

// ------------------------------------------------------------- documents --

void AnimatorWindow::openPath(const QString& path) {
    if (path.isEmpty()) {
        if (!confirmDiscardChanges()) {
            return;
        }
        document_->reset();
        return;
    }
    if (!confirmDiscardChanges()) {
        return;
    }
    QString error;
    if (!document_->load(path, &error)) {
        QMessageBox::critical(this, tr("Incogine Animator"),
                              tr("Could not open the animation:\n%1").arg(error));
        return;
    }
    statusBar()->showMessage(tr("Opened %1").arg(document_->displayName()), 4000);
}

void AnimatorWindow::onNewDocument() {
    if (!confirmDiscardChanges()) {
        return;
    }
    document_->reset();
    statusBar()->showMessage(tr("New animation"), 3000);
}

void AnimatorWindow::onOpenDocument() {
    const QString path = QFileDialog::getOpenFileName(
        this, tr("Open Animation"), QString(),
        tr("Incogine animations (*.incoanim);;All files (*)"));
    if (!path.isEmpty()) {
        openPath(path);
    }
}

void AnimatorWindow::onSaveDocument() {
    if (!document_->hasPath()) {
        onSaveDocumentAs();
        return;
    }
    QString error;
    if (!document_->save(&error)) {
        QMessageBox::critical(this, tr("Incogine Animator"),
                              tr("Could not save the animation:\n%1").arg(error));
        return;
    }
    statusBar()->showMessage(tr("Saved %1").arg(document_->displayName()), 3000);
}

void AnimatorWindow::onSaveDocumentAs() {
    const QString suggested =
        document_->hasPath()
            ? document_->path()
            : QDir(QStandardPaths::writableLocation(
                       QStandardPaths::DocumentsLocation))
                  .filePath(QStringLiteral("untitled.incoanim"));
    const QString path = QFileDialog::getSaveFileName(
        this, tr("Save Animation As"), suggested,
        tr("Incogine animations (*.incoanim)"));
    if (path.isEmpty()) {
        return;
    }
    // A user who types "hero" instead of "hero.incoanim" still gets a valid
    // asset, which matters because the asset browser dispatches on extension.
    QString target = path;
    if (QFileInfo(target).suffix().compare(QStringLiteral("incoanim"),
                                           Qt::CaseInsensitive) != 0) {
        target += QStringLiteral(".incoanim");
    }
    QString error;
    if (!document_->saveAs(target, &error)) {
        QMessageBox::critical(this, tr("Incogine Animator"),
                              tr("Could not save the animation:\n%1").arg(error));
        return;
    }
    statusBar()->showMessage(tr("Saved %1").arg(document_->displayName()), 3000);
}

void AnimatorWindow::onUndo() {
    if (!document_->undo()) {
        return;
    }
    statusBar()->showMessage(tr("Undo %1").arg(document_->undoName()), 2000);
}

void AnimatorWindow::onRedo() {
    if (!document_->redo()) {
        return;
    }
    statusBar()->showMessage(tr("Redo %1").arg(document_->redoName()), 2000);
}

bool AnimatorWindow::confirmDiscardChanges() {
    if (!document_->isDirty()) {
        return true;
    }
    const QMessageBox::StandardButton answer = QMessageBox::warning(
        this, tr("Incogine Animator"),
        tr("\"%1\" has unsaved changes.\n\nSave before continuing?")
            .arg(document_->displayName()),
        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel,
        QMessageBox::Save);
    if (answer == QMessageBox::Cancel) {
        return false;
    }
    if (answer == QMessageBox::Save) {
        if (!document_->hasPath()) {
            onSaveDocumentAs();
            // A cancelled Save As leaves the document dirty, so keep the caller
            // from discarding the edits after all.
            return !document_->isDirty();
        }
        QString error;
        if (!document_->save(&error)) {
            QMessageBox::critical(this, tr("Incogine Animator"),
                                  tr("Could not save the animation:\n%1")
                                      .arg(error));
            return false;
        }
    }
    return true;
}

void AnimatorWindow::closeEvent(QCloseEvent* event) {
    if (!confirmDiscardChanges()) {
        event->ignore();
        return;
    }
    // Release the socket so a later launch can bind this document again.
    if (channel_ != nullptr) {
        channel_->close();
    }

    QSettings settings;
    settings.setValue(QStringLiteral("animator/geometry"), saveGeometry());
    settings.setValue(QStringLiteral("animator/windowState"), saveState());
    if (toolGroup_ != nullptr && toolGroup_->checkedAction() != nullptr) {
        settings.setValue(QStringLiteral("animator/tool"),
                          toolGroup_->checkedAction()->data().toString());
    }
    event->accept();
}

// ----------------------------------------------------------------- state --

bool AnimatorWindow::isDirty() const {
    return document_->isDirty();
}

QString AnimatorWindow::documentName() const {
    return document_->displayName();
}

void AnimatorWindow::onDocumentChanged() {
    refreshProperties();
    refreshUndoRedo();
}

void AnimatorWindow::onDirtyChanged(bool dirty) {
    refreshWindowTitle();
    if (dirty) {
        statusBar()->showMessage(tr("Modified"));
    }
}

void AnimatorWindow::onPathChanged(const QString&) {
    refreshWindowTitle();
    saveAction_->setEnabled(true);
}

void AnimatorWindow::onAutosaved(const QString& path) {
    statusBar()->showMessage(tr("Autosaved to %1").arg(QFileInfo(path).fileName()),
                             4000);
}

void AnimatorWindow::onChannelOpenRequested(const QString& path) {
    // Another Studio launch handed us this file. Surface the window so the user
    // sees which editor answered.
    showNormal();
    raise();
    activateWindow();
    openPath(path);
    statusBar()->showMessage(
        tr("Focused existing window for %1").arg(QFileInfo(path).fileName()),
        4000);
}

void AnimatorWindow::onZoomChanged(double percent) {
    if (zoomLabel_ == nullptr) {
        return;
    }
    zoomLabel_->setText(tr("%1 %").arg(percent, 0, 'f', percent < 10.0 ? 1 : 0));
}

void AnimatorWindow::onCanvasStatus(const QString& text) {
    if (!text.isEmpty()) {
        statusBar()->showMessage(text, 3000);
    }
}

void AnimatorWindow::onToolTriggered() {
    if (toolGroup_ == nullptr || canvas_ == nullptr) {
        return;
    }
    QAction* action = toolGroup_->checkedAction();
    if (action == nullptr) {
        return;
    }
    canvas_->setActiveTool(action->data().toString().toStdString());
    refreshOptionsVisibility();
    // Give the canvas keyboard focus so Space (temporary Hand) and Delete work
    // without an extra click.
    canvas_->setFocus(Qt::OtherFocusReason);
}

void AnimatorWindow::onFitStage() {
    if (canvas_ != nullptr) {
        canvas_->fitToStage();
    }
}

void AnimatorWindow::onZoomIn() {
    if (canvas_ != nullptr) {
        canvas_->zoomBy(1.25);
    }
}

void AnimatorWindow::onZoomOut() {
    if (canvas_ != nullptr) {
        canvas_->zoomBy(1.0 / 1.25);
    }
}

void AnimatorWindow::onDeleteSelection() {
    if (canvas_ != nullptr) {
        canvas_->deleteSelection();
    }
}

void AnimatorWindow::refreshWindowTitle() {
    const QString name = document_->displayName();
    setWindowTitle(
        tr("%1%2 - Incogine Animator")
            .arg(name, document_->isDirty() ? QStringLiteral("*") : QString()));
}

void AnimatorWindow::refreshUndoRedo() {
    // The command carries its own name, so label the action from the stack
    // instead of Qt's localized generic "Undo".
    undoAction_->setEnabled(document_->canUndo());
    undoAction_->setText(document_->canUndo()
                             ? tr("&Undo %1").arg(document_->undoName())
                             : tr("&Undo"));
    redoAction_->setEnabled(document_->canRedo());
    redoAction_->setText(document_->canRedo()
                             ? tr("&Redo %1").arg(document_->redoName())
                             : tr("&Redo"));
}

void AnimatorWindow::refreshProperties() {
    updatingControls_ = true;
    const AnimDocument& model = document_->document();
    widthSpin_->setValue(model.stageWidth);
    heightSpin_->setValue(model.stageHeight);
    fpsSpin_->setValue(model.fps);
    lengthSpin_->setValue(model.lengthFrames);
    loopBox_->setChecked(model.loop);
    bakeScaleSpin_->setValue(model.bakeScale);
    stageInfo_->setText(stageSummary(model));
    updatingControls_ = false;
}


