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
#include <QPointF>

#include <cstdint>
#include <string>

#include "../core/scene_cpp.h"
#include "preview_session.h"
#include "preview_viewport.h"

class QComboBox;
class QCheckBox;
class QDoubleSpinBox;
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
    // Overlay + picking helpers used by the canvas.
    void drawOverlay(QPainter* painter, const QRectF& fitted);
    bool isMode2D() const;

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
    void onFramesUpdated();
    void onPick(const QPoint& widgetPos);
    void onDragMove(const QPoint& widgetPos);
    void onDragFinish();
    void onDropFile(const QString& path, const QPoint& widgetPos);
    void onPan(const QPoint& deltaPixels);
    void onZoom2D(double factor, const QPoint& widgetPos);
    void onOrbit(const QPoint& deltaPixels);
    void onZoom3D(double factor);

private:
    void rebuildHierarchy();
    void refreshInspector();
    void refreshSourceView();
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
    QLabel* idLabel_ = nullptr;
    QPushButton* applyButton_ = nullptr;
    QPushButton* addButton_ = nullptr;
    QPushButton* deleteButton_ = nullptr;
    QPlainTextEdit* sourceView_ = nullptr;

    icg::studio::scenecpp::SceneFile sceneFile_;
    bool sceneOk_ = false;
    QString sceneClass_;
    QString sceneHeader_;
    QString sceneSource_;
    QString selectedVar_;
    bool dirty_ = false;
    bool headerDirty_ = false;
    bool draggingObject_ = false;
    QPointF dragGrabOffset_; // design-pixel offset, filled at press
    QElapsedTimer liveClock_; // throttles live sends during drags/spins
    float orbitYaw_ = -45.0f;
    float orbitPitch_ = 20.0f;
    float dolly_ = 1.0f;
};
