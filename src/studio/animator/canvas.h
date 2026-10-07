// Incogine Animator - the stage canvas and its view transform.
// Part of Incogine by leafstudiosDot (MPL-2.0). See LICENSE.
//
// Coordinates (Milestone 2 decision): **y-down, origin at the stage's
// top-left corner.** That matches the engine's world space (camera_math.h,
// QuadRenderer), Qt's QPainter orientation, and the animated-sprite draw path,
// so nothing anywhere needs a Y flip. `(0, 0)` is the stage's top-left pixel.
//
// Rendering (Milestone 2 decision): shapes are drawn from
// `anim_geometry::Flatten()`, the SAME flattening the runtime rasterizer will
// use. Qt's own QPainterPath bezier flattening is deliberately not used for the
// geometry, because it is a different algorithm and the preview could then
// differ from the baked sprite sheet. Qt still antialiases the resulting
// polygons, so the canvas stays smooth.
#pragma once

#include <QHash>
#include <QPainterPath>
#include <QPointF>
#include <QRectF>
#include <QSet>
#include <QWidget>

#include <cstdint>
#include <string>
#include <vector>

#include "animation/anim_commands.h"
#include "animation/anim_document.h"
#include "animation/anim_geometry.h"
#include "tools/tools.h"

class AnimatorDocument;
class ITool;
class HandTool;

// Brush / pen style shared by the drawing tools. Owned by the canvas; the
// options strip in the window edits it, and the tools read it when committing.
// Fill is stored for future shape tools - brush and pen are stroke-only in M3
// (except a single-click brush dot, which is a filled circle in the stroke
// color so it looks like a dot rather than a ring).
struct DrawingOptions {
    icg::anim::AnimColor strokeColor = icg::anim::AnimColor(0, 0, 0, 255);
    float strokeWidth = 4.0f; // stage units, zoom-independent
    float opacity = 1.0f;     // 0..1, multiplies the stroke alpha
    float smoothing = 1.0f;   // fit tolerance in stage units (honest: curves stay within it)
    icg::anim::AnimColor fillColor = icg::anim::AnimColor(255, 255, 255, 255);
};

// Preview quality: how much work a repaint may spend on vector fidelity.
// Draft is the zoomed-in lag lever - no antialiasing and 4x coarser curve
// subdivision, ~12-18x faster than Normal at zoom 4-8 (measured). Normal is
// the default; High halves the subdivision tolerance for inspecting smooth
// curves up close (slower, not faster).
enum class PreviewQuality { Draft, Normal, High };

// Pan/zoom mapping between widget pixels and stage units.
class StageView {
public:
    float zoom = 1.0f;  // widget pixels per stage unit
    // Widget-pixel offset of stage origin (0,0).
    QPointF offset;

    QPointF toStage(const QPointF& widget) const;
    QPointF toWidget(const QPointF& stage) const;
    // Stage-space rectangle covered by a widget-space rect.
    QRectF toStageRect(const QRectF& widgetRect) const;

    // Zooms by `factor` keeping the stage point under `anchorWidget` fixed.
    void zoomAt(float factor, const QPointF& anchorWidget);
    void panByPixels(const QPointF& deltaWidget);

    // Zooms/centers so the whole stage fits `viewport` with a margin, in widget
    // pixels. Returns false for a degenerate viewport (nothing sensible to fit).
    bool fitToStage(const QSize& viewport, int stageWidth, int stageHeight,
                    double marginPixels = 24.0);

    static constexpr float kMinZoom = 0.02f;
    static constexpr float kMaxZoom = 64.0f;
};

// The stage canvas.
class AnimatorCanvas : public QWidget {
    Q_OBJECT

public:
    explicit AnimatorCanvas(AnimatorDocument* document,
                            QWidget* parent = nullptr);

    // --- view ---
    const StageView& view() const { return view_; }
    void setView(const StageView& view);
    void zoomBy(double factor);
    void fitToStage();
    // Fits after the first paint, when the widget has a real size.
    void fitWhenSized();

    // --- preview quality ---
    PreviewQuality previewQuality() const { return quality_; }
    void setPreviewQuality(PreviewQuality quality);
    // Flattening tolerance in stage units for this quality level. Everything
    // vector (subdivision, joint/cap arcs) scales off it, so one knob moves
    // the whole fidelity/cost tradeoff.
    float flattenTolerance() const;

    // --- selection ---
    // Selection is a set of shape ids on the ACTIVE layer's active keyframe.
    // Pruned automatically when the document changes, so ids cannot outlive the
    // shapes they name (Milestone 4's playhead will move which keyframe that is).
    const QSet<uint64_t>& selection() const { return selection_; }
    void setSelection(const QSet<uint64_t>& ids);
    void addToSelection(const QSet<uint64_t>& ids);
    void toggleInSelection(uint64_t shapeId);
    void clearSelection();
    int selectionCount() const { return static_cast<int>(selection_.size()); }
    bool isSelected(uint64_t shapeId) const { return selection_.contains(shapeId); }

    // --- frame targeting ---
    // Cursor-tool edits apply to the keyframe covering this frame on the active
    // layer. No timeline UI yet (Milestone 4), so it stays at frame 1 today, but
    // routing through the same rule means nothing is revisited then.
    int currentFrame() const { return currentFrame_; }
    void setCurrentFrame(int frame);

    // Active layer: the topmost visible, unlocked layer. nullptr when every
    // layer is hidden or locked, which is what makes editing refuse cleanly.
    uint64_t activeLayerId() const;
    // Const and mutable variants have distinct names: C++ cannot overload on
    // return type alone.
    const icg::anim::AnimKeyframe* activeKeyframe() const;
    icg::anim::AnimKeyframe* activeKeyframeMutable();

    // True when the active layer can be edited (exists, not locked).
    bool isEditable() const;

    // Hit-tests in stage space against the active keyframe's shapes.
    // Returns the topmost shape id under `stagePos`, or 0 when none.
    // `toleranceStage` widens thin strokes so they stay clickable.
    uint64_t hitTest(const QPointF& stagePos, double toleranceStage = 4.0) const;

    // Shape ids fully enclosed by a stage-space rect (marquee selection).
    std::vector<uint64_t> shapesInRect(const QRectF& stageRect) const;

    // Applies a live drag offset to the selected shapes without touching the
    // command stack; commitDrag() turns it into one undo step. Returns false
    // when there is nothing to drag.
    bool beginDrag(const QPointF& stageAnchor);
    void updateDrag(const QPointF& stagePos);
    bool commitDrag();
    void cancelDrag();
    bool isDragging() const { return drag_.active; }

    // Deletes the current selection on the active keyframe, as one undo step.
    // Refuses (and reports) when the layer is locked.
    void deleteSelection();
    void reportStatus(const QString& text);

    // --- drawing style (Brush / Pen) ---
    const DrawingOptions& drawingOptions() const { return drawOptions_; }
    void setStrokeWidth(float width);
    void setStrokeColor(const icg::anim::AnimColor& color);
    void setStrokeOpacity(float opacity);
    void setSmoothing(float tolerance);
    void setFillColor(const icg::anim::AnimColor& color);

    // Commits one drawn shape on the active keyframe as one undo step,
    // auto-creating the keyframe when needed. Selects the new shape. Returns
    // the new shape id, or 0 when refused (locked/hidden layer, empty path).
    uint64_t addDrawnShape(icg::anim::AnimPath path, icg::anim::AnimStyle style,
                           const std::string& name);

    // --- view helpers the tools use ---
    // Pans by a widget-pixel delta. Kept here so HandTool does not need to know
    // how the transform is stored.
    void panBy(const QPointF& deltaWidget);
    void updateViewAfterPan();
    void setToolCursor();

    // --- tools ---
    // Activates a tool by id; an unknown id falls back to the cursor tool.
    void setActiveTool(const std::string& id);
    void setActiveTool(ITool* tool);
    ITool* activeTool() const { return activeTool_; }
    // The tool set, for the toolbar to enumerate available tools.
    const ToolSet* toolSet() const { return &tools_; }
    // The Hand tool, used temporarily whenever Space is held.
    HandTool* handTool() const;

    void setSpaceHeld(bool held);

    // Resolved drawing state for every drawable shape on the active keyframe,
    // in draw order (lowest index first, i.e. bottom of the stack first).
    // Returned by const reference: the list is rebuilt only when the model,
    // frame or quality changes, so repaints share it instead of deep-copying
    // every FlatPath per frame. Do not hold the reference across any edit.
    const std::vector<icg::anim::ResolvedShape>& drawList() const;

signals:
    void selectionChanged();
    void zoomChanged(double percent);
    void statusMessage(const QString& text);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void keyReleaseEvent(QKeyEvent* event) override;
    void focusOutEvent(QFocusEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    // --- painting helpers ---
    void paintBackdrop(QPainter& painter) const;
    void paintStageFill(QPainter& painter) const;
    void paintShapes(QPainter& painter) const;
    void paintSelectionOutlines(QPainter& painter) const;
    void paintStageOutline(QPainter& painter) const;
    void paintEmptyHint(QPainter& painter) const;
    static QColor toQColor(const icg::anim::AnimColor& color);

    // --- one-pass visible draw list ---
    // `paintShapes`, `paintSelectionOutlines` and `paintEmptyHint` all need the
    // draw list, and each used to build its own (a deep copy of every visible
    // shape's FlatPath). They now share ONE list per paint, viewport-culled
    // once, reused by all three. `visibleList()` caches it; the cache is
    // invalidated whenever the view or the document changes, which is what
    // makes it safe to share across a single paintEvent.
    const std::vector<icg::anim::ResolvedShape>& visibleList() const;
    void invalidateVisibleList() const { visibleCacheValid_ = false; }

    // --- path caches ---
    // Keyed by shape id. Path geometry only changes through commands (which
    // clear the cache), so repaints - including live move drags, which only
    // touch transforms - reuse the subdivision AND the built QPainterPath.
    // Mutable so const paint and hit-test paths can populate them.
    const icg::anim::FlatPath& flattenedPath(const icg::anim::AnimShape& shape) const;
    // Stage-space QPainterPath for one resolved shape, rebuilt only when its
    // matrix changed since the last paint. This is what makes zoomed-in
    // panning cheap: the subdivision and the Qt path build both cache-hit.
    const QPainterPath& bakedPath(const icg::anim::ResolvedShape& shape) const;
    // Stroke outline for one shape: the centerline expanded to a closed,
    // filled band by anim_geometry::StrokeToOutline. Used ONLY for the
    // selection highlight, where the loop's edges are stroked (winding never
    // applies to a stroked highlight, so the loop's self-overlap holes cannot
    // show). Painting uses strokePiecesPath below.
    //
    // Rebuilt when the transform or any stroke parameter changes; it is a pure
    // function of them, so a pan or zoom reuses it entirely.
    const QPainterPath& strokeOutlinePath(
        const icg::anim::ResolvedShape& shape) const;
    // Stroke fill pieces for one shape: the centerline expanded to UNION-CORRECT
    // convex pieces by anim_geometry::StrokeToPieces. Painting these with one
    // WindingFill is ~8-14x faster than stroking the centerline with a pen, and
    // - unlike the single outline loop - stays solid where the stroke crosses
    // itself (a brush circle's overlap). Same cache versioning as the outline.
    const QPainterPath& strokePiecesPath(
        const icg::anim::ResolvedShape& shape) const;
    // Stage-space rectangle currently visible in the widget. Shapes whose
    // stroke-inflated bounds miss it are skipped before any path work.
    QRectF visibleStageRect() const;

    // --- drag state ---
    struct DragState {
        bool active = false;
        uint64_t layerId = 0;
        int frame = 1;
        QPointF anchorStage;
        std::vector<icg::anim::ShapeTransformSnapshot> moves;
    };

    void pruneSelection();
    void emitZoomChanged();

    AnimatorDocument* document_;
    StageView view_;
    QSet<uint64_t> selection_;
    int currentFrame_ = 1;
    DragState drag_;
    bool needsFitOnFirstSize_ = true;
    mutable QHash<uint64_t, icg::anim::FlatPath> pathCache_;
    struct BakedEntry {
        QPainterPath path;
        icg::anim::Mat2x3 matrix;
        bool hasMatrix = false;
    };
    mutable QHash<uint64_t, BakedEntry> bakedCache_;
    // Stroke outlines, keyed by shape id. `OutlineEntry` also remembers the
    // stroke parameters, since width/cap/join all change the geometry.
    struct OutlineEntry {
        QPainterPath path;
        icg::anim::Mat2x3 matrix;
        float width = -1.0f;
        int cap = -1;
        int join = -1;
        bool hasMatrix = false;
    };
    mutable QHash<uint64_t, OutlineEntry> outlineCache_;
    // Stroke fill pieces, keyed the same way (transform + width/cap/join).
    // Reused struct: `width`/`cap`/`join` are the stroke parameters either way.
    mutable QHash<uint64_t, OutlineEntry> piecesCache_;
    PreviewQuality quality_ = PreviewQuality::Normal;
    // One culled draw list per paint, shared by every painter in that paint.
    mutable std::vector<icg::anim::ResolvedShape> visibleCache_;
    mutable bool visibleCacheValid_ = false;
    // Full draw list, cached across paints. Rebuilding it deep-copies every
    // visible shape's FlatPath, so doing that once per paint dominated
    // per-frame cost once scenes grew; now it rebuilds only when the model,
    // the frame, or the quality changes. Anything that mutates transforms
    // live (updateDrag/cancelDrag) invalidates it alongside the model path.
    mutable std::vector<icg::anim::ResolvedShape> drawCache_;
    mutable bool drawCacheValid_ = false;
    void invalidateDrawCache() const {
        drawCacheValid_ = false;
        visibleCacheValid_ = false;
    }
    DrawingOptions drawOptions_;

    // Tool dispatch. The canvas owns the tools and forwards input; Space
    // temporarily routes everything to the Hand tool.
    // Value member: the tools are not QObjects and the canvas owns them for its
    // whole lifetime, so a raw owned pointer would only add a leak to track.
    ToolSet tools_;
    ITool* activeTool_ = nullptr;
    bool spaceHeld_ = false;
    HandTool* handTool_ = nullptr;
    // The tool that was active before Space was pressed, restored on release.
    ITool* toolBeforeSpace_ = nullptr;
};
