// Incogine Studio — scene editor tab implementation.
// Part of Incogine by leafstudiosDot (MPL-2.0). See LICENSE.
#include "scene_editor.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QFontDatabase>
#include <QFontMetrics>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
#include <QPaintEvent>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QShortcut>
#include <QSplitter>
#include <QTabWidget>
#include <QTextStream>
#include <QUrl>
#include <QVBoxLayout>
#include <QWheelEvent>
#include <cmath>

#include "../../core/render/camera.h"
#include "../core/scene/scene_cpp_detail.h"

namespace {

constexpr float kDesignWidth = 1280.0f;
constexpr float kDesignHeight = 720.0f;
constexpr float kSnapStep = 10.0f;

double spinValue(QDoubleSpinBox* spin) {
    return spin ? spin->value() : 0.0;
}

QString formatDouble(double v) {
    return QString::number(v, 'g', 6);
}

} // namespace

// ---- SceneCanvas ----

SceneCanvas::SceneCanvas(PreviewSession* session, QWidget* parent)
    : PreviewCanvas(parent), session_(session) {
    setMouseTracking(false);
    setAcceptDrops(true);
    setCursor(Qt::CrossCursor);
    setFocusPolicy(Qt::StrongFocus);
}

void SceneCanvas::setMode2D(bool enabled) {
    mode2D_ = enabled;
    update();
}

void SceneCanvas::paintEvent(QPaintEvent* event) {
    if (!frame_.isNull()) {
        PreviewCanvas::paintEvent(event);
    } else if (editor_) {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        editor_->paintOffline(&painter, viewRect());
        return;
    } else {
        PreviewCanvas::paintEvent(event);
        return;
    }
    if (editor_) {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        editor_->drawOverlay(&painter, viewRect());
    }
}

QRectF SceneCanvas::viewRect() const {
    // Live frames define the rect; offline, the 16:9 design area is fitted
    // the same way so mapping stays identical in both modes.
    if (!frame_.isNull()) {
        return fittedRect();
    }
    if (width() <= 0 || height() <= 0) {
        return QRectF();
    }
    const QSize design(1280, 720);
    const QSize fitted = design.scaled(size(), Qt::KeepAspectRatio);
    const int x = (width() - fitted.width()) / 2;
    const int y = (height() - fitted.height()) / 2;
    return QRectF(x, y, fitted.width(), fitted.height());
}

void SceneCanvas::mousePressEvent(QMouseEvent* event) {
    lastPos_ = event->pos();
    if (event->button() == Qt::MiddleButton) {
        panning_ = true;
        return;
    }
    if (event->button() == Qt::RightButton && !mode2D_) {
        orbiting_ = true;
        return;
    }
    if (event->button() == Qt::LeftButton) {
        dragging_ = true;
        emit pickRequested(event->pos());
    }
}

void SceneCanvas::mouseMoveEvent(QMouseEvent* event) {
    const QPoint delta = event->pos() - lastPos_;
    lastPos_ = event->pos();
    if (panning_) {
        emit panBy(delta);
        return;
    }
    if (orbiting_) {
        emit orbitBy(delta);
        return;
    }
    if (dragging_ && (event->buttons() & Qt::LeftButton)) {
        emit dragMoved(event->pos());
    }
}

void SceneCanvas::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() == Qt::MiddleButton) {
        panning_ = false;
    } else if (event->button() == Qt::RightButton) {
        orbiting_ = false;
    } else if (event->button() == Qt::LeftButton && dragging_) {
        dragging_ = false;
        emit dragFinished();
    }
}

void SceneCanvas::wheelEvent(QWheelEvent* event) {
    const int degrees = event->angleDelta().y();
    if (degrees == 0) {
        return;
    }
    if (mode2D_) {
        emit zoomBy(std::pow(1.0015, -degrees), event->position().toPoint());
    } else {
        emit zoom3DBy(std::pow(1.0015, -degrees));
    }
}

void SceneCanvas::dragEnterEvent(QDragEnterEvent* event) {
    if (event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
    }
}

void SceneCanvas::dropEvent(QDropEvent* event) {
    const QList<QUrl> urls = event->mimeData()->urls();
    if (!urls.isEmpty() && urls.first().isLocalFile()) {
        emit dropFile(urls.first().toLocalFile(), event->position().toPoint());
        event->acceptProposedAction();
    }
}

// ---- HierarchyTree ----

HierarchyTree::HierarchyTree(QWidget* parent) : QTreeWidget(parent) {
    setHeaderLabels({tr("Object"), tr("Detail")});
    setDragEnabled(true);
    setAcceptDrops(true);
    setDropIndicatorShown(true);
    setDragDropMode(QAbstractItemView::InternalMove);
}

void HierarchyTree::dropEvent(QDropEvent* event) {
    QTreeWidget::dropEvent(event); // performs the visual move first
    QTreeWidgetItem* moved = currentItem();
    if (!moved) {
        return;
    }
    const QString childVar = moved->data(0, Qt::UserRole).toString();
    if (childVar.isEmpty()) {
        return;
    }
    QString parentVar;
    QTreeWidgetItem* target = itemAt(event->position().toPoint());
    if (dropIndicatorPosition() == QAbstractItemView::OnItem && target) {
        parentVar = target->data(0, Qt::UserRole).toString();
    } else if (target && target != moved) {
        QTreeWidgetItem* parentItem = target->parent();
        if (parentItem) {
            parentVar = parentItem->data(0, Qt::UserRole).toString();
        }
    }
    emit reparentRequested(childVar, parentVar);
}

// ---- SceneEditorTab ----

SceneEditorTab::SceneEditorTab(const std::string& projectRoot, PreviewSession* session,
                               QWidget* parent)
    : QWidget(parent), projectRoot_(projectRoot), session_(session) {
    camera_ = new icg::Camera(icg::Camera::Make2D(kDesignWidth, kDesignHeight));

    // Toolbar: dimension, camera, gizmo, snap, save.
    modeCombo_ = new QComboBox();
    modeCombo_->addItems({tr("2D"), tr("3D")});
    modeCombo_->setToolTip(tr("Editor dimension (3D needs engine Cube rendering)"));
    cameraCombo_ = new QComboBox();
    cameraCombo_->addItems({tr("Perspective"), tr("Orthographic"), tr("Isometric")});
    cameraCombo_->setToolTip(tr("Studio camera projection"));
    gizmoCombo_ = new QComboBox();
    gizmoCombo_->addItems({tr("Move"), tr("Rotate"), tr("Scale")});
    gizmoCombo_->setToolTip(tr("Drag moves in 2D; rotate/scale via the Inspector"));
    snapBox_ = new QCheckBox(tr("Snap 10px"));
    snapBox_->setToolTip(tr("Snap viewport drags to a 10px grid"));
    undoButton_ = new QPushButton(tr("Undo"));
    undoButton_->setToolTip(tr("Undo the last scene source change (this tab only)"));
    undoButton_->setEnabled(false);
    redoButton_ = new QPushButton(tr("Redo"));
    redoButton_->setToolTip(tr("Redo the undone scene source change (this tab only)"));
    redoButton_->setEnabled(false);
    saveButton_ = new QPushButton(tr("Save to source"));
    saveButton_->setToolTip(tr("Write scene .cpp/.h changes to disk"));

    QHBoxLayout* toolbar = new QHBoxLayout();
    toolbar->addWidget(new QLabel(tr("Scene viewport:")));
    toolbar->addWidget(modeCombo_);
    toolbar->addWidget(cameraCombo_);
    toolbar->addWidget(gizmoCombo_);
    toolbar->addWidget(snapBox_);
    toolbar->addStretch(1);
    toolbar->addWidget(undoButton_);
    toolbar->addWidget(redoButton_);
    toolbar->addWidget(saveButton_);

    canvas_ = new SceneCanvas(session_);
    canvas_->setEditorCamera(camera_);
    canvas_->setEditor(this);
    canvas_->setMode2D(true);

    // Right side: Hierarchy / Inspector / Source.
    auto* side = new QTabWidget();
    hierarchy_ = new HierarchyTree();
    side->addTab(hierarchy_, tr("Hierarchy"));

    auto* inspector = new QWidget();
    QVBoxLayout* inspectorLayout = new QVBoxLayout(inspector);
    auto* grid = new QGridLayout();
    const char* axes[3] = {"X", "Y", "Z"};
    QDoubleSpinBox** spins[3] = {posSpin_, rotSpin_, scaleSpin_};
    const char* rows[3] = {"Position", "Rotation", "Scale"};
    for (int r = 0; r < 3; ++r) {
        grid->addWidget(new QLabel(tr(rows[r])), r, 0);
        for (int c = 0; c < 3; ++c) {
            spins[r][c] = new QDoubleSpinBox();
            spins[r][c]->setRange(-100000.0, 100000.0);
            spins[r][c]->setDecimals(3);
            if (r == 2) {
                spins[r][c]->setValue(1.0);
            }
            grid->addWidget(new QLabel(tr(axes[c])), r, 1 + c * 2);
            grid->addWidget(spins[r][c], r, 2 + c * 2);
            connect(spins[r][c], &QDoubleSpinBox::valueChanged, this,
                    &SceneEditorTab::onSpinEdited);
        }
    }
    inspectorLayout->addLayout(grid);
    auto* colorRow = new QHBoxLayout();
    colorRow->addWidget(new QLabel(tr("Color RGBA:")));
    for (int c = 0; c < 4; ++c) {
        colorSpin_[c] = new QDoubleSpinBox();
        colorSpin_[c]->setRange(0, 255);
        colorSpin_[c]->setValue(255);
        colorRow->addWidget(colorSpin_[c]);
        connect(colorSpin_[c], &QDoubleSpinBox::valueChanged, this,
                &SceneEditorTab::onSpinEdited);
    }
    inspectorLayout->addLayout(colorRow);
    // Anchor presets for renderUI text labels: 9 design-space points over
    // the 1280x720 window (Left/Center/Right x Top/Middle/Bottom), the same
    // GetWindowSize() reference the engine exposes. Choosing one fills the
    // Position spins and commits through the normal text path (constants,
    // so the label stays draggable); shared/dynamic layouts disable it.
    auto* anchorRow = new QHBoxLayout();
    anchorRow->addWidget(new QLabel(tr("Anchor:")));
    anchorCombo_ = new QComboBox();
    anchorCombo_->addItems({tr("Top-Left"), tr("Top-Center"), tr("Top-Right"),
                            tr("Middle-Left"), tr("Center"), tr("Middle-Right"),
                            tr("Bottom-Left"), tr("Bottom-Center"),
                            tr("Bottom-Right")});
    anchorCombo_->setPlaceholderText(tr("Anchor…"));
    anchorCombo_->setCurrentIndex(-1);
    anchorCombo_->setEnabled(false);
    anchorCombo_->setToolTip(
        tr("Snap the label to a window anchor (1280x720 design space)."));
    anchorRow->addWidget(anchorCombo_, 1);
    inspectorLayout->addLayout(anchorRow);
    idLabel_ = new QLabel();
    inspectorLayout->addWidget(idLabel_);
    applyButton_ = new QPushButton(tr("Apply live"));
    applyButton_->setToolTip(tr("Move the running object (needs a file id)"));
    inspectorLayout->addWidget(applyButton_);
    QHBoxLayout* addDelRow = new QHBoxLayout();
    addButton_ = new QPushButton(tr("Add Square"));
    deleteButton_ = new QPushButton(tr("Delete"));
    addDelRow->addWidget(addButton_);
    addDelRow->addWidget(deleteButton_);
    inspectorLayout->addLayout(addDelRow);
    noteLabel_ = new QLabel();
    noteLabel_->setWordWrap(true);
    noteLabel_->setStyleSheet("color: palette(mid);");
    inspectorLayout->addWidget(noteLabel_);
    inspectorLayout->addStretch(1);
    side->addTab(inspector, tr("Inspector"));

    sourceView_ = new QPlainTextEdit();
    sourceView_->setReadOnly(true);
    sourceView_->setObjectName("sceneEditorSource");
    side->addTab(sourceView_, tr("Source"));

    auto* splitter = new QSplitter(Qt::Horizontal);
    splitter->addWidget(canvas_);
    splitter->addWidget(side);
    splitter->setStretchFactor(0, 1);
    splitter->setSizes({900, 300});

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->addLayout(toolbar);
    layout->addWidget(splitter, 1);

    connect(modeCombo_, &QComboBox::currentIndexChanged, this, &SceneEditorTab::onModeChanged);
    connect(cameraCombo_, &QComboBox::currentIndexChanged, this,
            &SceneEditorTab::onCameraChanged);
    connect(gizmoCombo_, &QComboBox::currentIndexChanged, this,
            &SceneEditorTab::onGizmoChanged);
    connect(hierarchy_, &QTreeWidget::itemClicked, this,
            &SceneEditorTab::onHierarchyClicked);
    connect(hierarchy_, &HierarchyTree::reparentRequested, this, &SceneEditorTab::onReparent);
    connect(applyButton_, &QPushButton::clicked, this, &SceneEditorTab::onApplyLive);
    connect(saveButton_, &QPushButton::clicked, this, &SceneEditorTab::onSaveToSource);
    connect(undoButton_, &QPushButton::clicked, this, &SceneEditorTab::undoScene);
    connect(redoButton_, &QPushButton::clicked, this, &SceneEditorTab::redoScene);
    // Tab-local history shortcuts: WidgetWithChildrenShortcut keeps them
    // inside this tab, so each Code tab keeps its own document history.
    auto* undoShortcut = new QShortcut(QKeySequence::Undo, this);
    undoShortcut->setContext(Qt::WidgetWithChildrenShortcut);
    connect(undoShortcut, &QShortcut::activated, this, &SceneEditorTab::undoScene);
    auto* redoShortcut = new QShortcut(QKeySequence::Redo, this);
    redoShortcut->setContext(Qt::WidgetWithChildrenShortcut);
    connect(redoShortcut, &QShortcut::activated, this, &SceneEditorTab::redoScene);
    connect(addButton_, &QPushButton::clicked, this, &SceneEditorTab::onAddObject);
    connect(deleteButton_, &QPushButton::clicked, this, &SceneEditorTab::onDeleteObject);
    // Deliberately NOT connected to framesUpdated: the Scene tab renders
    // the parsed layout itself and never switches to the live view.
    connect(canvas_, &SceneCanvas::pickRequested, this, &SceneEditorTab::onPick);
    connect(canvas_, &SceneCanvas::dragMoved, this, &SceneEditorTab::onDragMove);
    connect(canvas_, &SceneCanvas::dragFinished, this, &SceneEditorTab::onDragFinish);
    // `activated` (not currentIndexChanged): fires only on user picks, and
    // the slot resets the display to the placeholder afterwards.
    connect(anchorCombo_, &QComboBox::activated, this,
            &SceneEditorTab::onAnchorChanged);
    connect(canvas_, &SceneCanvas::panBy, this, &SceneEditorTab::onPan);
    connect(canvas_, &SceneCanvas::zoomBy, this, &SceneEditorTab::onZoom2D);
    connect(canvas_, &SceneCanvas::orbitBy, this, &SceneEditorTab::onOrbit);
    connect(canvas_, &SceneCanvas::zoom3DBy, this, &SceneEditorTab::onZoom3D);
    connect(canvas_, &SceneCanvas::dropFile, this, &SceneEditorTab::onDropFile);
    onCameraChanged(0);
    setNote(tr("Select a scene in the Scenes panel to start editing."));
    // NOTE: the Scene tab never shows live preview frames (see Preview
    // tab). It renders the parsed layout itself, so editing works with no
    // game process running.
}

SceneEditorTab::~SceneEditorTab() {
    delete camera_;
}

void SceneEditorTab::setNote(const QString& text) {
    noteLabel_->setText(text);
}

void SceneEditorTab::setDirty(bool dirty) {
    if (dirty_ != dirty) {
        dirty_ = dirty;
        emit dirtyChanged(dirty);
    }
}

int SceneEditorTab::sceneObjectCount() const {
    return sceneOk_ ? static_cast<int>(sceneFile_.model.objects.size()) : -1;
}

void SceneEditorTab::setScene(const QString& className, const QString& headerPath,
                              const QString& sourcePath) {
    sceneClass_ = className;
    sceneHeader_ = headerPath;
    sceneSource_ = sourcePath;
    selectedVar_.clear();
    draggingObject_ = false;
    std::string error;
    sceneOk_ = icg::studio::scenecpp::ParseSceneFiles(headerPath.toStdString(),
                                                      sourcePath.toStdString(),
                                                      sceneFile_, error);
    if (!sceneOk_) {
        setNote(tr("Scene source uses unrecognized patterns: %1")
                    .arg(QString::fromStdString(error)));
    } else if (sceneFile_.model.objects.empty()) {
        setNote(tr("No parser-known objects in this scene."));
    } else {
        setNote(tr("%1 editable object(s). Drag in the viewport, reorder in "
                   "the hierarchy, then Save to source.")
                    .arg(sceneFile_.model.objects.size()));
    }
    setDirty(false);
    headerDirty_ = false;
    // Fresh history per loaded scene; the clean baseline drives dirty
    // checks after undo/redo instead of a blind dirty flag.
    undoStack_.clear();
    redoStack_.clear();
    cleanSource_ = icg::studio::scenecpp::SerializeSource(sceneFile_);
    cleanHeader_ = icg::studio::scenecpp::SerializeHeader(sceneFile_);
    refreshUndoRedo();
    rebuildHierarchy();
    refreshInspector();
    refreshSourceView();
}

SceneEditorTab::SceneHistoryEntry SceneEditorTab::currentSnapshot() const {
    return {icg::studio::scenecpp::SerializeSource(sceneFile_),
            icg::studio::scenecpp::SerializeHeader(sceneFile_)};
}

void SceneEditorTab::pushSceneUndo() {
    undoStack_.push_back(currentSnapshot());
    while (undoStack_.size() > kSceneHistoryCap) {
        undoStack_.erase(undoStack_.begin());
    }
    redoStack_.clear();
    refreshUndoRedo();
}

void SceneEditorTab::dropUndoIfNoChange() {
    if (undoStack_.empty()) {
        return;
    }
    const SceneHistoryEntry now = currentSnapshot();
    if (now.sourceText == undoStack_.back().sourceText &&
        now.headerText == undoStack_.back().headerText) {
        undoStack_.pop_back();
    }
    refreshUndoRedo();
}

bool SceneEditorTab::sceneMatchesClean() const {
    return icg::studio::scenecpp::SerializeSource(sceneFile_) == cleanSource_ &&
           icg::studio::scenecpp::SerializeHeader(sceneFile_) == cleanHeader_;
}

bool SceneEditorTab::restoreSnapshot(const SceneHistoryEntry& entry,
                                     std::string& error) {
    sceneFile_.sourceText = entry.sourceText;
    sceneFile_.headerText = entry.headerText;
    sceneFile_.sourceLines = icg::studio::scenecpp::detail::SplitLines(
        sceneFile_.sourceText, sceneFile_.sourceEol);
    sceneFile_.headerLines = icg::studio::scenecpp::detail::SplitLines(
        sceneFile_.headerText, sceneFile_.headerEol);
    if (!icg::studio::scenecpp::detail::Reparse(sceneFile_, error)) {
        return false;
    }
    setDirty(!sceneMatchesClean());
    rebuildHierarchy();
    refreshInspector();
    refreshSourceView();
    canvas_->update();
    return true;
}

void SceneEditorTab::undoScene() {
    if (!ensureScene(tr("undo")) || undoStack_.empty()) {
        setNote(tr("Nothing to undo."));
        return;
    }
    redoStack_.push_back(currentSnapshot());
    const SceneHistoryEntry entry = undoStack_.back();
    undoStack_.pop_back();
    std::string error;
    if (!restoreSnapshot(entry, error)) {
        setNote(tr("Undo failed: %1").arg(QString::fromStdString(error)));
        return;
    }
    refreshUndoRedo();
    setNote(tr("Undone. Save to source to persist."));
}

void SceneEditorTab::redoScene() {
    if (!ensureScene(tr("redo")) || redoStack_.empty()) {
        setNote(tr("Nothing to redo."));
        return;
    }
    undoStack_.push_back(currentSnapshot());
    const SceneHistoryEntry entry = redoStack_.back();
    redoStack_.pop_back();
    std::string error;
    if (!restoreSnapshot(entry, error)) {
        setNote(tr("Redo failed: %1").arg(QString::fromStdString(error)));
        return;
    }
    refreshUndoRedo();
    setNote(tr("Redone. Save to source to persist."));
}

void SceneEditorTab::refreshUndoRedo() {
    if (undoButton_) {
        undoButton_->setEnabled(!undoStack_.empty());
    }
    if (redoButton_) {
        redoButton_->setEnabled(!redoStack_.empty());
    }
}

int SceneEditorTab::objectIndex(const QString& var) const {
    const std::string name = var.toStdString();
    for (size_t i = 0; i < sceneFile_.model.objects.size(); ++i) {
        if (sceneFile_.model.objects[i].varName == name) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

bool SceneEditorTab::ensureScene(const QString& action) {
    if (!sceneOk_) {
        setNote(tr("Cannot %1: no editable scene loaded.").arg(action));
        return false;
    }
    return true;
}

bool SceneEditorTab::objectRect(int objectIdx, float& x, float& y, float& w,
                                float& h) const {
    // Matches Square::Render exactly: (x, y, scale.x * winW, scale.y * winH).
    if (objectIdx < 0 ||
        objectIdx >= static_cast<int>(sceneFile_.model.objects.size())) {
        return false;
    }
    const auto& obj = sceneFile_.model.objects[objectIdx];
    if (!obj.hasPosition || !obj.position.numeric || !obj.hasScale ||
        !obj.scale.numeric || obj.scale.values.size() < 2 ||
        obj.position.values.size() < 2) {
        return false;
    }
    x = static_cast<float>(obj.position.values[0]);
    y = static_cast<float>(obj.position.values[1]);
    w = static_cast<float>(obj.scale.values[0]) * kDesignWidth;
    h = static_cast<float>(obj.scale.values[1]) * kDesignHeight;
    return w > 0 && h > 0;
}

bool SceneEditorTab::widgetToWorld(const QPoint& widgetPos, float& outX, float& outY) {
    const QRectF fitted = canvas_->viewRect();
    if (fitted.width() <= 0 || fitted.height() <= 0) {
        return false;
    }
    const float rw = static_cast<float>(fitted.width());
    const float rh = static_cast<float>(fitted.height());
    const float px = static_cast<float>(widgetPos.x() - fitted.x());
    const float py = static_cast<float>(widgetPos.y() - fitted.y());
    const icg::Ray ray = camera_->ScreenPointToRay(px, py, rw, rh);
    float t = 0;
    if (!icg::Camera::IntersectRayPlane(ray, {0, 0, 0}, {0, 0, 1}, t)) {
        return false;
    }
    outX = ray.origin.x + ray.direction.x * t;
    outY = ray.origin.y + ray.direction.y * t;
    return true;
}

void SceneEditorTab::rebuildHierarchy() {
    hierarchy_->clear();
    if (!sceneOk_) {
        return;
    }
    const auto& objects = sceneFile_.model.objects;
    QMap<QString, QTreeWidgetItem*> items;
    // Objects: roots first (unknown parents stay top-level), then children.
    for (const auto& obj : objects) {
        const QString var = QString::fromStdString(obj.varName);
        auto* item = new QTreeWidgetItem();
        QString label = QString::fromStdString(
            obj.displayName.empty() ? obj.varName : obj.displayName);
        label += QString(" (%1)").arg(var);
        item->setText(0, label);
        item->setText(1, QString::fromStdString(obj.typeName));
        item->setData(0, Qt::UserRole, var);
        item->setData(0, Qt::UserRole + 1, "object");
        item->setData(0, Qt::UserRole + 2, -1);
        item->setFlags(item->flags() | Qt::ItemIsDragEnabled | Qt::ItemIsDropEnabled);
        items.insert(var, item);
    }
    for (const auto& obj : objects) {
        const QString var = QString::fromStdString(obj.varName);
        QTreeWidgetItem* item = items.value(var, nullptr);
        if (!item) {
            continue;
        }
        const QString parent =
            (obj.hasParent && items.contains(QString::fromStdString(obj.parentVar)))
                ? QString::fromStdString(obj.parentVar)
                : QString();
        if (parent.isEmpty()) {
            hierarchy_->addTopLevelItem(item);
        } else {
            items[parent]->addChild(item);
        }
    }
    // Font labels: always top-level (the engine has no text parenting).
    // Dynamic ones are listed but not placed; reparent drags reject them.
    for (const auto& text : sceneFile_.model.texts) {
        const QString var = QString::fromStdString(text.varName);
        auto* item = new QTreeWidgetItem();
        QString label = QString::fromStdString(
            text.hasContent && !text.content.empty()
                ? (text.content.size() > 24 ? text.content.substr(0, 24) + "..."
                                            : text.content)
                : "[" + text.varName + "]");
        if (text.dynamicPos) {
            label += tr(" (dynamic)");
        }
        item->setText(0, label);
        item->setText(1, tr("Text"));
        item->setData(0, Qt::UserRole, var);
        item->setData(0, Qt::UserRole + 1, "text");
        item->setData(0, Qt::UserRole + 2, text.index);
        hierarchy_->addTopLevelItem(item);
    }
    hierarchy_->expandAll();
}

void SceneEditorTab::refreshInspector() {
    applyButton_->setEnabled(false);
    deleteButton_->setEnabled(false);
    if (selectedKind_ == "text") {
        refreshTextInspector();
        return;
    }
    if (anchorCombo_) {
        anchorCombo_->setEnabled(false); // anchors are text-label only
    }
    const int idx = objectIndex(selectedVar_);
    const bool has = idx >= 0;
    deleteButton_->setEnabled(has);
    if (!has) {
        idLabel_->setText(tr("No selection."));
        return;
    }
    const auto& obj = sceneFile_.model.objects[idx];
    for (int i = 0; i < 3; ++i) {
        rotSpin_[i]->setEnabled(true);
        scaleSpin_[i]->setEnabled(true);
        posSpin_[i]->setEnabled(true);
    }
    posSpin_[2]->setEnabled(true);
    auto fill = [](QDoubleSpinBox* spins[3],
                   const icg::studio::scenecpp::VecExpr& vec, bool known,
                   double fallback) {
        for (int i = 0; i < 3; ++i) {
            spins[i]->blockSignals(true);
            spins[i]->setValue((known && vec.numeric && i < static_cast<int>(vec.values.size()))
                                   ? vec.values[i]
                                   : fallback);
            spins[i]->blockSignals(false);
        }
    };
    fill(posSpin_, obj.position, obj.hasPosition, 0.0);
    fill(rotSpin_, obj.rotation, obj.hasRotation, 0.0);
    fill(scaleSpin_, obj.scale, obj.hasScale, 1.0);
    const bool isSquare = obj.typeName == "Square";
    for (int i = 0; i < 4; ++i) {
        colorSpin_[i]->blockSignals(true);
        colorSpin_[i]->setEnabled(isSquare);
        colorSpin_[i]->setValue((isSquare && obj.hasColor && obj.color.numeric &&
                                 i < static_cast<int>(obj.color.values.size()))
                                    ? obj.color.values[i]
                                    : 255.0);
        colorSpin_[i]->blockSignals(false);
    }
    if (obj.hasId) {
        idLabel_->setText(tr("id #%1").arg(obj.id));
        applyButton_->setEnabled(session_->isConnected());
    } else {
        idLabel_->setText(tr("No file id — Save to source assigns one."));
    }
}

void SceneEditorTab::refreshTextInspector() {
    const icg::studio::scenecpp::TextItem* text = selectedText();
    deleteButton_->setEnabled(false); // text sites are removed in source
    if (!text) {
        idLabel_->setText(tr("No selection."));
        if (anchorCombo_) {
            anchorCombo_->setEnabled(false);
        }
        return;
    }
    const bool placed = !text->dynamicPos && text->xNum && text->yNum;
    for (int i = 0; i < 3; ++i) {
        posSpin_[i]->blockSignals(true);
        rotSpin_[i]->blockSignals(true);
        scaleSpin_[i]->blockSignals(true);
        rotSpin_[i]->setEnabled(false);
        scaleSpin_[i]->setEnabled(false);
    }
    posSpin_[0]->setEnabled(placed);
    posSpin_[1]->setEnabled(placed);
    posSpin_[2]->setEnabled(false);
    if (placed) {
        posSpin_[0]->setValue(text->xVal);
        posSpin_[1]->setValue(text->yVal);
    } else {
        posSpin_[0]->setValue(0.0);
        posSpin_[1]->setValue(0.0);
    }
    posSpin_[2]->setValue(0.0);
    for (int i = 0; i < 3; ++i) {
        posSpin_[i]->blockSignals(false);
        rotSpin_[i]->blockSignals(false);
        scaleSpin_[i]->blockSignals(false);
    }
    if (anchorCombo_) {
        // Anchors commit constants through SetTextPosition, so they need a
        // placed label with its own call site (shared loop layouts refuse).
        anchorCombo_->blockSignals(true);
        anchorCombo_->setCurrentIndex(-1);
        anchorCombo_->setEnabled(placed && !text->sharedSite);
        anchorCombo_->blockSignals(false);
    }
    for (int i = 0; i < 4; ++i) {
        colorSpin_[i]->blockSignals(true);
        colorSpin_[i]->setEnabled(false); // text color edits stay in source for now
        colorSpin_[i]->setValue(text->hasColor ? text->color[i] : 255.0);
        colorSpin_[i]->blockSignals(false);
    }
    QString info = QString::fromStdString(text->fontFile);
    if (text->pointSize > 0) {
        info += tr(" %1pt").arg(text->pointSize);
    }
    if (!text->hasContent || text->content.empty()) {
        info += tr(" (no literal content)");
    }
    if (!placed) {
        info += tr(" — dynamic layout, not draggable");
    }
    idLabel_->setText(info);
}

void SceneEditorTab::refreshSourceView() {
    if (!sceneOk_) {
        sourceView_->setPlainText(QString());
        return;
    }
    sourceView_->setPlainText(
        QString::fromStdString(icg::studio::scenecpp::SerializeSource(sceneFile_)));
}

void SceneEditorTab::onModeChanged(int index) {
    const bool is2D = index == 0;
    camera_->Set2D(is2D);
    canvas_->setMode2D(is2D);
    if (!is2D) {
        setNote(tr("3D orbit: right-drag orbits, wheel zooms. Picking and "
                   "drag-editing need engine Cube rendering — coming next."));
    }
    canvas_->update();
}

bool SceneEditorTab::isMode2D() const {
    return camera_->Is2D();
}

void SceneEditorTab::onCameraChanged(int index) {
    if (index == 0) {
        camera_->SetProjection(icg::Camera::Projection::Perspective);
    } else if (index == 1) {
        camera_->SetProjection(icg::Camera::Projection::Orthographic);
    } else {
        camera_->SetProjection(icg::Camera::Projection::Isometric);
    }
    canvas_->update();
}

void SceneEditorTab::onGizmoChanged(int /*index*/) {
    canvas_->update(); // overlay shape follows the mode
}

int SceneEditorTab::gizmoMode() const {
    return gizmoCombo_ ? gizmoCombo_->currentIndex() : 0; // 0 Move, 1 Rotate, 2 Scale
}

void SceneEditorTab::onHierarchyClicked(QTreeWidgetItem* item) {
    if (!item) {
        selectedVar_.clear();
        selectedKind_.clear();
        selectedIndex_ = -1;
    } else {
        selectedVar_ = item->data(0, Qt::UserRole).toString();
        selectedKind_ = item->data(0, Qt::UserRole + 1).toString();
        if (selectedKind_.isEmpty()) {
            selectedKind_ = "object"; // legacy items
        }
        selectedIndex_ = item->data(0, Qt::UserRole + 2).toInt();
    }
    refreshInspector();
    canvas_->update();
}

const icg::studio::scenecpp::ObjectModel* SceneEditorTab::selectedObject() const {
    if (selectedKind_ != "object") {
        return nullptr;
    }
    const std::string name = selectedVar_.toStdString();
    for (const auto& obj : sceneFile_.model.objects) {
        if (obj.varName == name) {
            return &obj;
        }
    }
    return nullptr;
}

const icg::studio::scenecpp::TextItem* SceneEditorTab::selectedText() const {
    if (selectedKind_ != "text") {
        return nullptr;
    }
    const std::string name = selectedVar_.toStdString();
    for (const auto& text : sceneFile_.model.texts) {
        if (text.varName == name && text.index == selectedIndex_) {
            return &text;
        }
    }
    return nullptr;
}

bool SceneEditorTab::textBounds(const icg::studio::scenecpp::TextItem& text,
                                float viewScale, float& x, float& y, float& w,
                                float& h) const {
    if (text.dynamicPos || !text.xNum || !text.yNum || viewScale <= 0.0f) {
        return false;
    }
    QString content = QString::fromStdString(text.content);
    if (content.isEmpty()) {
        content = QString("[%1]").arg(QString::fromStdString(text.varName));
    }
    QFont font;
    // familyForFont is non-const (caches); cast is safe here (outer const).
    const QString family =
        const_cast<SceneEditorTab*>(this)->familyForFont(
            QString::fromStdString(text.fontFile));
    if (!family.isEmpty()) {
        font.setFamily(family);
    }
    font.setPointSizeF((text.pointSize > 0 ? text.pointSize : 16) * viewScale);
    const QFontMetricsF metrics(font);
    // Bounds in design pixels so picking matches what is drawn.
    const float scaleW = static_cast<float>(metrics.horizontalAdvance(content)) / viewScale;
    const float scaleH = static_cast<float>(metrics.height()) / viewScale;
    x = static_cast<float>(text.xVal);
    y = static_cast<float>(text.yVal);
    w = scaleW;
    h = scaleH;
    return w > 0 && h > 0;
}

void SceneEditorTab::onReparent(const QString& childVar, const QString& newParentVar) {
    if (!ensureScene(tr("reparent"))) {
        return;
    }
    if (childVar.isEmpty()) {
        rebuildHierarchy();
        return;
    }
    // Texts cannot parent (the engine has no such concept).
    for (const auto& text : sceneFile_.model.texts) {
        if (text.varName == childVar.toStdString()) {
            setNote(tr("Font labels cannot parent — engine has no text hierarchy."));
            rebuildHierarchy();
            return;
        }
    }
    // Cycle guard on the model before rewriting.
    if (!newParentVar.isEmpty()) {
        std::string walk = newParentVar.toStdString();
        for (int depth = 0; depth < 1024; ++depth) {
            if (walk == childVar.toStdString()) {
                setNote(tr("Cannot parent: that would create a cycle."));
                rebuildHierarchy();
                return;
            }
            bool found = false;
            for (const auto& obj : sceneFile_.model.objects) {
                if (obj.varName == walk && obj.hasParent) {
                    walk = obj.parentVar;
                    found = true;
                    break;
                }
            }
            if (!found) {
                break;
            }
        }
    }
    std::string error;
    pushSceneUndo();
    if (!icg::studio::scenecpp::SetParentObject(sceneFile_, childVar.toStdString(),
                                                newParentVar.toStdString(), error)) {
        setNote(tr("Reparent failed: %1").arg(QString::fromStdString(error)));
        dropUndoIfNoChange();
        rebuildHierarchy();
        return;
    }
    dropUndoIfNoChange();
    setDirty(true);
    rebuildHierarchy();
    refreshSourceView();
    setNote(tr("Reparented %1. Save to source to persist.").arg(childVar));
}

void SceneEditorTab::currentTransform(double pos[3], double rot[3], double scale[3],
                                      double color[4]) const {
    for (int i = 0; i < 3; ++i) {
        pos[i] = spinValue(posSpin_[i]);
        rot[i] = spinValue(rotSpin_[i]);
        scale[i] = spinValue(scaleSpin_[i]);
    }
    for (int i = 0; i < 4; ++i) {
        color[i] = spinValue(colorSpin_[i]);
    }
}

bool SceneEditorTab::applySpinsToModel(std::string& error) {
    const int idx = objectIndex(selectedVar_);
    if (idx < 0) {
        error = "no selection";
        return false;
    }
    const std::string var = selectedVar_.toStdString();
    double pos[3], rot[3], scale[3], color[4];
    currentTransform(pos, rot, scale, color);
    const auto& obj = sceneFile_.model.objects[idx];
    if (!icg::studio::scenecpp::SetTransform(
            sceneFile_, var, "position", formatDouble(pos[0]).toStdString(),
            formatDouble(pos[1]).toStdString(), formatDouble(pos[2]).toStdString(), "",
            error) ||
        !icg::studio::scenecpp::SetTransform(
            sceneFile_, var, "rotation", formatDouble(rot[0]).toStdString(),
            formatDouble(rot[1]).toStdString(), formatDouble(rot[2]).toStdString(), "",
            error) ||
        !icg::studio::scenecpp::SetTransform(
            sceneFile_, var, "scale", formatDouble(scale[0]).toStdString(),
            formatDouble(scale[1]).toStdString(), formatDouble(scale[2]).toStdString(),
            "", error)) {
        return false;
    }
    if (obj.typeName == "Square") {
        if (!icg::studio::scenecpp::SetTransform(
                sceneFile_, var, "color", formatDouble(color[0]).toStdString(),
                formatDouble(color[1]).toStdString(), formatDouble(color[2]).toStdString(),
                formatDouble(color[3]).toStdString(), error)) {
            return false;
        }
    }
    return true;
}

void SceneEditorTab::sendLive(bool force) {
    const int idx = objectIndex(selectedVar_);
    if (idx < 0) {
        return;
    }
    const auto& obj = sceneFile_.model.objects[idx];
    if (!obj.hasId) {
        return;
    }
    // Live sends wait for an engine ack (~a frame); throttle drag/spin
    // streams so the UI thread never stalls, but always send on force.
    if (!force && liveClock_.isValid() && liveClock_.elapsed() < 100) {
        return;
    }
    liveClock_.restart();
    double pos[3], rot[3], scale[3], color[4];
    currentTransform(pos, rot, scale, color);
    const float p[3] = {static_cast<float>(pos[0]), static_cast<float>(pos[1]),
                        static_cast<float>(pos[2])};
    const float r[3] = {static_cast<float>(rot[0]), static_cast<float>(rot[1]),
                        static_cast<float>(rot[2])};
    const float s[3] = {static_cast<float>(scale[0]), static_cast<float>(scale[1]),
                        static_cast<float>(scale[2])};
    std::string error;
    if (!session_->sendTransform(obj.id, p, r, s, error)) {
        setNote(tr("Live send failed: %1").arg(QString::fromStdString(error)));
    }
}

void SceneEditorTab::onSpinEdited() {
    if (draggingObject_ || !ensureScene(tr("edit"))) {
        return;
    }
    if (selectedKind_ == "text") {
        const icg::studio::scenecpp::TextItem* text = selectedText();
        if (!text || text->dynamicPos) {
            return;
        }
        std::string error;
        pushSceneUndo();
        if (!icg::studio::scenecpp::SetTextPosition(
                sceneFile_, text->varName, text->index,
                formatDouble(posSpin_[0]->value()).toStdString(),
                formatDouble(posSpin_[1]->value()).toStdString(), error)) {
            setNote(tr("Edit failed: %1").arg(QString::fromStdString(error)));
            dropUndoIfNoChange();
            refreshInspector();
            return;
        }
        dropUndoIfNoChange();
        setDirty(true);
        refreshSourceView();
        canvas_->update();
        return;
    }
    const int idx = objectIndex(selectedVar_);
    if (idx < 0) {
        return;
    }
    std::string error;
    pushSceneUndo();
    if (!applySpinsToModel(error)) {
        setNote(tr("Edit failed: %1").arg(QString::fromStdString(error)));
        dropUndoIfNoChange();
        refreshInspector();
        return;
    }
    dropUndoIfNoChange();
    setDirty(true);
    refreshSourceView();
    sendLive();
}

void SceneEditorTab::onAnchorChanged(int index) {
    // 9-point window anchor grid over the 1280x720 design space (the same
    // GetWindowSize() reference the engine exposes). Commits constants so
    // the label stays draggable and round-trippable; the combo resets to
    // its placeholder so it never displays a stale anchor.
    static const double kFX[3] = {0.0, 0.5, 1.0};
    static const double kFY[3] = {0.0, 0.5, 1.0};
    auto reset = [&]() {
        if (anchorCombo_) {
            anchorCombo_->blockSignals(true);
            anchorCombo_->setCurrentIndex(-1);
            anchorCombo_->blockSignals(false);
        }
    };
    if (index < 0 || index > 8 || draggingObject_ || !ensureScene(tr("anchor"))) {
        reset();
        return;
    }
    if (selectedKind_ != "text") {
        reset();
        return;
    }
    const icg::studio::scenecpp::TextItem* text = selectedText();
    if (!text || text->dynamicPos || !text->xNum || !text->yNum ||
        text->sharedSite) {
        setNote(tr("Anchor needs a placed label with its own renderUI call."));
        reset();
        refreshInspector();
        return;
    }
    const double ax = kFX[index % 3] * kDesignWidth;
    const double ay = kFY[index / 3] * kDesignHeight;
    posSpin_[0]->blockSignals(true);
    posSpin_[1]->blockSignals(true);
    posSpin_[0]->setValue(ax);
    posSpin_[1]->setValue(ay);
    posSpin_[0]->blockSignals(false);
    posSpin_[1]->blockSignals(false);
    reset();
    onSpinEdited(); // commits through the normal text position path
    const icg::studio::scenecpp::TextItem* moved = selectedText();
    if (moved && !moved->dynamicPos && moved->xVal == ax && moved->yVal == ay) {
        setNote(tr("Anchored to %1. Save to source to persist.")
                    .arg(anchorCombo_ ? anchorCombo_->itemText(index) : tr("anchor")));
    }
}

void SceneEditorTab::onApplyLive() {
    if (!ensureScene(tr("apply"))) {
        return;
    }
    if (selectedKind_ == "text") {
        setNote(tr("Font labels have no live objects — save to source instead."));
        return;
    }
    const int idx = objectIndex(selectedVar_);
    if (idx < 0) {
        return;
    }
    if (!sceneFile_.model.objects[idx].hasId) {
        setNote(tr("No file id — Save to source assigns one first."));
        return;
    }
    sendLive(true);
}

void SceneEditorTab::onSaveToSource() {
    if (!ensureScene(tr("save"))) {
        return;
    }
    // Byte-verbatim write (no QIODevice::Text): the serializer already
    // carries the detected EOL, and Text mode would translate every \n
    // into \r\n on Windows — doubling CRLF files into blank lines.
    QFile sourceFile(sceneSource_);
    if (!sourceFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        setNote(tr("Cannot write %1").arg(sceneSource_));
        return;
    }
    QTextStream stream(&sourceFile);
    stream << QString::fromStdString(icg::studio::scenecpp::SerializeSource(sceneFile_));
    sourceFile.close();
    if (headerDirty_) {
        QFile headerFile(sceneHeader_);
        if (headerFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            QTextStream hstream(&headerFile);
            hstream << QString::fromStdString(
                icg::studio::scenecpp::SerializeHeader(sceneFile_));
            headerFile.close();
            headerDirty_ = false;
        } else {
            setNote(tr("Source saved; header write failed: %1").arg(sceneHeader_));
            setDirty(false);
            return;
        }
    }
    setDirty(false);
    cleanSource_ = icg::studio::scenecpp::SerializeSource(sceneFile_);
    cleanHeader_ = icg::studio::scenecpp::SerializeHeader(sceneFile_);
    refreshUndoRedo();
    setNote(tr("Saved — rebuild + relaunch the preview to run it."));
}

QString SceneEditorTab::makeUniqueVar(const QString& base) const {
    for (int n = 1; n < 100000; ++n) {
        const QString candidate = base + "_" + QString::number(n);
        if (objectIndex(candidate) < 0) {
            return candidate;
        }
    }
    return base + "_new";
}

uint64_t SceneEditorTab::nextId() const {
    uint64_t maxId = 0;
    for (const auto& obj : sceneFile_.model.objects) {
        if (obj.hasId && obj.id > maxId) {
            maxId = obj.id;
        }
    }
    return maxId + 1;
}

void SceneEditorTab::onAddObject() {
    onDropFile(QString(), QPoint(-1, -1)); // centered placeholder
}

void SceneEditorTab::onDeleteObject() {
    if (!ensureScene(tr("delete"))) {
        return;
    }
    if (selectedKind_ == "text") {
        setNote(tr("Font labels are removed in source for now (render call + decl)."));
        return;
    }
    const int idx = objectIndex(selectedVar_);
    if (idx < 0) {
        return;
    }
    const std::string var = selectedVar_.toStdString();
    icg::studio::scenecpp::RemoveResult result;
    std::string error;
    pushSceneUndo();
    if (!icg::studio::scenecpp::RemoveObject(sceneFile_, var, result, error)) {
        setNote(tr("Delete failed: %1").arg(QString::fromStdString(error)));
        dropUndoIfNoChange();
        return;
    }
    dropUndoIfNoChange();
    selectedVar_.clear();
    selectedKind_.clear();
    selectedIndex_ = -1;
    setDirty(true);
    rebuildHierarchy();
    refreshInspector();
    refreshSourceView();
    if (result.needsAttention) {
        QString refs;
        for (size_t ln : result.otherRefs) {
            refs += QString::number(ln) + " ";
        }
        setNote(tr("Deleted. Also referenced at line(s) %1— update by hand.")
                    .arg(refs.trimmed()));
    } else {
        setNote(tr("Deleted. Save to source to persist."));
    }
}

void SceneEditorTab::onDropFile(const QString& path, const QPoint& widgetPos) {
    if (!ensureScene(tr("drop"))) {
        return;
    }
    // Placeholder boxes until the engine supports textures/scripts on drop.
    float worldX = kDesignWidth / 2.0f, worldY = kDesignHeight / 2.0f;
    if (widgetPos.x() >= 0 &&
        widgetToWorld(widgetPos, worldX, worldY) == false) {
        return;
    }
    if (snapBox_->isChecked()) {
        worldX = std::round(worldX / kSnapStep) * kSnapStep;
        worldY = std::round(worldY / kSnapStep) * kSnapStep;
    }
    QString display = QFileInfo(path).baseName();
    if (display.isEmpty()) {
        display = "Square";
    }
    const QString var = makeUniqueVar("Square");
    icg::studio::scenecpp::AddResult result;
    std::string error;
    pushSceneUndo();
    if (!icg::studio::scenecpp::AddObject(
            sceneFile_, "Square", var.toStdString(), display.toStdString(), nextId(),
            true, formatDouble(worldX).toStdString(), formatDouble(worldY).toStdString(),
            "0", result, error)) {
        setNote(tr("Drop failed: %1").arg(QString::fromStdString(error)));
        dropUndoIfNoChange();
        return;
    }
    dropUndoIfNoChange();
    headerDirty_ = headerDirty_ || result.headerUpdated;
    selectedVar_ = var;
    setDirty(true);
    rebuildHierarchy();
    refreshInspector();
    refreshSourceView();
    setNote(tr("Added %1 (placeholder box — textures/scripts land later). "
               "Save to source to persist.")
                .arg(var));
}

void SceneEditorTab::onPick(const QPoint& widgetPos) {
    if (!sceneOk_) {
        return;
    }
    float worldX = 0, worldY = 0;
    if (modeCombo_->currentIndex() != 0 || !widgetToWorld(widgetPos, worldX, worldY)) {
        if (modeCombo_->currentIndex() != 0) {
            setNote(tr("Picking needs engine Cube rendering — 3D editing comes next."));
        }
        return;
    }
    // Topmost = last in file order (painter's algorithm, no depth test).
    // Texts paint above boxes, so they pick first.
    int hitObject = -1;
    for (int i = static_cast<int>(sceneFile_.model.objects.size()) - 1; i >= 0; --i) {
        float x = 0, y = 0, w = 0, h = 0;
        if (!objectRect(i, x, y, w, h)) {
            continue;
        }
        if (worldX >= x && worldX <= x + w && worldY >= y && worldY <= y + h) {
            hitObject = i;
            break;
        }
    }
    int hitText = -1;
    const float pickScale = static_cast<float>(canvas_->viewRect().width()) / kDesignWidth;
    for (int i = static_cast<int>(sceneFile_.model.texts.size()) - 1; i >= 0; --i) {
        float x = 0, y = 0, w = 0, h = 0;
        if (!textBounds(sceneFile_.model.texts[i], pickScale, x, y, w, h)) {
            continue;
        }
        if (worldX >= x && worldX <= x + w && worldY >= y && worldY <= y + h) {
            hitText = i;
            break;
        }
    }
    if (hitText < 0 && hitObject < 0) {
        selectedVar_.clear();
        selectedKind_.clear();
        selectedIndex_ = -1;
        draggingObject_ = false;
        dragStartValid_ = false;
    } else if (hitText >= 0) {
        const auto& text = sceneFile_.model.texts[hitText];
        selectedVar_ = QString::fromStdString(text.varName);
        selectedKind_ = "text";
        selectedIndex_ = text.index;
        dragGrabOffset_ = QPointF(worldX - text.xVal, worldY - text.yVal);
        dragStartX_ = text.xVal;
        dragStartY_ = text.yVal;
        dragStartValid_ = true;
        if (text.sharedSite) {
            // One renderUI call draws every sibling: a rewrite would move
            // them all, so the parser refuses — don't start a drag that
            // can only fail on release.
            draggingObject_ = false;
            setNote(tr("Shared loop layout — drag disabled; edit the source instead."));
        } else {
            draggingObject_ = gizmoCombo_->currentIndex() == 0;
            if (!draggingObject_) {
                setNote(tr("Rotate/Scale drag arrives later — use the Inspector spins."));
            }
        }
    } else {
        selectedVar_ =
            QString::fromStdString(sceneFile_.model.objects[hitObject].varName);
        selectedKind_ = "object";
        selectedIndex_ = -1;
        const auto& obj = sceneFile_.model.objects[hitObject];
        if (obj.hasPosition && obj.position.numeric && obj.position.values.size() >= 2) {
            dragGrabOffset_ = QPointF(worldX - obj.position.values[0],
                                      worldY - obj.position.values[1]);
            dragStartX_ = obj.position.values[0];
            dragStartY_ = obj.position.values[1];
            dragStartValid_ = true;
        } else {
            dragGrabOffset_ = QPointF(0, 0);
            dragStartValid_ = false;
        }
        draggingObject_ = gizmoCombo_->currentIndex() == 0;
        if (!draggingObject_) {
            setNote(tr("Rotate/Scale drag arrives later — use the Inspector spins."));
        }
    }
    refreshInspector();
    canvas_->update();
}

void SceneEditorTab::onDragMove(const QPoint& widgetPos) {
    if (!draggingObject_) {
        return;
    }
    if (!ensureScene(tr("drag"))) {
        draggingObject_ = false;
        return;
    }
    float worldX = 0, worldY = 0;
    if (!widgetToWorld(widgetPos, worldX, worldY)) {
        return;
    }
    float nx = worldX - dragGrabOffset_.x();
    float ny = worldY - dragGrabOffset_.y();
    if (snapBox_->isChecked()) {
        nx = std::round(nx / kSnapStep) * kSnapStep;
        ny = std::round(ny / kSnapStep) * kSnapStep;
    }
    posSpin_[0]->blockSignals(true);
    posSpin_[1]->blockSignals(true);
    posSpin_[0]->setValue(nx);
    posSpin_[1]->setValue(ny);
    posSpin_[0]->blockSignals(false);
    posSpin_[1]->blockSignals(false);
    // Live-mutate the in-memory model so the canvas repaints the dragged
    // item at the cursor (realtime feedback). Source spans are rewritten
    // once, on release, by onDragFinish; a failed finish restores the
    // press-time snapshot below.
    if (selectedKind_ == "text") {
        const std::string name = selectedVar_.toStdString();
        for (auto& text : sceneFile_.model.texts) {
            if (text.varName == name && text.index == selectedIndex_) {
                text.xVal = nx;
                text.yVal = ny;
                break;
            }
        }
    } else {
        const int idx = objectIndex(selectedVar_);
        if (idx >= 0) {
            auto& obj = sceneFile_.model.objects[idx];
            if (obj.hasPosition && obj.position.numeric &&
                obj.position.values.size() >= 2) {
                obj.position.values[0] = nx;
                obj.position.values[1] = ny;
            }
        }
    }
    sendLive();
    canvas_->update();
}

void SceneEditorTab::onDragFinish() {
    if (!draggingObject_) {
        return;
    }
    draggingObject_ = false;
    std::string error;
    if (selectedKind_ == "text") {
        const icg::studio::scenecpp::TextItem* text = selectedText();
        pushSceneUndo();
        if (!text || !icg::studio::scenecpp::SetTextPosition(
                         sceneFile_, text->varName, text->index,
                         formatDouble(posSpin_[0]->value()).toStdString(),
                         formatDouble(posSpin_[1]->value()).toStdString(), error)) {
            // Restore the press-time snapshot: the live drag above moved
            // the in-memory item, but the source rewrite refused.
            if (dragStartValid_) {
                const std::string name = selectedVar_.toStdString();
                for (auto& item : sceneFile_.model.texts) {
                    if (item.varName == name && item.index == selectedIndex_) {
                        item.xVal = dragStartX_;
                        item.yVal = dragStartY_;
                        break;
                    }
                }
            }
            setNote(tr("Drag apply failed: %1").arg(QString::fromStdString(error)));
            dragStartValid_ = false;
            dropUndoIfNoChange(); // source untouched: drop the pushed snapshot
            refreshInspector();
            canvas_->update();
            return;
        }
        dragStartValid_ = false;
        dropUndoIfNoChange(); // click-without-move pushes a no-op: drop it
        setDirty(true);
        refreshSourceView();
        refreshInspector();
        canvas_->update();
        setNote(tr("Moved. Save to source to persist (text has no live preview)."));
        return;
    }
    pushSceneUndo();
    if (!applySpinsToModel(error)) {
        // Restore the press-time position: the live drag moved the
        // in-memory object, but the source rewrite refused.
        if (dragStartValid_) {
            const int idx = objectIndex(selectedVar_);
            if (idx >= 0) {
                auto& obj = sceneFile_.model.objects[idx];
                if (obj.hasPosition && obj.position.numeric &&
                    obj.position.values.size() >= 2) {
                    obj.position.values[0] = dragStartX_;
                    obj.position.values[1] = dragStartY_;
                }
            }
        }
        dragStartValid_ = false;
        setNote(tr("Drag apply failed: %1").arg(QString::fromStdString(error)));
        dropUndoIfNoChange();
        refreshInspector();
        canvas_->update();
        return;
    }
    dragStartValid_ = false;
    dragStartValid_ = false;
    dropUndoIfNoChange();
    setDirty(true);
    refreshSourceView();
    refreshInspector();
    canvas_->update();
    sendLive(true);
    setNote(tr("Moved. Save to source to persist."));
}

void SceneEditorTab::onPan(const QPoint& deltaPixels) {
    const QRectF fitted = canvas_->viewRect();
    if (fitted.width() <= 0) {
        return;
    }
    const float designPerPixel = kDesignWidth / static_cast<float>(fitted.width());
    const icg::Vec3 pan = camera_->GetPan();
    camera_->SetPan(pan.x - deltaPixels.x() * designPerPixel,
                    pan.y - deltaPixels.y() * designPerPixel);
    canvas_->update();
}

void SceneEditorTab::onZoom2D(double factor, const QPoint& widgetPos) {
    float worldX = 0, worldY = 0;
    if (!widgetToWorld(widgetPos, worldX, worldY)) {
        // Fall back to center zoom when off-canvas math fails.
        camera_->SetZoom(camera_->GetZoom() * static_cast<float>(factor));
        canvas_->update();
        return;
    }
    const float oldZoom = camera_->GetZoom();
    const float newZoom = qBound(0.1f, oldZoom * static_cast<float>(factor), 8.0f);
    const icg::Vec3 pan = camera_->GetPan();
    // Keep the world point under the cursor fixed.
    camera_->SetZoom(newZoom);
    camera_->SetPan(pan.x + worldX * (newZoom - oldZoom),
                    pan.y + worldY * (newZoom - oldZoom));
    canvas_->update();
}

void SceneEditorTab::onOrbit(const QPoint& deltaPixels) {
    // Track current yaw/pitch via projection round-trip is overkill; keep a
    // local orbit state instead.
    orbitYaw_ -= deltaPixels.x() * 0.25f;
    orbitPitch_ += deltaPixels.y() * 0.25f;
    camera_->SetYawPitch(orbitYaw_, orbitPitch_);
    canvas_->update();
}

void SceneEditorTab::onZoom3D(double factor) {
    // Works for perspective (distance) and ortho/iso (height) alike via a
    // shared dolly factor on the projection size.
    dolly_ = qBound(0.1f, dolly_ * static_cast<float>(factor), 20.0f);
    camera_->SetDistance(10.0f * dolly_);
    camera_->SetOrthoHeight(10.0f * dolly_);
    canvas_->update();
}

QString SceneEditorTab::familyForFont(const QString& assetPath) {
    if (assetPath.isEmpty()) {
        return QString();
    }
    auto it = fontFamilies_.find(assetPath);
    if (it != fontFamilies_.end()) {
        return it.value();
    }
    const QString full =
        QString::fromStdString(projectRoot_ + "/src/assets/" + assetPath.toStdString());
    const int id = QFontDatabase::addApplicationFont(full);
    QString family;
    if (id != -1) {
        const QStringList families = QFontDatabase::applicationFontFamilies(id);
        if (!families.isEmpty()) {
            family = families.first();
        }
    }
    fontFamilies_.insert(assetPath, family);
    return family;
}

void SceneEditorTab::paintOffline(QPainter* painter, const QRectF& view) {
    // Pure black like the game's glClearColor — the viewport must match
    // what the game sees.
    painter->fillRect(rect(), Qt::black);
    if (view.width() <= 0 || view.height() <= 0 || !sceneOk_) {
        painter->setPen(Qt::gray);
        painter->drawText(rect(), Qt::AlignCenter,
                          tr("Select a scene in the Scenes panel"));
        return;
    }
    const float rw = static_cast<float>(view.width());
    const float rh = static_cast<float>(view.height());
    auto toWidget = [&](float worldX, float worldY) {
        const icg::Vec3 s = camera_->WorldToScreen({worldX, worldY, 0}, rw, rh);
        return QPointF(view.x() + s.x, view.y() + s.y);
    };
    // 1px border: exactly what the game sees (16:9 design area).
    painter->setPen(QPen(Qt::white, 1));
    painter->setBrush(Qt::NoBrush);
    painter->drawRect(QRectF(view.x() + 0.5, view.y() + 0.5, view.width() - 1,
                             view.height() - 1));
    // Boxes: Squares filled, others outlined + labeled.
    for (size_t i = 0; i < sceneFile_.model.objects.size(); ++i) {
        const auto& obj = sceneFile_.model.objects[i];
        float x = 0, y = 0, w = 0, h = 0;
        if (!objectRect(static_cast<int>(i), x, y, w, h)) {
            continue;
        }
        const QPointF a = toWidget(x, y);
        const QPointF b = toWidget(x + w, y + h);
        const QRectF rect(a, b);
        if (obj.typeName == "Square" && obj.hasColor && obj.color.numeric &&
            obj.color.values.size() >= 4) {
            painter->setBrush(QColor(static_cast<int>(obj.color.values[0]),
                                     static_cast<int>(obj.color.values[1]),
                                     static_cast<int>(obj.color.values[2]),
                                     static_cast<int>(obj.color.values[3])));
            painter->setPen(Qt::NoPen);
        } else {
            painter->setBrush(Qt::NoBrush);
            painter->setPen(QPen(Qt::white, 1, Qt::DashLine));
        }
        painter->drawRect(rect);
        if (obj.typeName != "Square") {
            painter->setPen(Qt::white);
            painter->drawText(rect.adjusted(2, 2, -2, -2),
                              Qt::AlignLeft | Qt::AlignTop | Qt::TextSingleLine,
                              QString::fromStdString(obj.varName));
        }
    }
    // Font labels at their placed positions (dynamic ones are listed, not drawn).
    // Sizes scale with the viewport (base 1280x720) like the game's
    // windowHeight/720 factor, so layout matches at any canvas size — and
    // stays backend-agnostic (SDL3/OpenGL today, DirectX/Metal/Vulkan later
    // must preserve the same design-pixel mapping).
    const float viewScale = rw / kDesignWidth;
    for (const auto& text : sceneFile_.model.texts) {
        if (text.dynamicPos || !text.xNum || !text.yNum) {
            continue;
        }
        QString content = QString::fromStdString(text.content);
        if (content.isEmpty()) {
            content = QString("[%1]").arg(QString::fromStdString(text.varName));
        }
        QFont font;
        const QString family = familyForFont(QString::fromStdString(text.fontFile));
        if (!family.isEmpty()) {
            font.setFamily(family);
        }
        font.setPointSizeF((text.pointSize > 0 ? text.pointSize : 16) * viewScale);
        painter->setFont(font);
        if (text.hasColor) {
            painter->setPen(QColor(text.color[0], text.color[1], text.color[2],
                                   text.color[3]));
        } else {
            painter->setPen(Qt::white);
        }
        const QPointF at = toWidget(static_cast<float>(text.xVal),
                                    static_cast<float>(text.yVal));
        painter->drawText(QRectF(at.x(), at.y(), view.right() - at.x(), 400),
                          Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap, content);
    }
    // Selection + gizmo overlay on top.
    drawOverlay(painter, view);
}

void SceneEditorTab::drawOverlay(QPainter* painter, const QRectF& fitted) {
    if (fitted.width() <= 0 || fitted.height() <= 0) {
        return;
    }
    const float rw = static_cast<float>(fitted.width());
    const float rh = static_cast<float>(fitted.height());
    auto toWidget = [&](float worldX, float worldY) {
        const icg::Vec3 s = camera_->WorldToScreen({worldX, worldY, 0}, rw, rh);
        return QPointF(fitted.x() + s.x, fitted.y() + s.y);
    };
    if (!isMode2D()) {
        // 3D mode: ground grid (X red, Z blue) through the Studio camera.
        // The game renders no 3D yet; this previews camera math + orbit.
        auto toWidget3D = [&](const icg::Vec3& world) {
            const icg::Vec3 s = camera_->WorldToScreen(world, rw, rh);
            return QPointF(fitted.x() + s.x, fitted.y() + s.y);
        };
        painter->setPen(QColor(120, 120, 120, 160));
        for (int i = -10; i <= 10; ++i) {
            const float c = i * 64.0f;
            painter->drawLine(toWidget3D({c, 0, -640}), toWidget3D({c, 0, 640}));
            painter->drawLine(toWidget3D({-640, 0, c}), toWidget3D({640, 0, c}));
        }
        painter->setPen(Qt::red);
        painter->drawLine(toWidget3D({0, 0, 0}), toWidget3D({640, 0, 0}));
        painter->setPen(Qt::blue);
        painter->drawLine(toWidget3D({0, 0, 0}), toWidget3D({0, 0, 640}));
        return;
    }
    if (selectedKind_ == "text") {
        const icg::studio::scenecpp::TextItem* text = selectedText();
        float x = 0, y = 0, w = 0, h = 0;
        const float gizmoScale = rw / kDesignWidth;
        if (text && textBounds(*text, gizmoScale, x, y, w, h)) {
            const QPointF topLeft = toWidget(x, y);
            const QPointF bottomRight = toWidget(x + w, y + h);
            painter->setPen(QPen(Qt::yellow, 1, Qt::DashLine));
            painter->setBrush(Qt::NoBrush);
            painter->drawRect(QRectF(topLeft, bottomRight));
        }
        return;
    }
    const int idx = objectIndex(selectedVar_);
    if (idx < 0) {
        return;
    }
    float x = 0, y = 0, w = 0, h = 0;
    if (!objectRect(idx, x, y, w, h)) {
        return;
    }
    const int mode = gizmoCombo_ ? gizmoCombo_->currentIndex() : 0;
    if (mode == 0) {
        // Move gizmo: X (red, right) and Y (green, down in world space).
        const QPointF origin = toWidget(x, y);
        const QPointF xEnd = toWidget(x + 60.0f, y);
        const QPointF yEnd = toWidget(x, y + 60.0f);
        painter->setPen(QPen(Qt::red, 2));
        painter->drawLine(origin, xEnd);
        painter->setPen(QPen(Qt::green, 2));
        painter->drawLine(origin, yEnd);
        painter->setBrush(Qt::white);
        painter->setPen(Qt::black);
        painter->drawRect(QRectF(origin.x() - 4, origin.y() - 4, 8, 8));
    } else if (mode == 1) {
        const QPointF center = toWidget(x + w / 2.0f, y + h / 2.0f);
        const double radius = qMax(8.0, static_cast<double>(qMin(w, h)) * 0.4);
        painter->setPen(QPen(Qt::cyan, 2));
        painter->drawEllipse(center,
                             radius * rw / kDesignWidth, radius * rh / kDesignHeight);
    } else {
        const QPointF topLeft = toWidget(x, y);
        const QPointF bottomRight = toWidget(x + w, y + h);
        painter->setPen(QPen(Qt::yellow, 1, Qt::DashLine));
        painter->drawRect(QRectF(topLeft, bottomRight));
        painter->setBrush(Qt::yellow);
        for (const QPointF& corner :
             {topLeft, bottomRight, QPointF(topLeft.x(), bottomRight.y()),
              QPointF(bottomRight.x(), topLeft.y())}) {
            painter->drawRect(QRectF(corner.x() - 4, corner.y() - 4, 8, 8));
        }
    }
}
