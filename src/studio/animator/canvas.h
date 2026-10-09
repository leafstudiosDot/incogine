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
#include <QPixmap>
#include <QPointF>
#include <QRectF>
#include <QSet>
#include <QTimer>
#include <QTransform>
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
class FrameCache;

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
// Exactly one table defines what each level changes (tessellation, AA, bake
// scale cap, MSAA) - see QualitySpec below, not scattered `if`s. Draft is
// the zoomed-in lag lever - no antialiasing and 4x coarser curve subdivision,
// ~12-18x faster than Low at zoom 4-8 (measured). Low is the default; High
// halves the subdivision tolerance for inspecting smooth curves up close
// (slower, not faster). Final is NOT a viewport mode: it is the canonical
// quality the RAM cache and export bake at, so cached frames, scrubbing, and
// exported pixels all agree with each other regardless of viewport setting.
enum class PreviewQuality { Draft, Low, High, Final };

// One row of the quality config table. msaaSamples is reserved for the QRhi
// backend (render-target sample count); the CPU raster path ignores it.
struct QualitySpec {
    float toleranceScale = 1.0f; // x kFlattenTolerance for subdivision + arcs
    bool antialias = true;
    float maxBakeScale = 4.0f; // scene-pixmap adaptive cap (Draft never upscales)
    int msaaSamples = 4;       // QRhi future; unused on CPU raster
};

inline QualitySpec qualitySpec(PreviewQuality quality) {
    switch (quality) {
        case PreviewQuality::Draft:
            return QualitySpec{4.0f, false, 1.0f, 0};
        case PreviewQuality::High:
            return QualitySpec{0.5f, true, 4.0f, 8};
        case PreviewQuality::Final:
            return QualitySpec{1.0f, true, 4.0f, 8};
        case PreviewQuality::Low:
        default:
            return QualitySpec{1.0f, true, 4.0f, 4};
    }
}

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
    // Onion skinning (M4): when enabled, the nearest keyframes strictly before
    // and after the current frame paint ghosted behind the scene (Flash red/
    // green convention). View-only: never touches the model or the bake.
    void setOnionSkinEnabled(bool enabled);
    bool onionSkinEnabled() const { return onionSkin_; }
    // --- RAM frame cache (1.3) ---
    // During playback the canvas blits cached composites instead of rebaking
    // vectors per frame; on a miss it falls back to the scene path (always
    // correct) and the cache prefetches ahead. Non-owning: the window owns the
    // cache and shares it with the timeline strip.
    void setFrameCache(FrameCache* cache);
    // True while the timeline is playing: enables cache blits + prefetch.
    // Scrubbing with playback off stays on the exact scene path.
    void setPlaybackActive(bool active);
    bool isPlaybackActive() const { return playbackActive_; }

    // True when every current shape is baked into the scene pixmap (no pending
    // chunked work). For tests and benchmarks that must observe a settled
    // scene without pumping the event loop for timer slices.
    bool isSceneBaked() const;
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
    void paintOnionSkins(QPainter& painter) const;
    void paintSceneBlit(QPainter& painter) const;
    // Blits the RAM-cache composite for the current frame (1.3 playback
    // path). The image is stage-sized at 1px/unit, transparent: stage fill and
    // backdrop paint underneath exactly like the scene pixmap.
    void paintCacheBlit(QPainter& painter) const;
    void paintLiveSelection(QPainter& painter) const;
    void paintSelectionOutlines(QPainter& painter) const;
    void paintStageOutline(QPainter& painter) const;
    void paintEmptyHint(QPainter& painter) const;
    static QColor toQColor(const icg::anim::AnimColor& color);

    // --- one-pass visible draw list ---
    // Painters share ONE viewport-culled list, rebuilt only when the view or
    // the draw list changed (generations below) instead of once per paint.
    const std::vector<icg::anim::ResolvedShape>& visibleList() const;
    // Bumps the view generation: call from every view mutator (pan/zoom/fit/
    // resize/wheel) so the visible list above stays correct.
    void bumpView() { ++viewGen_; }

    // --- path caches ---
    // Only the compact subdivision is cached (keyed by shape id): path
    // geometry changes only through commands, which clear it. The RASTER paths
    // (baked/outline/pieces QPainterPaths) are deliberately NOT cached per
    // shape anymore: at ~450 kB per fat stroke they OOM before 5000 strokes,
    // while the scene pixmap below holds the whole committed scene in ~8 MB.
    // Raster paths are built transiently where needed (scene bake, selection
    // highlight) and discarded. Mutable so const paint and hit-test paths can
    // populate the subdivision cache.
    const icg::anim::FlatPath& flattenedPath(const icg::anim::AnimShape& shape) const;
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
    bool onionSkin_ = false;
    StageView view_;
    QSet<uint64_t> selection_;
    int currentFrame_ = 1;
    DragState drag_;
    bool needsFitOnFirstSize_ = true;
    mutable QHash<uint64_t, icg::anim::FlatPath> pathCache_;
    PreviewQuality quality_ = PreviewQuality::Low;
    // Committed-scene pixmap (stage space, sceneBakeScale_ px per unit).
    // Transparent: the backdrop/checker paint live underneath every frame.
    QPixmap sceneCache_;
    float sceneBakeScale_ = 1.0f;
    // What the pixmap currently shows, per shape id: bounds (with halo, in
    // stage space) and matrix. The documentChanged diff compares against this
    // to paint added ids incrementally and regionally rebake removed/moved
    // ones - precise invalidation from a coarse signal.
    struct BakedState {
        QRectF bounds;
        icg::anim::Mat2x3 matrix;
        // Everything else that changes rendered pixels. Compared alongside
        // the matrix so e.g. toggling a layer's outline mode repaints instead
        // of going stale (the toggle changes no geometry at all).
        bool hasFill = false;
        icg::anim::AnimColor fill;
        bool hasStroke = false;
        icg::anim::AnimColor stroke;
        float strokeWidth = 0.0f;
        icg::anim::LineCap cap = icg::anim::LineCap::Round;
        icg::anim::LineJoin join = icg::anim::LineJoin::Round;
        bool layerOutline = false;
    };
    // True when `shape` would paint exactly what `state` recorded.
    static bool matchesBaked(const BakedState& state,
                             const icg::anim::ResolvedShape& shape) {
        return state.matrix == shape.matrix && state.hasFill == shape.hasFill &&
               state.fill == shape.fill && state.hasStroke == shape.hasStroke &&
               state.stroke == shape.stroke &&
               state.strokeWidth == shape.strokeWidth &&
               state.cap == shape.cap && state.join == shape.join &&
               state.layerOutline == shape.layerOutline;
    }
    QHash<uint64_t, BakedState> lastBaked_;
    // Stage-space origin of the pixmap's top-left pixel (content can live
    // off-stage, so the pixmap covers stage + content, not just the stage).
    QPointF sceneOrigin_;
    // Chunked-bake state: ids still missing from the pixmap, served a time
    // slice at a time on idle. `bakeGeneration_` aborts stale slices when an
    // edit lands mid-bake (the diff path takes over instead).
    std::vector<uint64_t> bakeQueue_;
    uint64_t bakeGeneration_ = 0;
    bool bakeReported_ = false;
    QTimer bakeTimer_;
    // One culled draw list, shared by every painter in a paint and reused
    // across paints until the view or the draw list moves on (generations).
    mutable std::vector<icg::anim::ResolvedShape> visibleCache_;
    mutable uint64_t visViewGen_ = 0;
    mutable uint64_t visDrawGen_ = 0;
    // Generation counters: viewGen_ bumps on any view change (pan/zoom/fit/
    // resize), drawGen_ wherever the draw list is invalidated. The visible
    // list rebuilds only on mismatch, and the scene-bake diff runs only when
    // sceneDirty_ is set (model/frame/quality/drag changes). Static
    // view + static model = zero per-frame resolve work.
    mutable uint64_t viewGen_ = 0;
    mutable uint64_t drawGen_ = 0;
    mutable bool sceneDirty_ = true;
    // Full draw list, cached across paints. Rebuilding it deep-copies every
    // visible shape's FlatPath, so doing that once per paint dominated
    // per-frame cost once scenes grew; now it rebuilds only when the model,
    // the frame, or the quality changes. Anything that mutates transforms
    // live (updateDrag/cancelDrag) invalidates it alongside the model path.
    mutable std::vector<icg::anim::ResolvedShape> drawCache_;
    mutable bool drawCacheValid_ = false;

    // --- committed-scene raster cache (Part 0 fix) ---
    // Committed shapes bake ONCE into a stage-space pixmap; each repaint
    // blits it instead of re-filling thousands of vector subpaths (~30 ms
    // per fat stroke per fill otherwise - the entire Part 0 lag). Per-frame
    // paint is then blit + live content (preview, selection, dragged shapes,
    // overlays): flat in stroke count. Live vectors never touch the pixmap.
    void ensureSceneBaked();
    // Paints `ids` (current drawList entries) onto the pixmap, in z-order.
    void bakeShapesIntoPixmap(const std::vector<uint64_t>& ids);
    // Erases `stageRect` to transparent and repaints intersecting shapes
    // (except `skipIds`, painted separately) back-to-front.
    void rebakeRegion(const QRectF& stageRect, const QSet<uint64_t>& skipIds);
    // Chunked full (re)bake with soft carry-over (see the .cpp): reallocs,
    // carries old pixels across scaled, queues the missing ids, arms idle
    // slices. `trustBaked=false` drops the baked record (quality/scale
    // changes alter every pixel); true keeps it (coverage growth only).
    void requestFullBake(bool trustBaked);
    void onBakeSlice();
    // One time-boxed chunk of the bake queue; true when work remains.
    bool bakeSliceOnce(int budgetMs);
    // Bake resolution follow: zooming past 2x the baked scale (or far below
    // it) schedules a re-bake at a new scale on idle. Pan never rebakes -
    // the blit sub-rects the stage-space pixmap.
    void maybeAdaptBakeScale();
    // Stage -> pixmap-device transform for the current origin and scale.
    QTransform pixmapTransform() const;
    static BakedState bakedStateFor(const icg::anim::ResolvedShape& shape);
    static QRectF shapeBounds(const icg::anim::ResolvedShape& shape);
    void invalidateDrawCache() const {
        drawCacheValid_ = false;
        ++drawGen_;
        sceneDirty_ = true;
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
    // RAM frame cache + playback flag (1.3). Both non-owning; null until the
    // window wires them. Null-safe everywhere: the canvas works without them.
    FrameCache* frameCache_ = nullptr;
    bool playbackActive_ = false;
};
