// Incogine Studio — Unity-style scene editor tab (Qt Widgets).
// Part of Incogine by leafstudiosDot (MPL-2.0). See LICENSE.
//
// The Scene tab: live game frames on a canvas with a mode toolbar, an
// object hierarchy with drag-reparent, an inspector, and a source view.
// Edits apply to the in-memory round-trip model immediately (dirty flag)
// and flush to disk on Save. 2D dragging is exact through the engine
// Camera; 3D mode offers orbit/zoom/grid until Cube rendering lands.
#pragma once

#include <QTreeWidget>
#include <QWidget>
#include <QElapsedTimer>
#include <QKeyEvent>
#include <QMap>
#include <QPointF>
#include <QSet>
#include <QTimer>

#include <cstdint>
#include <string>
#include <vector>

#include "../core/scene/scene_cpp.h"
#include "preview_session.h"
#include "preview_viewport.h"

class QComboBox;
class QCheckBox;
class QDoubleSpinBox;
class QFocusEvent;
class QLabel;
class QPainter;
class QPlainTextEdit;
class QPushButton;
class QRectF;

namespace icg {
class Camera;
}

// 2D design-space picking matches Square::Render exactly:
// rect (pos.x, pos.y, scale.x * 1280, scale.y * 720), y down.
class SceneEditorTab;

class SceneCanvas : public PreviewCanvas {
    Q_OBJECT

public:
    explicit SceneCanvas(PreviewSession* session, QWidget* parent = nullptr);

    void setEditorCamera(icg::Camera* camera) { camera_ = camera; }
    void setEditor(SceneEditorTab* editor) { editor_ = editor; }
    void setMode2D(bool enabled);
    // 16:9 design rect when offline (no frame); equals fittedRect live.
    QRectF viewRect() const;
    // True while the right mouse button is held in 3D mode (fly-look).
    bool isFlyHeld() const { return rmbDown_ && !mode2D_; }
    // Held WASDQE keys (Qt::Key_*) for 3D fly movement.
    QSet<int> flyKeys() const { return flyKeys_; }

signals:
    void pickRequested(const QPoint& widgetPos);
    void dragMoved(const QPoint& widgetPos);
    void dragFinished();
    void panBy(const QPoint& deltaPixels);
    void zoomBy(double factor, const QPoint& widgetPos);
    void orbitBy(const QPoint& deltaPixels);
    void zoom3DBy(double factor);
    void dropFile(const QString& path, const QPoint& widgetPos);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void keyReleaseEvent(QKeyEvent* event) override;
    void focusOutEvent(QFocusEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;

private:
    PreviewSession* session_ = nullptr;
    SceneEditorTab* editor_ = nullptr;
    icg::Camera* camera_ = nullptr;
    bool mode2D_ = true;
    bool dragging_ = false;
    bool panning_ = false;
    bool orbiting_ = false;
    bool rmbDown_ = false; // right button held (pan in 2D, look in 3D)
    Qt::MouseButton panButton_ = Qt::NoButton; // which button started the pan
    QSet<int> flyKeys_; // held WASDQE (Qt::Key_*) for 3D fly
    QPoint lastPos_;
};

class HierarchyTree : public QTreeWidget {
    Q_OBJECT

public:
    explicit HierarchyTree(QWidget* parent = nullptr);

signals:
    // Empty newParent = drop on root.
    void reparentRequested(const QString& childVar, const QString& newParentVar);

protected:
    void dropEvent(QDropEvent* event) override;
};

class SceneEditorTab : public QWidget {
    Q_OBJECT

public:
    explicit SceneEditorTab(const std::string& projectRoot, PreviewSession* session,
                            QWidget* parent = nullptr);
    ~SceneEditorTab() override;

    // Loads a scene for editing (parsed fresh from disk).
    void setScene(const QString& className, const QString& headerPath,
                  const QString& sourcePath);
    bool isDirty() const { return dirty_; }
    QString sceneClass() const { return sceneClass_; }
    int sceneObjectCount() const;
    int gizmoMode() const; // 0 Move, 1 Rotate, 2 Scale
    // Undo/redo over committed source states (per scene tab instance).
    bool canUndoScene() const { return !undoStack_.empty(); }
    bool canRedoScene() const { return !redoStack_.empty(); }
    void undoScene();
    void redoScene();
    // Writes .cpp/.h to disk; false when nothing was written.
    bool saveSceneToSource();
    // Overlay + picking helpers used by the canvas.
    void drawOverlay(QPainter* painter, const QRectF& fitted);
    bool isMode2D() const;
    // Offline scene paint (no live frame): border, boxes, texts, gizmo.
    void paintOffline(QPainter* painter, const QRectF& view);
    // Selection: objects ("object") and font labels ("text", index in model).
    QString selectedKind() const { return selectedKind_; }
    int selectedIndex() const;
    // Simulated engine window (Scene tab "Window" combo); the canvas fits
    // this aspect like the game window.
    int simWindowWidth() const { return windowWidth_; }
    int simWindowHeight() const { return windowHeight_; }

signals:
    void dirtyChanged(bool dirty);

private slots:
    void onModeChanged(int index);
    void onCameraChanged(int index);
    void onGizmoChanged(int index);
    void onHierarchyClicked(QTreeWidgetItem* item);
    void onReparent(const QString& childVar, const QString& newParentVar);
    void onSpinEdited();
    void onApplyLive();
    void onSaveToSource();
    void onAddObject();
    void onDeleteObject();
    void onPick(const QPoint& widgetPos);
    void onDragMove(const QPoint& widgetPos);
    void onDragFinish();
    void onAnchorChanged(int index);
    void onWindowSizeChanged(int index);
    void applyEdgePin(int edge, bool on); // 0 Left, 1 Right, 2 Top, 3 Bottom
    void onDropFile(const QString& path, const QPoint& widgetPos);
    void onPan(const QPoint& deltaPixels);
    void onZoom2D(double factor, const QPoint& widgetPos);
    void onOrbit(const QPoint& deltaPixels);
    void onZoom3D(double factor);
    void onFlyTick();
    void onUiView();

private:
    void rebuildHierarchy();
    void refreshInspector();
    void refreshTextInspector();
    void refreshSourceView();
    void refreshZoomLabel(); // viewport zoom % under the canvas
    void setDirty(bool dirty);
    void setNote(const QString& text);
    void currentTransform(double pos[3], double rot[3], double scale[3],
                          double color[4]) const;
    bool applySpinsToModel(std::string& error);
    void sendLive(bool force = false);
    bool ensureScene(const QString& action);
    // Widget -> 2D design pixels (y down), honoring pan/zoom.
    bool widgetToWorld(const QPoint& widgetPos, float& outX, float& outY);
    // Parser object index by var, or -1.
    int objectIndex(const QString& var) const;
    const icg::studio::scenecpp::ObjectModel* selectedObject() const;
    const icg::studio::scenecpp::TextItem* selectedText() const;
    // Design-pixel bounds of a placed text label at a viewport scale
    // (fonts scale with the view like the game's windowHeight/720 factor);
    // false when dynamic.
    bool textBounds(const icg::studio::scenecpp::TextItem& text, float viewScale,
                    float& x, float& y, float& w, float& h) const;
    // Numeric rect of an object in design pixels; false when non-numeric.
    bool objectRect(int objectIdx, float& x, float& y, float& w, float& h) const;
    QString makeUniqueVar(const QString& base) const;
    uint64_t nextId() const;

    std::string projectRoot_;
    PreviewSession* session_ = nullptr;
    icg::Camera* camera_ = nullptr;

    SceneCanvas* canvas_ = nullptr;
    QComboBox* modeCombo_ = nullptr;   // 2D / 3D
    QComboBox* cameraCombo_ = nullptr; // Perspective / Orthographic / Isometric
    QComboBox* gizmoCombo_ = nullptr;  // Move / Rotate / Scale
    QCheckBox* snapBox_ = nullptr;
    QPushButton* saveButton_ = nullptr;
    HierarchyTree* hierarchy_ = nullptr;
    QLabel* noteLabel_ = nullptr;
    QDoubleSpinBox* posSpin_[3] = {nullptr, nullptr, nullptr};
    QDoubleSpinBox* rotSpin_[3] = {nullptr, nullptr, nullptr};
    QDoubleSpinBox* scaleSpin_[3] = {nullptr, nullptr, nullptr};
    QDoubleSpinBox* colorSpin_[4] = {nullptr, nullptr, nullptr, nullptr};
    QComboBox* anchorCombo_ = nullptr; // renderUI anchor presets (texts)
    QPushButton* edgeBtn_[4] = {nullptr, nullptr, nullptr, nullptr}; // L/R/T/B constraint toggles
    QComboBox* windowCombo_ = nullptr; // simulated engine window size
    QPushButton* undoButton_ = nullptr; // scene source history (per tab)
    QPushButton* redoButton_ = nullptr;
    QPushButton* uiViewButton_ = nullptr; // reset to the game view
    QLabel* zoomLabel_ = nullptr; // viewport zoom readout under the canvas
    QTimer* flyTimer_ = nullptr; // 3D WASDQE movement ticks
    QLabel* idLabel_ = nullptr;
    QPushButton* applyButton_ = nullptr;
    QPushButton* addButton_ = nullptr;
    QPushButton* deleteButton_ = nullptr;
    QPlainTextEdit* sourceView_ = nullptr;

    QMap<QString, QString> fontFamilies_; // asset path -> loaded family
    QString familyForFont(const QString& assetPath);
    // Rasterized text size in engine px (mirrors Font::getSize after the
    // windowHeight/720 font scale all in-repo scenes use). Backs both the
    // parser measurer and the constraint margin math (single source).
    bool measureFont(const QString& assetPath, int pointSizePt, int windowHeight,
                     const QString& content, double& w, double& h);

    // ---- scene source history (undo/redo, per tab instance) ----
    struct SceneHistoryEntry {
        std::string sourceText;
        std::string headerText;
    };
    static constexpr size_t kSceneHistoryCap = 50;
    std::vector<SceneHistoryEntry> undoStack_;
    std::vector<SceneHistoryEntry> redoStack_;
    std::string cleanSource_; // last saved (or loaded) state for dirty checks
    std::string cleanHeader_;
    SceneHistoryEntry currentSnapshot() const;
    void pushSceneUndo();   // snapshot pre-edit state, clears redo
    void dropUndoIfNoChange(); // pop the snapshot when the op was a no-op
    bool restoreSnapshot(const SceneHistoryEntry& entry, std::string& error);
    void refreshUndoRedo(); // button enabled states
    bool sceneMatchesClean() const;

    icg::studio::scenecpp::SceneFile sceneFile_;
    bool sceneOk_ = false;
    // Simulated engine window (GetWindowSize reports this; the engine
    // forces 16:9, so presets stay 16:9). Editing/dragging stays in
    // engine pixels at this size.
    int windowWidth_ = 1280;
    int windowHeight_ = 720;
    QString sceneClass_;
    QString sceneHeader_;
    QString sceneSource_;
    QString selectedVar_;
    QString selectedKind_; // "object", "text", or empty
    int selectedIndex_ = -1; // model index for the current selection
    bool dirty_ = false;
    bool headerDirty_ = false;
    bool draggingObject_ = false;
    QPointF dragGrabOffset_; // design-pixel offset, filled at press
    double dragStartX_ = 0.0; // press-time model position (failed-drop restore)
    double dragStartY_ = 0.0;
    bool dragStartValid_ = false;
    QElapsedTimer liveClock_; // throttles live sends during drags/spins
    QElapsedTimer flyClock_; // frame time for 3D fly movement
    float orbitYaw_ = -45.0f;
    float orbitPitch_ = 20.0f;
    float dolly_ = 1.0f;
};
