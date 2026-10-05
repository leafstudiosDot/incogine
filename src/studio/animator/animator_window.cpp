#include "animator_window.h"

#include <QAction>
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
#include <QPainter>
#include <QSettings>
#include <QSpinBox>
#include <QStandardPaths>
#include <QStatusBar>
#include <QVBoxLayout>

#include "animator_channel.h"
#include "animator_document.h"

using icg::anim::AnimDocument;

namespace {
constexpr int kDefaultWindowWidth = 1400;
constexpr int kDefaultWindowHeight = 900;

// Autosave cadence. Long enough not to thrash the disk while drawing, short
// enough that a crash loses little work.
constexpr int kAutosaveIntervalMs = 60000;

QString stageSummary(const AnimDocument& document) {
    const icg::anim::AnimLayer* top = document.layers.empty()
                                          ? nullptr
                                          : &document.layers.front();
    return QStringLiteral("%1 x %2  ·  %3 fps  ·  %4 frames  ·  %5 layer(s)%6")
        .arg(document.stageWidth)
        .arg(document.stageHeight)
        .arg(document.fps)
        .arg(document.lengthFrames)
        .arg(document.layers.size())
        .arg(top != nullptr && !top->name.empty()
                 ? QStringLiteral("  ·  top: %1")
                       .arg(QString::fromStdString(top->name))
                 : QString());
}
} // namespace

// ------------------------------------------------------------ stage view --

AnimatorStageView::AnimatorStageView(AnimatorDocument* document, QWidget* parent)
    : QWidget(parent), document_(document) {
    setMinimumSize(320, 240);
    // The placeholder centers a stage rect and reports nothing interactive, but
    // it should still repaint when the document changes so Milestone 1 shows
    // the stage size reacting.
    if (document_ != nullptr) {
        connect(document_, &AnimatorDocument::documentChanged, this,
                QOverload<>::of(&QWidget::update));
    }
}

QSize AnimatorStageView::sizeHint() const {
    return QSize(960, 540);
}

void AnimatorStageView::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    painter.fillRect(rect(), palette().window().color().darker(115));

    if (document_ == nullptr) {
        return;
    }
    const AnimDocument& model = document_->document();

    // Draw the stage rect at 1:1-ish scale, centered, with a checkerboard
    // behind it so transparency is visible (Milestone 2 replaces this with the
    // real zoom/pan canvas, but the checkerboard is reused there).
    const int side = std::min(width(), height()) - 80;
    if (side <= 0) {
        return;
    }
    const double scale = static_cast<double>(side) /
                         static_cast<double>(std::max(1, model.stageHeight));
    const QSizeF stageSize(model.stageWidth * scale, model.stageHeight * scale);
    const QRectF stageRect((width() - stageSize.width()) * 0.5,
                           (height() - stageSize.height()) * 0.5, stageSize.width(),
                           stageSize.height());

    // Transparency checkerboard.
    const int cell = 8;
    for (int y = static_cast<int>(stageRect.top());
         y < static_cast<int>(stageRect.bottom()); y += cell) {
        for (int x = static_cast<int>(stageRect.left());
             x < static_cast<int>(stageRect.right()); x += cell) {
            const bool light = (((x / cell) + (y / cell)) % 2) == 0;
            painter.fillRect(QRect(x, y, cell, cell),
                             light ? QColor(220, 220, 220) : QColor(170, 170, 170));
        }
    }

    if (!model.transparentBackground && model.background.a > 0) {
        const icg::anim::AnimColor& bg = model.background;
        painter.fillRect(stageRect,
                         QColor(bg.r, bg.g, bg.b, model.transparentBackground ? 0 : bg.a));
    }

    // Outlined stage rectangle, like Flash's Stage.
    painter.setPen(QPen(QColor(90, 90, 90), 1, Qt::DashLine));
    painter.setBrush(Qt::NoBrush);
    painter.drawRect(stageRect);

    painter.setPen(palette().color(QPalette::WindowText));
    painter.drawText(QRectF(0, stageRect.bottom() + 8, width(), 20), Qt::AlignCenter,
                     tr("Stage  %1 x %2  (%3% zoom)").arg(model.stageWidth)
                         .arg(model.stageHeight)
                         .arg(scale * 100.0, 0, 'f', 0));
}

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
    stage_ = new AnimatorStageView(document_.get(), this);
    setCentralWidget(stage_);
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
    // emit valueChanged while the user types, which would push a command per
    // keystroke, so edits commit on editingFinished instead.
    connect(widthSpin_, &QSpinBox::editingFinished, this, [this] {
        if (!updatingControls()) {
            document_->setStageSize(widthSpin_->value(), heightSpin_->value());
        }
    });
    connect(heightSpin_, &QSpinBox::editingFinished, this, [this] {
        if (!updatingControls()) {
            document_->setStageSize(widthSpin_->value(), heightSpin_->value());
        }
    });
    connect(fpsSpin_, &QSpinBox::editingFinished, this, [this] {
        if (!updatingControls()) {
            document_->setFps(fpsSpin_->value());
        }
    });
    connect(lengthSpin_, &QSpinBox::editingFinished, this, [this] {
        if (!updatingControls()) {
            document_->setLengthFrames(lengthSpin_->value());
        }
    });
    connect(loopBox_, &QCheckBox::toggled, this, [this](bool on) {
        if (!updatingControls()) {
            document_->setLoop(on);
        }
    });
    connect(bakeScaleSpin_, &QDoubleSpinBox::editingFinished, this, [this] {
        if (!updatingControls()) {
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

    // Geometry/state memory, matching Studio's QSettings keys so the two apps
    // feel like one IDE suite.
    restoreGeometryFromSettings();
    saveAction_->setEnabled(document_->hasPath());
}

bool AnimatorWindow::restoreGeometryFromSettings() {
    const QSettings settings;
    if (!settings.contains(QStringLiteral("animator/geometry"))) {
        return false;
    }
    restoreGeometry(settings.value(QStringLiteral("animator/geometry"))
                        .toByteArray());
    const QByteArray state = settings.value(QStringLiteral("animator/windowState"))
                                 .toByteArray();
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
            : QDir(QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation))
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
    if (QFileInfo(target).suffix().compare(
            QStringLiteral("incoanim"), Qt::CaseInsensitive) != 0) {
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
    // Another Studio launch handed us this file. Surface the window so the
    // user sees which editor answered.
    showNormal();
    raise();
    activateWindow();
    openPath(path);
    statusBar()->showMessage(tr("Focused existing window for %1")
                                 .arg(QFileInfo(path).fileName()),
                             4000);
}

void AnimatorWindow::refreshWindowTitle() {
    const QString name = document_->displayName();
    setWindowTitle(tr("%1%2 — Incogine Animator")
                       .arg(name, document_->isDirty() ? QStringLiteral("*") : QString()));
}

void AnimatorWindow::refreshUndoRedo() {
    // QKeySequence::Undo's default text is localized, but our commands carry
    // their own names, so label them from the stack instead.
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
