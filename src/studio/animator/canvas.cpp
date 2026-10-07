#include "canvas.h"

#include <QElapsedTimer>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QResizeEvent>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>

#include "document.h"

using icg::anim::AnimColor;
using icg::anim::AnimKeyframe;
using icg::anim::AnimLayer;
using icg::anim::AnimShape;
using icg::anim::Mat2x3;
using icg::anim::ResolvedShape;
using icg::anim::Vec2;

namespace {
constexpr float kWheelZoomStep = 1.15f;
// Flattening tolerance in stage units. The default matches the engine's, so the
// canvas and the runtime rasterizer agree on the polyline.
constexpr float kFlattenTol = icg::anim::kFlattenTolerance;
} // namespace

// ------------------------------------------------------------- StageView --

QPointF StageView::toStage(const QPointF& widget) const {
    return QPointF((widget.x() - offset.x()) / zoom,
                   (widget.y() - offset.y()) / zoom);
}

QPointF StageView::toWidget(const QPointF& stage) const {
    return QPointF(stage.x() * zoom + offset.x(), stage.y() * zoom + offset.y());
}

QRectF StageView::toStageRect(const QRectF& widgetRect) const {
    const QPointF topLeft = toStage(widgetRect.topLeft());
    const QPointF bottomRight = toStage(widgetRect.bottomRight());
    return QRectF(topLeft, bottomRight).normalized();
}

void StageView::zoomAt(float factor, const QPointF& anchorWidget) {
    const float target =
        std::min(kMaxZoom, std::max(kMinZoom, zoom * factor));
    if (target == zoom) {
        return;
    }
    // Keep the stage point under the cursor pinned: solve offset' so that
    // toStage'(anchor) == toStage(anchor).
    const QPointF anchorStage((anchorWidget.x() - offset.x()) / zoom,
                              (anchorWidget.y() - offset.y()) / zoom);
    zoom = target;
    offset = QPointF(anchorWidget.x() - anchorStage.x() * zoom,
                     anchorWidget.y() - anchorStage.y() * zoom);
}

void StageView::panByPixels(const QPointF& deltaWidget) {
    offset += deltaWidget;
}

bool StageView::fitToStage(const QSize& viewport, int stageWidth, int stageHeight,
                           double marginPixels) {
    if (viewport.width() <= 0 || viewport.height() <= 0 || stageWidth <= 0 ||
        stageHeight <= 0) {
        return false;
    }
    const double usableW = std::max(1.0, viewport.width() - marginPixels * 2.0);
    const double usableH = std::max(1.0, viewport.height() - marginPixels * 2.0);
    zoom = static_cast<float>(
        std::min(usableW / stageWidth, usableH / stageHeight));
    zoom = std::min(kMaxZoom, std::max(kMinZoom, zoom));
    // Center the stage in the viewport.
    offset = QPointF((viewport.width() - stageWidth * zoom) * 0.5,
                     (viewport.height() - stageHeight * zoom) * 0.5);
    return true;
}

// --------------------------------------------------------------- helpers --

QColor AnimatorCanvas::toQColor(const AnimColor& color) {
    return QColor(color.r, color.g, color.b, color.a);
}

namespace {
AnimColor toAnimColor(const QColor& color) {
    return AnimColor(color.red(), color.green(), color.blue(), color.alpha());
}
} // namespace

// -------------------------------------------------------- AnimatorCanvas --

AnimatorCanvas::AnimatorCanvas(AnimatorDocument* document, QWidget* parent)
    : QWidget(parent), document_(document) {
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
    setAutoFillBackground(false);
    setMinimumSize(240, 160);

    // Tools are owned here and dispatched to from the event handlers.
    handTool_ = dynamic_cast<HandTool*>(tools_.find("hand"));
    activeTool_ = tools_.find("cursor");

    if (document_ != nullptr) {
        // Any command-driven edit may change geometry or the active layer, so
        // the flattening cache and the selection both need revalidating. The
        // scene pixmap is NOT touched here: paintEvent's ensureSceneBaked()
        // diffs against what the pixmap shows and repaints incrementally, so
        // there is exactly one freshness path and it cannot be forgotten.
        connect(document_, &AnimatorDocument::documentChanged, this, [this] {
            pathCache_.clear();
            invalidateDrawCache();
            pruneSelection();
            update();
        });
    }
    // Chunked scene baking: idle slices, abortable by generation. Started on
    // demand by requestFullBake(); the slice re-arms itself while work remains.
    bakeTimer_.setSingleShot(true);
    bakeTimer_.setInterval(0);
    connect(&bakeTimer_, &QTimer::timeout, this, [this] { onBakeSlice(); });
}

void AnimatorCanvas::setView(const StageView& view) {
    view_ = view;
    bumpView();
    emitZoomChanged();
    update();
}

void AnimatorCanvas::zoomBy(double factor) {
    view_.zoomAt(static_cast<float>(factor), QPointF(width() * 0.5, height() * 0.5));
    bumpView();
    emitZoomChanged();
    update();
}

void AnimatorCanvas::fitToStage() {
    if (document_ == nullptr) {
        return;
    }
    const icg::anim::AnimDocument& model = document_->document();
    if (view_.fitToStage(size(), model.stageWidth, model.stageHeight)) {
        needsFitOnFirstSize_ = false;
        bumpView();
        emitZoomChanged();
        update();
    }
}

void AnimatorCanvas::fitWhenSized() {
    if (!needsFitOnFirstSize_) {
        return;
    }
    // Wait for a real size, otherwise the first fit divides by a 0x0 viewport.
    if (width() > 1 && height() > 1) {
        needsFitOnFirstSize_ = false;
        fitToStage();
    }
}

float AnimatorCanvas::flattenTolerance() const {
    switch (quality_) {
        case PreviewQuality::Draft:
            return kFlattenTol * 4.0f;
        case PreviewQuality::High:
            return kFlattenTol * 0.5f;
        case PreviewQuality::Normal:
        default:
            return kFlattenTol;
    }
}

void AnimatorCanvas::setPreviewQuality(PreviewQuality quality) {
    if (quality == quality_) {
        return;
    }
    quality_ = quality;
    // Geometry at every cache level derives from the tolerance, so all of it
    // goes - including the scene pixmap, whose baked pixels used the old one.
    // The cull pad in visibleList reads flattenTolerance() live, so it
    // follows without its own invalidation.
    pathCache_.clear();
    invalidateDrawCache();
    requestFullBake(false);
    update();
}

void AnimatorCanvas::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    // Widget size feeds the visible stage rect: a resize re-culls.
    bumpView();
    fitWhenSized();
}

void AnimatorCanvas::emitZoomChanged() {
    emit zoomChanged(view_.zoom * 100.0);
}

// ------------------------------------------------------------- selection --

void AnimatorCanvas::setSelection(const QSet<uint64_t>& ids) {
    if (selection_ == ids) {
        return;
    }
    selection_ = ids;
    pruneSelection();
    emit selectionChanged();
    update();
}

void AnimatorCanvas::addToSelection(const QSet<uint64_t>& ids) {
    QSet<uint64_t> merged = selection_;
    merged.unite(ids);
    setSelection(merged);
}

void AnimatorCanvas::toggleInSelection(uint64_t shapeId) {
    QSet<uint64_t> next = selection_;
    if (next.contains(shapeId)) {
        next.remove(shapeId);
    } else {
        next.insert(shapeId);
    }
    setSelection(next);
}

void AnimatorCanvas::clearSelection() {
    setSelection(QSet<uint64_t>());
}

void AnimatorCanvas::pruneSelection() {
    if (selection_.isEmpty() || document_ == nullptr) {
        return;
    }
    const AnimKeyframe* key = activeKeyframe();
    if (key == nullptr) {
        selection_.clear();
        return;
    }
    // Drop ids that no longer exist on the targeted keyframe (deleted shapes, or
    // a frame change moving the target to a keyframe without them).
    QSet<uint64_t> live;
    for (const AnimShape& shape : key->shapes) {
        live.insert(shape.id);
    }
    bool changed = false;
    for (uint64_t id : selection_) {
        if (!live.contains(id)) {
            changed = true;
            break;
        }
    }
    if (!changed) {
        return;
    }
    QSet<uint64_t> kept;
    kept.reserve(selection_.size());
    for (uint64_t id : selection_) {
        if (live.contains(id)) {
            kept.insert(id);
        }
    }
    selection_ = kept;
    emit selectionChanged();
}

// ------------------------------------------------------- layer / keyframe --

uint64_t AnimatorCanvas::activeLayerId() const {
    if (document_ == nullptr) {
        return 0;
    }
    const icg::anim::AnimDocument& model = document_->document();
    // Layer 0 is the topmost layer, which is what a user means by "the layer
    // I'm drawing on". Skip hidden layers; locking is handled by isEditable().
    for (const AnimLayer& layer : model.layers) {
        if (layer.visible) {
            return layer.id;
        }
    }
    return 0;
}

const AnimKeyframe* AnimatorCanvas::activeKeyframe() const {
    if (document_ == nullptr) {
        return nullptr;
    }
    const icg::anim::AnimDocument& model = document_->document();
    const AnimLayer* layer = model.FindLayerById(activeLayerId());
    return layer != nullptr ? layer->AtOrBefore(currentFrame_) : nullptr;
}

AnimKeyframe* AnimatorCanvas::activeKeyframeMutable() {
    if (document_ == nullptr) {
        return nullptr;
    }
    icg::anim::AnimDocument& model = document_->document();
    AnimLayer* layer = model.FindLayerById(activeLayerId());
    return layer != nullptr ? layer->FindMutable(currentFrame_) : nullptr;
}

bool AnimatorCanvas::isEditable() const {
    if (document_ == nullptr) {
        return false;
    }
    const uint64_t id = activeLayerId();
    if (id == 0) {
        return false;
    }
    const AnimLayer* layer = document_->document().FindLayerById(id);
    return layer != nullptr && !layer->locked;
}

void AnimatorCanvas::setCurrentFrame(int frame) {
    const int clamped = std::max(1, frame);
    if (clamped == currentFrame_) {
        return;
    }
    currentFrame_ = clamped;
    // The target keyframe may not hold the selected shapes.
    pruneSelection();
    invalidateDrawCache();
    update();
}

// ----------------------------------------------------------- draw / pick --

const icg::anim::FlatPath& AnimatorCanvas::flattenedPath(
    const AnimShape& shape) const {
    auto cached = pathCache_.find(shape.id);
    if (cached != pathCache_.end()) {
        return *cached;
    }
    return *pathCache_.insert(shape.id,
                               icg::anim::Flatten(shape.path, flattenTolerance()));
}

const std::vector<ResolvedShape>& AnimatorCanvas::visibleList() const {
    if (visViewGen_ == viewGen_ && visDrawGen_ == drawGen_) {
        return visibleCache_;
    }
    visViewGen_ = viewGen_;
    visDrawGen_ = drawGen_;
    visibleCache_.clear();
    // Viewport culling happens HERE, once per paint, instead of once per
    // painter that needed the list. The pad covers the stroke halo plus a
    // pixel of antialiasing, so culling can never clip a visible pixel:
    // artwork is untouched, only fully-offscreen shapes are skipped.
    const QRectF visible = visibleStageRect();
    // The pad covers the stroke halo, a pixel of antialiasing, AND the current
    // flatten tolerance: a coarser Draft subdivision can sit up to `tol` away
    // from the true curve, and culling must never clip a visible pixel.
    const float cullPad = flattenTolerance() + 1.0f;
    for (const ResolvedShape& shape : drawList()) {
        icg::anim::Vec2 lo, hi;
        icg::anim::FlatTransformedBounds(shape.path, shape.matrix,
                                        shape.strokeWidth * 0.5f + cullPad, lo, hi);
        if (hi.x < lo.x || hi.y < lo.y || hi.x < visible.left() ||
            lo.x > visible.right() || hi.y < visible.top() ||
            lo.y > visible.bottom()) {
            continue;
        }
        visibleCache_.push_back(shape);
    }
    return visibleCache_;
}

const std::vector<ResolvedShape>& AnimatorCanvas::drawList() const {
    if (!drawCacheValid_) {
        drawCache_.clear();
        if (document_ != nullptr) {
            const icg::anim::AnimDocument& model = document_->document();
            for (const AnimLayer& layer : model.layers) {
                if (!layer.visible) {
                    continue;
                }
                const AnimKeyframe* key = layer.AtOrBefore(currentFrame_);
                if (key == nullptr) {
                    continue;
                }
                for (const AnimShape& shape : key->shapes) {
                    // Style is cheap (matrix + color mults); the subdivision
                    // reuses the cache, so a rebuild costs O(shapes), not
                    // O(segments) - and repaints reuse the list outright.
                    const icg::anim::ResolvedStyle style =
                        icg::anim::ResolveShapeStyle(shape, *key);
                    const icg::anim::FlatPath& flat = flattenedPath(shape);
                    if (flat.polylines.empty()) {
                        continue;
                    }
                    ResolvedShape resolved;
                    resolved.shapeId = shape.id;
                    resolved.name = shape.name;
                    resolved.path = flat;
                    resolved.matrix = style.matrix;
                    resolved.hasFill = style.hasFill;
                    resolved.fill = style.fill;
                    resolved.hasStroke = style.hasStroke;
                    resolved.stroke = style.stroke;
                    resolved.strokeWidth = style.strokeWidth;
                    resolved.cap = style.cap;
                    resolved.join = style.join;
                    resolved.drawable = true;
                    drawCache_.push_back(std::move(resolved));
                }
            }
            // Layers are stored top-first; painting must run bottom-first.
            std::reverse(drawCache_.begin(), drawCache_.end());
        }
        drawCacheValid_ = true;
    }
    return drawCache_;
}

uint64_t AnimatorCanvas::hitTest(const QPointF& stagePos,
                                 double toleranceStage) const {
    const auto& shapes = drawList();
    // drawList() is bottom-first, so iterate backwards to pick the topmost.
    for (auto it = shapes.rbegin(); it != shapes.rend(); ++it) {
        const QPointF p(stagePos.x(), stagePos.y());
        const Vec2 probe(p.x(), p.y());
        bool hit = false;
        if (it->hasFill) {
            hit = icg::anim::PointInFlatPath(probe, it->path, 0.0f);
        }
        // A stroke is pickable along its whole length even without a fill.
        if (!hit && it->hasStroke) {
            const double reach = toleranceStage + it->strokeWidth * 0.5;
            hit = icg::anim::PointNearFlatPath(probe, it->path,
                                                static_cast<float>(reach));
        }
        if (hit) {
            return it->shapeId;
        }
    }
    return 0;
}

std::vector<uint64_t> AnimatorCanvas::shapesInRect(const QRectF& stageRect) const {
    std::vector<uint64_t> out;
    if (stageRect.width() <= 0.0 || stageRect.height() <= 0.0) {
        return out;
    }
    for (const ResolvedShape& shape : drawList()) {
        // Stroke-inflated stage-space box: a wide stroke counts as enclosed
        // by its rendered pixels, not its centerline, so marquee selection
        // respects stroke width like picking does.
        icg::anim::Vec2 lo, hi;
        icg::anim::FlatTransformedBounds(shape.path, shape.matrix,
                                        shape.strokeWidth * 0.5f, lo, hi);
        if (hi.x < lo.x || hi.y < lo.y) {
            continue;
        }
        const QPointF corners[4] = {
            QPointF(lo.x, lo.y), QPointF(hi.x, lo.y), QPointF(hi.x, hi.y),
            QPointF(lo.x, hi.y),
        };
        bool allInside = true;
        for (const QPointF& corner : corners) {
            if (!stageRect.contains(corner)) {
                allInside = false;
                break;
            }
        }
        // Marquee selects only FULLY enclosed shapes (Flash behaviour), so
        // dragging a box across a big shape does not grab it accidentally.
        if (allInside) {
            out.push_back(shape.shapeId);
        }
    }
    return out;
}

// ------------------------------------------------------------------ drag --

bool AnimatorCanvas::beginDrag(const QPointF& stageAnchor) {
    if (!isEditable() || selection_.isEmpty()) {
        return false;
    }
    AnimKeyframe* key = activeKeyframeMutable();
    if (key == nullptr) {
        return false;
    }
    drag_ = DragState();
    drag_.active = true;
    drag_.layerId = activeLayerId();
    drag_.frame = currentFrame_;
    drag_.anchorStage = stageAnchor;
    for (const AnimShape& shape : key->shapes) {
        if (!selection_.contains(shape.id)) {
            continue;
        }
        icg::anim::ShapeTransformSnapshot snapshot;
        snapshot.shapeId = shape.id;
        snapshot.start = shape.transform;
        snapshot.end = shape.transform;
        drag_.moves.push_back(std::move(snapshot));
    }
    return !drag_.moves.empty();
}

void AnimatorCanvas::updateDrag(const QPointF& stagePos) {
    if (!drag_.active) {
        return;
    }
    AnimKeyframe* key = activeKeyframeMutable();
    if (key == nullptr) {
        return;
    }
    const Vec2 delta(static_cast<float>(stagePos.x() - drag_.anchorStage.x()),
                     static_cast<float>(stagePos.y() - drag_.anchorStage.y()));

    for (auto& snapshot : drag_.moves) {
        for (AnimShape& shape : key->shapes) {
            if (shape.id != snapshot.shapeId) {
                continue;
            }
            // Mutate live for immediate feedback. The pre-drag transform is
            // already captured as `start`; `end` must be refreshed on every
            // move too, or commitDrag would compare start against an unchanged
            // end, see a no-op, and refuse to push an undo entry.
            shape.transform = snapshot.start;
            shape.transform.position = snapshot.start.position + delta;
            snapshot.end = shape.transform;
            break;
        }
    }
    // Live transform mutation bypasses the command stack (no documentChanged),
    // so the draw list must be dropped explicitly or the drag paints stale.
    invalidateDrawCache();
    update();
}

bool AnimatorCanvas::commitDrag() {
    if (!drag_.active) {
        return false;
    }
    const auto moves = drag_.moves;
    const uint64_t layerId = drag_.layerId;
    const int frame = drag_.frame;
    drag_ = DragState();

    if (document_ == nullptr) {
        return false;
    }
    // The live mutation above bypassed the stack; commitDrag pushes a single
    // command holding the before/after transforms. Path geometry never changed,
    // so the flattening cache is still valid.
    if (document_->commitShapeMove(layerId, frame, moves)) {
        update();
        return true;
    }
    // Nothing actually moved: no history entry, and the document is untouched.
    update();
    return false;
}

void AnimatorCanvas::cancelDrag() {
    if (!drag_.active) {
        return;
    }
    const auto moves = drag_.moves;
    drag_ = DragState();
    // Put the shapes back exactly where the drag found them.
    if (document_ == nullptr) {
        return;
    }
    AnimKeyframe* key = activeKeyframeMutable();
    if (key != nullptr) {
        for (const auto& snapshot : moves) {
            for (AnimShape& shape : key->shapes) {
                if (shape.id == snapshot.shapeId) {
                    shape.transform = snapshot.start;
                    break;
                }
            }
        }
    }
    invalidateDrawCache();
    update();
}

// ------------------------------------------------------------- painting --

QRectF AnimatorCanvas::visibleStageRect() const {
    return view_.toStageRect(QRectF(0.0, 0.0, width(), height()));
}

namespace {

// Transient raster builders. These return QPainterPaths BY VALUE and retain
// nothing: the scene pixmap (not per-shape path objects) is the paint cache,
// so steady-state memory stays flat in stroke count. Callers are the scene
// baker (occasional, chunked) and the selection highlight (few shapes).

// The flattened subpaths in stage space. WindingFill: multi-subpath fills
// sharing one winding must stay solid where they overlap; single loops render
// identically under either rule.
QPainterPath buildFillPath(const ResolvedShape& shape) {
    QPainterPath path;
    path.setFillRule(Qt::WindingFill);
    for (const auto& polyline : shape.path.polylines) {
        if (polyline.size() < 2) {
            continue;
        }
        bool first = true;
        for (const Vec2& point : polyline) {
            // The polyline is in shape-local space; the matrix carries it into
            // stage space, which is also what QPainter then scales to widgets.
            const Vec2 stageVec = icg::anim::TransformPoint(shape.matrix, point);
            const QPointF stage(stageVec.x, stageVec.y);
            if (first) {
                path.moveTo(stage);
                first = false;
            } else {
                path.lineTo(stage);
            }
        }
    }
    return path;
}

// The centerline expanded to union-correct fill pieces, in stage space.
QPainterPath buildPiecesPath(const ResolvedShape& shape, float tolerance) {
    icg::anim::StrokeOutlineOptions opts;
    opts.width = shape.strokeWidth;
    opts.cap = shape.cap;
    opts.join = shape.join;
    opts.tolerance = tolerance;
    const icg::anim::FlatPath pieces =
        icg::anim::StrokeToPieces(shape.path, opts);
    QPainterPath path;
    path.setFillRule(Qt::WindingFill);
    for (const auto& polyline : pieces.polylines) {
        if (polyline.size() < 3) {
            continue;
        }
        bool first = true;
        for (const Vec2& point : polyline) {
            const Vec2 stageVec = icg::anim::TransformPoint(shape.matrix, point);
            if (first) {
                path.moveTo(stageVec.x, stageVec.y);
                first = false;
            } else {
                path.lineTo(stageVec.x, stageVec.y);
            }
        }
        path.closeSubpath();
    }
    return path;
}

// The single outline loop, for stroking the selection highlight (winding
// never applies to a stroked highlight, so the loop's self-overlap holes
// cannot show there).
QPainterPath buildOutlinePath(const ResolvedShape& shape, float tolerance) {
    icg::anim::StrokeOutlineOptions opts;
    opts.width = shape.strokeWidth;
    opts.cap = shape.cap;
    opts.join = shape.join;
    opts.tolerance = tolerance;
    const icg::anim::FlatPath outline =
        icg::anim::StrokeToOutline(shape.path, opts);
    QPainterPath path;
    for (const auto& polyline : outline.polylines) {
        if (polyline.size() < 3) {
            continue;
        }
        bool first = true;
        for (const Vec2& point : polyline) {
            const Vec2 stageVec = icg::anim::TransformPoint(shape.matrix, point);
            if (first) {
                path.moveTo(stageVec.x, stageVec.y);
                first = false;
            } else {
                path.lineTo(stageVec.x, stageVec.y);
            }
        }
        path.closeSubpath();
    }
    return path;
}

} // namespace

// ------------------------------------------------------- scene baking --
//
// The Part 0 fix in one place: committed shapes live in `sceneCache_` (a
// stage-space pixmap), so a repaint blits instead of re-filling thousands of
// vector subpaths. Freshness comes from exactly one path: ensureSceneBaked(),
// called at the top of every paintEvent, diffs the current draw list against
// what the pixmap shows (`lastBaked_`) and repaints incrementally. Small sets
// apply synchronously; anything big goes chunked on idle (bakeQueue_ +
// bakeTimer_) so a 5000-stroke load stays interactive instead of freezing.

namespace {

// Pixmap coverage for the current scene: the stage plus every visible shape's
// halo-inflated bounds, so off-stage artwork (visible while editing) bakes
// too. Returns false when there is nothing to cover.
bool sceneCoverage(const std::vector<ResolvedShape>& shapes, float stageWidth,
                   float stageHeight, QRectF& boundsOut) {
    bool any = false;
    QRectF bounds(0.0, 0.0, stageWidth, stageHeight);
    for (const ResolvedShape& shape : shapes) {
        icg::anim::Vec2 lo, hi;
        icg::anim::FlatTransformedBounds(shape.path, shape.matrix,
                                        shape.strokeWidth * 0.5f + 1.0f, lo,
                                        hi);
        if (hi.x < lo.x || hi.y < lo.y) {
            continue;
        }
        const QRectF box(lo.x, lo.y, hi.x - lo.x, hi.y - lo.y);
        bounds = any ? bounds.united(box) : box.united(bounds);
        any = true;
    }
    boundsOut = bounds;
    return true; // the stage rect alone is always coverable
}

} // namespace

void AnimatorCanvas::ensureSceneBaked() {
    if (!sceneDirty_) {
        return;
    }
    sceneDirty_ = false;
    if (document_ == nullptr) {
        sceneCache_ = QPixmap();
        lastBaked_.clear();
        bakeQueue_.clear();
        return;
    }
    const std::vector<ResolvedShape>& shapes = drawList();
    const icg::anim::AnimDocument& model = document_->document();

    // Coverage: stage plus current content. Growing past the pixmap (drawing
    // outside previous extents) needs a full re-bake at the new coverage.
    QRectF coverage;
    sceneCoverage(shapes, model.stageWidth, model.stageHeight, coverage);
    const QSize wantSize(
        std::max(1, static_cast<int>(std::ceil(coverage.width() *
                                              sceneBakeScale_))),
        std::max(1, static_cast<int>(std::ceil(coverage.height() *
                                              sceneBakeScale_))));
    bool sizeOk = !sceneCache_.isNull() && sceneCache_.size() == wantSize &&
                  sceneOrigin_ == coverage.topLeft();
    if (!sizeOk) {
        // Coverage grew past the pixmap (drawing outside previous extents).
        // The old pixels are still valid, so keep them: realloc, copy across,
        // and queue only what is missing.
        requestFullBake(true);
        return;
    }

    // Diff current ids against what the pixmap shows. Dragged shapes are
    // skipped: they paint live until commit, and their baked entries still
    // describe the pre-drag state the pixmap shows (ghosting, by design).
    QSet<uint64_t> dragging;
    if (drag_.active) {
        for (const auto& move : drag_.moves) {
            dragging.insert(move.shapeId);
        }
    }
    QHash<uint64_t, const ResolvedShape*> current;
    for (const ResolvedShape& shape : shapes) {
        current.insert(shape.shapeId, &shape);
    }
    std::vector<uint64_t> added;
    std::vector<uint64_t> changed;
    std::vector<uint64_t> removed;
    for (auto it = current.begin(); it != current.end(); ++it) {
        if (dragging.contains(it.key())) {
            continue;
        }
        auto baked = lastBaked_.find(it.key());
        if (baked == lastBaked_.end()) {
            added.push_back(it.key());
        } else if (!(baked->matrix == it.value()->matrix)) {
            changed.push_back(it.key());
        }
    }
    for (auto it = lastBaked_.begin(); it != lastBaked_.end(); ++it) {
        if (!current.contains(it.key())) {
            removed.push_back(it.key());
        }
    }
    // lastBaked_ can also hold dragged ids from before the drag began; they
    // stay valid (pre-drag state) and are simply not touched until commit.
    if (added.empty() && changed.empty() && removed.empty()) {
        return;
    }
    static constexpr size_t kSyncIdLimit = 64;
    if (added.size() + changed.size() + removed.size() > kSyncIdLimit) {
        if (!changed.empty() || !removed.empty()) {
            // Stale regions plus many missing ids: progressive full rebuild
            // (old pixels stay visible, soft, until slices replace them).
            requestFullBake(false);
            return;
        }
        // Added-only flood (paste/load of hundreds of shapes): keep the valid
        // pixmap and queue the new ids for progressive chunked baking. Dedupe
        // against ids already queued: without this every paint re-queues the
        // same flood and the queue grows without bound (each paint appends
        // while slices drain far slower).
        QSet<uint64_t> queued(bakeQueue_.begin(), bakeQueue_.end());
        for (uint64_t id : added) {
            if (!queued.contains(id)) {
                bakeQueue_.push_back(id);
                queued.insert(id);
            }
        }
        ++bakeGeneration_;
        if (!bakeTimer_.isActive()) {
            bakeTimer_.start();
        }
        if (bakeQueue_.size() > 256) {
            reportStatus(tr("Baking scene (%1 shapes)…").arg(bakeQueue_.size()));
            bakeReported_ = true;
        }
        return;
    }
    // Small change: apply synchronously. Added shapes paint straight onto the
    // pixmap; removed/moved shapes need their regions erased and repainted.
    if (!added.empty()) {
        bakeShapesIntoPixmap(added);
        for (uint64_t id : added) {
            auto found = current.find(id);
            if (found != current.end()) {
                lastBaked_.insert(id, bakedStateFor(**found));
            }
        }
    }
    if (!changed.empty() || !removed.empty()) {
        QRectF dirty;
        bool hasDirty = false;
        auto grow = [&](const QRectF& box) {
            dirty = hasDirty ? dirty.united(box) : box;
            hasDirty = true;
        };
        QSet<uint64_t> skip(added.begin(), added.end());
        for (uint64_t id : changed) {
            auto baked = lastBaked_.find(id);
            if (baked != lastBaked_.end()) {
                grow(baked->bounds);
            }
            auto found = current.find(id);
            if (found != current.end()) {
                grow(shapeBounds(**found));
            }
        }
        for (uint64_t id : removed) {
            auto baked = lastBaked_.find(id);
            if (baked != lastBaked_.end()) {
                grow(baked->bounds);
            }
            lastBaked_.remove(id);
        }
        if (hasDirty) {
            rebakeRegion(dirty, skip);
        }
        for (uint64_t id : changed) {
            auto found = current.find(id);
            if (found != current.end()) {
                lastBaked_.insert(id, bakedStateFor(**found));
            }
        }
    }
}

AnimatorCanvas::BakedState AnimatorCanvas::bakedStateFor(
    const ResolvedShape& shape) {
    BakedState state;
    state.bounds = shapeBounds(shape);
    state.matrix = shape.matrix;
    return state;
}

QRectF AnimatorCanvas::shapeBounds(const ResolvedShape& shape) {
    icg::anim::Vec2 lo, hi;
    icg::anim::FlatTransformedBounds(shape.path, shape.matrix,
                                    shape.strokeWidth * 0.5f + 1.0f, lo, hi);
    if (hi.x < lo.x || hi.y < lo.y) {
        return QRectF();
    }
    return QRectF(lo.x, lo.y, hi.x - lo.x, hi.y - lo.y);
}

// Paints drawList entries for `ids` (in z-order) onto the scene pixmap.
// Paths are built transiently and discarded: nothing per-shape persists.
void AnimatorCanvas::bakeShapesIntoPixmap(const std::vector<uint64_t>& ids) {
    if (sceneCache_.isNull() || ids.empty()) {
        return;
    }
    QSet<uint64_t> wanted(ids.begin(), ids.end());
    QPainter painter(&sceneCache_);
    painter.setTransform(pixmapTransform());
    painter.setRenderHint(QPainter::Antialiasing,
                          quality_ != PreviewQuality::Draft);
    painter.setPen(Qt::NoPen);
    for (const ResolvedShape& shape : drawList()) {
        if (!wanted.contains(shape.shapeId)) {
            continue;
        }
        if (shape.hasFill && shape.fill.a > 0) {
            painter.fillPath(buildFillPath(shape), toQColor(shape.fill));
        }
        if (shape.hasStroke && shape.stroke.a > 0 && shape.strokeWidth > 0.0) {
            painter.fillPath(buildPiecesPath(shape, flattenTolerance()),
                             toQColor(shape.stroke));
        }
    }
}

// Erases `stageRect` on the pixmap, then repaints every current shape
// intersecting it (except `skipIds`, painted separately) back-to-front.
void AnimatorCanvas::rebakeRegion(const QRectF& stageRect,
                                 const QSet<uint64_t>& skipIds) {
    if (sceneCache_.isNull()) {
        return;
    }
    // Pixmap-space rect for the stage-space dirty region. The pad scales with
    // the bake resolution: antialiasing fringe is ~1 stage unit wide, which is
    // more device pixels the higher the bake scale. Too small a pad leaves
    // fringe pixels behind on erase (measured as residue after undo).
    const int pad = static_cast<int>(std::ceil(sceneBakeScale_)) + 2;
    const QRect erase = QRect(
        static_cast<int>(std::floor((stageRect.left() - sceneOrigin_.x()) *
                                    sceneBakeScale_)) -
            pad,
        static_cast<int>(std::floor((stageRect.top() - sceneOrigin_.y()) *
                                    sceneBakeScale_)) -
            pad,
        static_cast<int>(std::ceil(stageRect.width() * sceneBakeScale_)) +
            pad * 2,
        static_cast<int>(std::ceil(stageRect.height() * sceneBakeScale_)) +
            pad * 2)
                            .intersected(sceneCache_.rect());
    {
        QPainter clear(&sceneCache_);
        clear.setCompositionMode(QPainter::CompositionMode_Clear);
        clear.fillRect(erase, Qt::transparent);
    }
    QPainter painter(&sceneCache_);
    painter.setTransform(pixmapTransform());
    painter.setRenderHint(QPainter::Antialiasing,
                          quality_ != PreviewQuality::Draft);
    painter.setPen(Qt::NoPen);
    // Clip to the dirty region (expanded slightly): shapes outside cannot
    // contribute a pixel, so dense scenes rebake locally, not globally.
    painter.setClipRect(stageRect.adjusted(-2.0, -2.0, 2.0, 2.0));
    for (const ResolvedShape& shape : drawList()) {
        if (skipIds.contains(shape.shapeId)) {
            continue;
        }
        if (!shapeBounds(shape).intersects(stageRect)) {
            continue;
        }
        if (shape.hasFill && shape.fill.a > 0) {
            painter.fillPath(buildFillPath(shape), toQColor(shape.fill));
        }
        if (shape.hasStroke && shape.stroke.a > 0 && shape.strokeWidth > 0.0) {
            painter.fillPath(buildPiecesPath(shape, flattenTolerance()),
                             toQColor(shape.stroke));
        }
    }
}

void AnimatorCanvas::requestFullBake(bool trustBaked) {
    if (document_ == nullptr) {
        sceneCache_ = QPixmap();
        lastBaked_.clear();
        bakeQueue_.clear();
        return;
    }
    const std::vector<ResolvedShape>& shapes = drawList();
    const icg::anim::AnimDocument& model = document_->document();
    QRectF coverage;
    sceneCoverage(shapes, model.stageWidth, model.stageHeight, coverage);
    const QSize wantSize(
        std::max(1, static_cast<int>(std::ceil(coverage.width() *
                                              sceneBakeScale_))),
        std::max(1, static_cast<int>(std::ceil(coverage.height() *
                                              sceneBakeScale_))));
    // Realloc only when the size or origin actually changed; copy the old
    // pixels across scaled so the scene never blanks (soft during progressive
    // catch-up, then crisp as slices land). Quality/scale changes clear the
    // baked record (tessellation differs) but still keep the soft image.
    QPixmap fresh(wantSize);
    fresh.fill(Qt::transparent);
    if (!sceneCache_.isNull()) {
        QPainter carry(&fresh);
        carry.drawPixmap(fresh.rect(), sceneCache_, sceneCache_.rect());
    }
    sceneCache_ = fresh;
    sceneOrigin_ = coverage.topLeft();
    if (!trustBaked) {
        lastBaked_.clear();
    }
    bakeQueue_.clear();
    bakeQueue_.reserve(shapes.size());
    for (const ResolvedShape& shape : shapes) {
        if (trustBaked && lastBaked_.contains(shape.shapeId)) {
            continue;
        }
        bakeQueue_.push_back(shape.shapeId);
    }
    ++bakeGeneration_;
    // Small scenes bake synchronously, right here: no timer round-trip, so a
    // first paint (or a synchronous grab, or a future frame export) never
    // observes a half-baked pixmap. Big scenes stay chunked on idle.
    static constexpr size_t kSyncBakeLimit = 64;
    if (bakeQueue_.size() > kSyncBakeLimit) {
        reportStatus(tr("Baking scene (%1 shapes)…").arg(bakeQueue_.size()));
        bakeReported_ = true;
        if (!bakeTimer_.isActive()) {
            bakeTimer_.start();
        }
        return;
    }
    while (bakeSliceOnce(12)) {
    }
}

void AnimatorCanvas::onBakeSlice() {
    if (bakeSliceOnce(12) && !bakeTimer_.isActive()) {
        bakeTimer_.start();
    }
}

// Bakes queued shapes for up to `budgetMs`, then returns whether work
// remains. Stale generations abort silently: whoever superseded the bake owns
// freshness now (the diff path in ensureSceneBaked).
bool AnimatorCanvas::bakeSliceOnce(int budgetMs) {
    const uint64_t generation = bakeGeneration_;
    if (bakeQueue_.empty()) {
        if (bakeReported_) {
            bakeReported_ = false;
            reportStatus(tr("Scene ready."));
        }
        return false;
    }
    QElapsedTimer slice;
    slice.start();
    std::vector<uint64_t> done;
    while (!bakeQueue_.empty() &&
           slice.nsecsElapsed() < static_cast<qint64>(budgetMs) * 1000000LL) {
        if (generation != bakeGeneration_) {
            return !bakeQueue_.empty();
        }
        done.push_back(bakeQueue_.back());
        bakeQueue_.pop_back();
    }
    if (!done.empty()) {
        bakeShapesIntoPixmap(done);
        const std::vector<ResolvedShape>& shapes = drawList();
        QHash<uint64_t, const ResolvedShape*> current;
        for (const ResolvedShape& shape : shapes) {
            current.insert(shape.shapeId, &shape);
        }
        for (uint64_t id : done) {
            auto found = current.find(id);
            if (found != current.end()) {
                lastBaked_.insert(id, bakedStateFor(**found));
            }
        }
        update(); // show progress: the fresh strip paints on the next frame
    }
    return !bakeQueue_.empty();
}

void AnimatorCanvas::maybeAdaptBakeScale() {
    if (sceneCache_.isNull()) {
        return;
    }
    // Zooming far past the baked resolution (or far below it) re-bakes at a
    // new scale on idle. Pan never rebakes: the blit sub-rects the pixmap.
    // Between threshold and completion the old pixmap keeps blitting (soft
    // when magnified, never blank).
    if (view_.zoom > sceneBakeScale_ * 2.0f) {
        sceneBakeScale_ *= 2.0f;
        requestFullBake(false);
    } else if (view_.zoom < sceneBakeScale_ / 8.0f && sceneBakeScale_ > 1.0f) {
        sceneBakeScale_ *= 0.5f;
        requestFullBake(false);
    }
}

QTransform AnimatorCanvas::pixmapTransform() const {
    // Stage -> pixmap device pixels: scale, then shift by the origin.
    return QTransform(sceneBakeScale_, 0.0, 0.0, sceneBakeScale_,
                      -sceneOrigin_.x() * sceneBakeScale_,
                      -sceneOrigin_.y() * sceneBakeScale_);
}

void AnimatorCanvas::paintEvent(QPaintEvent*) {
    // Drop the shared draw list up front: it is viewport-culled, so it must be
    // rebuilt whenever the view moved. Doing it here rather than in every view
    // mutator means the cache cannot go stale through a path that forgot to
    // invalidate it, and it still collapses three rebuilds into one per paint.
    // Single freshness path for the scene pixmap: diffs the draw list against
    // what the pixmap shows and repaints incrementally (small sets sync, big
    // sets chunked on idle). Nothing else touches the pixmap. Skipped outright
    // when nothing model-side changed since the last paint.
    ensureSceneBaked();
    maybeAdaptBakeScale();
    QPainter painter(this);
    // Draft skips antialiasing (the biggest fill-rate lever); Normal and High
    // smooth. Set once here so every painter in the pass inherits it.
    painter.setRenderHint(QPainter::Antialiasing,
                          quality_ != PreviewQuality::Draft);
    paintBackdrop(painter);
    if (document_ == nullptr) {
        return;
    }

    painter.save();
    // Stage units -> widget pixels. y-down needs no flip: Qt's y grows downward
    // too, so this is a plain scale.
    painter.translate(view_.offset.x(), view_.offset.y());
    painter.scale(view_.zoom, view_.zoom);

    paintStageFill(painter);
    paintSceneBlit(painter);
    paintLiveSelection(painter);
    paintSelectionOutlines(painter);
    // Tool overlays (marquee, brush preview) draw last, in stage space.
    if (activeTool_ != nullptr) {
        activeTool_->paintOverlay(painter, *this);
    }
    painter.restore();

    // The stage border and the "outside the stage" dimming are drawn in widget
    // space so their line width stays constant at any zoom.
    paintStageOutline(painter);
    paintEmptyHint(painter);
}

// Blits the baked scene pixmap for the visible stage rect. The pixmap is
// stage-space, so pan is a sub-rect and zoom is a scaled blit - neither
// re-rasterizes a single vector subpath.
void AnimatorCanvas::paintSceneBlit(QPainter& painter) const {
    if (sceneCache_.isNull()) {
        return;
    }
    const QRectF visible = visibleStageRect();
    const QRectF source((visible.left() - sceneOrigin_.x()) * sceneBakeScale_,
                        (visible.top() - sceneOrigin_.y()) * sceneBakeScale_,
                        visible.width() * sceneBakeScale_,
                        visible.height() * sceneBakeScale_);
    // Smooth only when magnifying past the baked resolution; 1:1 blits stay
    // crisp with no filtering cost.
    painter.setRenderHint(QPainter::SmoothPixmapTransform,
                          view_.zoom > sceneBakeScale_);
    painter.drawPixmap(visible, sceneCache_, source);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, false);
}

// Live vectors for SELECTED shapes (and, during a drag, the dragged ones -
// which are always selected). Everything else comes from the pixmap.
// Selected shapes paint live because their highlight must track the cursor
// exactly; during a drag the pixmap still shows their pre-drag position,
// which reads as intentional ghosting until commit rebakes the region.
void AnimatorCanvas::paintLiveSelection(QPainter& painter) const {
    if (selection_.isEmpty()) {
        return;
    }
    painter.save();
    painter.setPen(Qt::NoPen);
    for (const ResolvedShape& shape : visibleList()) {
        if (!selection_.contains(shape.shapeId)) {
            continue;
        }
        // Skip shapes the pixmap already shows correctly: baked with the
        // current matrix and not being dragged. (During a drag the pixmap
        // holds the stale position, so the live paint is the move feedback.)
        bool dragging = false;
        if (drag_.active) {
            for (const auto& move : drag_.moves) {
                if (move.shapeId == shape.shapeId) {
                    dragging = true;
                    break;
                }
            }
        }
        if (!dragging) {
            auto baked = lastBaked_.find(shape.shapeId);
            if (baked != lastBaked_.end() && baked->matrix == shape.matrix) {
                continue;
            }
        }
        if (shape.hasFill && shape.fill.a > 0) {
            painter.fillPath(buildFillPath(shape), toQColor(shape.fill));
        }
        if (shape.hasStroke && shape.stroke.a > 0 && shape.strokeWidth > 0.0) {
            painter.fillPath(buildPiecesPath(shape, flattenTolerance()),
                             toQColor(shape.stroke));
        }
    }
    painter.restore();
}

void AnimatorCanvas::paintBackdrop(QPainter& painter) const {
    // Slightly darker than the widget background so the stage reads as the
    // page, and outside-stage content is visibly outside it.
    painter.fillRect(rect(), palette().window().color().darker(130));
}

void AnimatorCanvas::paintStageFill(QPainter& painter) const {
    const icg::anim::AnimDocument& model = document_->document();
    const QRectF stageRect(0.0, 0.0, model.stageWidth, model.stageHeight);

    painter.save();
    painter.setClipRect(stageRect);
    if (model.transparentBackground || model.background.a == 0) {
        // Fixed 8px cells in WIDGET space, clipped to the stage: the cell
        // count is bounded by the viewport at any zoom, unlike stage-space
        // cells which explode into hundreds of thousands of rects zoomed in.
        painter.save();
        painter.resetTransform();
        const QPointF stageTopLeft = view_.toWidget(QPointF(0.0, 0.0));
        const QPointF stageBottomRight = view_.toWidget(
            QPointF(model.stageWidth, model.stageHeight));
        const QRectF stageWidget(stageTopLeft, stageBottomRight);
        painter.setClipRect(stageWidget.intersected(QRectF(rect())));
        constexpr double kCell = 8.0;
        const int light = palette().color(QPalette::Light).darker(105).rgb();
        const int dark = palette().color(QPalette::Mid).rgb();
        const int x0 = static_cast<int>(std::floor(stageWidget.left() / kCell));
        const int x1 = static_cast<int>(std::ceil(stageWidget.right() / kCell));
        const int y0 = static_cast<int>(std::floor(stageWidget.top() / kCell));
        const int y1 =
            static_cast<int>(std::ceil(stageWidget.bottom() / kCell));
        for (int y = y0; y < y1; ++y) {
            for (int x = x0; x < x1; ++x) {
                // Wrapped modulo keeps the pattern stable for negative cells
                // (panned off-stage top/left).
                const bool even = (((x + y) % 2) + 2) % 2 == 0;
                painter.fillRect(QRectF(x * kCell, y * kCell, kCell, kCell),
                                 QColor(even ? light : dark));
            }
        }
        painter.restore();
    } else {
        painter.fillRect(stageRect, toQColor(model.background));
    }
    painter.restore();
}

void AnimatorCanvas::paintSelectionOutlines(QPainter& painter) const {
    if (selection_.isEmpty()) {
        return;
    }
    painter.save();
    QPen pen(QColor(60, 140, 255));
    pen.setStyle(Qt::DashLine);
    pen.setCosmetic(true); // constant width regardless of zoom
    painter.setPen(pen);
    painter.setBrush(Qt::NoBrush);
    // Shares this paint's culled list instead of rebuilding its own.
    for (const ResolvedShape& shape : visibleList()) {
        if (!selection_.contains(shape.shapeId)) {
            continue;
        }
        // Strokes highlight by tracing the rendered BAND, not the centerline,
        // so the highlight sits exactly on the pixels the user sees (stroking
        // the loop has no winding issue - only fills do). The loop is built
        // transiently: only selected shapes pay for it, so it never shows up
        // in scene-wide costs. Pure fills skip the path trace and use the box
        // and handles below as their whole highlight.
        if (shape.hasStroke) {
            const QPainterPath band =
                buildOutlinePath(shape, flattenTolerance());
            if (!band.isEmpty()) {
                painter.drawPath(band);
            } else {
                painter.drawPath(buildFillPath(shape));
            }
        }
        // Bounding box + corner handles, so a move has an obvious affordance.
        // Computed from the EXACT path and inflated by the stroke halo only, so
        // the handles sit exactly on the rendered pixels.
        icg::anim::Vec2 blo, bhi;
        icg::anim::FlatTransformedBounds(shape.path, shape.matrix,
                                        shape.strokeWidth * 0.5f, blo, bhi);
        if (bhi.x < blo.x || bhi.y < blo.y) {
            continue;
        }
        const QPointF corners[4] = {
            QPointF(blo.x, blo.y), QPointF(bhi.x, blo.y),
            QPointF(bhi.x, bhi.y), QPointF(blo.x, bhi.y),
        };
        for (const QPointF& corner : corners) {
            painter.drawRect(QRectF(corner.x() - 3.0, corner.y() - 3.0, 6.0, 6.0));
        }
    }
    painter.restore();
}

void AnimatorCanvas::paintStageOutline(QPainter& painter) const {
    const icg::anim::AnimDocument& model = document_->document();
    const QPointF topLeft = view_.toWidget(QPointF(0.0, 0.0));
    const QPointF bottomRight =
        view_.toWidget(QPointF(model.stageWidth, model.stageHeight));
    const QRectF stageRect(topLeft, bottomRight);

    // Dim what is outside the stage so off-stage artwork stays VISIBLE while
    // editing (it is clipped only on export and in-game) but is obviously not
    // part of the frame.
    painter.save();
    QRegion widgetRegion(rect());
    QRegion stageRegion(stageRect.toRect());
    painter.setClipRegion(widgetRegion - stageRegion);
    painter.fillRect(rect(), QColor(0, 0, 0, 110));
    painter.restore();

    QPen pen(QColor(120, 120, 120));
    pen.setStyle(Qt::DashLine);
    pen.setCosmetic(true);
    painter.setPen(pen);
    painter.setBrush(Qt::NoBrush);
    painter.drawRect(stageRect);
}

void AnimatorCanvas::paintEmptyHint(QPainter& painter) const {
    if (selectionCount() > 0 || !isEditable()) {
        return;
    }
    // Cheap path first: anything on screen means there is art to edit. Only if
    // the culled list is empty do we pay for a full build to tell "nothing
    // drawn anywhere" from "everything drawn off-screen".
    if (!visibleList().empty()) {
        return;
    }
    if (!drawList().empty()) {
        return;
    }
    // Only nag when there is genuinely nothing to edit; a locked or hidden
    // layer is a normal state, not an error worth a message on every repaint.
    painter.setPen(palette().color(QPalette::Mid));
    painter.drawText(rect(), Qt::AlignCenter,
                     tr("This layer is locked or hidden - nothing to select."));
}

// ---------------------------------------------------- tools / view glue --

void AnimatorCanvas::setActiveTool(ITool* tool) {
    if (tool == nullptr || tool == activeTool_) {
        return;
    }
    if (activeTool_ != nullptr) {
        // Lets a tool drop a half-finished marquee or drag instead of leaking it
        // into the next one.
        activeTool_->onDeactivated(*this);
    }
    activeTool_ = tool;
    if (activeTool_ != nullptr) {
        activeTool_->onActivated(*this);
    }
    setToolCursor();
    update();
}

void AnimatorCanvas::setActiveTool(const std::string& id) {
    setActiveTool(tools_.find(id));
}

HandTool* AnimatorCanvas::handTool() const {
    return handTool_;
}

void AnimatorCanvas::setSpaceHeld(bool held) {
    if (spaceHeld_ == held) {
        return;
    }
    spaceHeld_ = held;
    if (held) {
        // Remember what to come back to; handTool() is null only if the tool set
        // failed to build, in which case there is nothing to switch to.
        toolBeforeSpace_ = activeTool_;
        if (handTool_ != nullptr) {
            setActiveTool(handTool_);
        }
    } else {
        setActiveTool(toolBeforeSpace_ != nullptr ? toolBeforeSpace_
                                                  : activeTool_);
        toolBeforeSpace_ = nullptr;
    }
}

void AnimatorCanvas::setToolCursor() {
    if (handTool_ != nullptr && activeTool_ == handTool_) {
        // OpenHand while idle; HandTool switches to ClosedHand during a drag.
        setCursor(Qt::OpenHandCursor);
        return;
    }
    setCursor(activeTool_ != nullptr ? activeTool_->cursor() : Qt::ArrowCursor);
}

void AnimatorCanvas::panBy(const QPointF& deltaWidget) {
    view_.panByPixels(deltaWidget);
    bumpView();
    update();
}

bool AnimatorCanvas::isSceneBaked() const {
    return !sceneCache_.isNull() && bakeQueue_.empty();
}

void AnimatorCanvas::updateViewAfterPan() {
    emitZoomChanged();
    update();
}

void AnimatorCanvas::deleteSelection() {
    if (document_ == nullptr || selection_.isEmpty()) {
        return;
    }
    if (!isEditable()) {
        reportStatus(tr("Layer is locked - nothing deleted."));
        return;
    }
    const uint64_t layerId = activeLayerId();
    const int frame = currentFrame_;
    const std::vector<uint64_t> ids(selection_.begin(), selection_.end());
    if (document_->deleteShapes(layerId, frame, ids)) {
        // pruneSelection drops the now-deleted ids and repaints.
        reportStatus(tr("Deleted %n shape(s)", nullptr, ids.size()));
    }
}

void AnimatorCanvas::reportStatus(const QString& text) {
    emit statusMessage(text);
}

// ------------------------------------------------------- drawing options --

void AnimatorCanvas::setStrokeWidth(float width) {
    if (width < 0.5f) {
        width = 0.5f;
    }
    if (width > 100.0f) {
        width = 100.0f;
    }
    drawOptions_.strokeWidth = width;
    update();
}

void AnimatorCanvas::setStrokeColor(const icg::anim::AnimColor& color) {
    drawOptions_.strokeColor = color;
    update();
}

void AnimatorCanvas::setStrokeOpacity(float opacity) {
    if (opacity < 0.05f) {
        opacity = 0.05f;
    }
    if (opacity > 1.0f) {
        opacity = 1.0f;
    }
    drawOptions_.opacity = opacity;
    update();
}

void AnimatorCanvas::setSmoothing(float tolerance) {
    if (tolerance < 0.0f) {
        tolerance = 0.0f;
    }
    if (tolerance > 8.0f) {
        tolerance = 8.0f;
    }
    drawOptions_.smoothing = tolerance;
}

void AnimatorCanvas::setFillColor(const icg::anim::AnimColor& color) {
    drawOptions_.fillColor = color;
}

uint64_t AnimatorCanvas::addDrawnShape(icg::anim::AnimPath path,
                                      icg::anim::AnimStyle style,
                                      const std::string& name) {
    if (document_ == nullptr || path.IsEmpty()) {
        return 0;
    }
    if (!isEditable()) {
        reportStatus(tr("Layer is locked - nothing drawn."));
        return 0;
    }
    const uint64_t layerId = activeLayerId();
    if (layerId == 0) {
        reportStatus(tr("No visible layer - nothing drawn."));
        return 0;
    }
    const uint64_t id =
        document_->addDrawnShape(layerId, currentFrame_, std::move(path), style, name);
    if (id == 0) {
        return 0;
    }
    // Select the new stroke so the user gets immediate feedback and can move
    // it straight away with the Selection tool.
    setSelection(QSet<uint64_t>{id});
    return id;
}

// ---------------------------------------------------------------- events --

void AnimatorCanvas::mousePressEvent(QMouseEvent* event) {
    setFocus(Qt::MouseFocusReason);
    // Middle-drag always pans whatever the active tool: the universal escape
    // hatch when a tool's modifier is unclear.
    if (event->button() == Qt::MiddleButton && handTool_ != nullptr &&
        handTool_->onPress(*this, *event)) {
        return;
    }
    if (event->button() == Qt::LeftButton && activeTool_ != nullptr &&
        activeTool_->onPress(*this, *event)) {
        return;
    }
    QWidget::mousePressEvent(event);
}

void AnimatorCanvas::mouseMoveEvent(QMouseEvent* event) {
    if (activeTool_ != nullptr && activeTool_->onMove(*this, *event)) {
        return;
    }
    QWidget::mouseMoveEvent(event);
}

void AnimatorCanvas::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() == Qt::MiddleButton && handTool_ != nullptr &&
        handTool_->onRelease(*this, *event)) {
        return;
    }
    if (event->button() == Qt::LeftButton && activeTool_ != nullptr &&
        activeTool_->onRelease(*this, *event)) {
        return;
    }
    QWidget::mouseReleaseEvent(event);
}

void AnimatorCanvas::mouseDoubleClickEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton && activeTool_ != nullptr &&
        activeTool_->onDoubleClick(*this, *event)) {
        return;
    }
    QWidget::mouseDoubleClickEvent(event);
}

void AnimatorCanvas::wheelEvent(QWheelEvent* event) {
    if (activeTool_ != nullptr && activeTool_->onWheel(*this, *event)) {
        event->accept();
        return;
    }
    const QPoint delta = event->angleDelta();
    if (delta.y() == 0) {
        QWidget::wheelEvent(event);
        return;
    }
    // Scroll up zooms in, matching Studio's scene canvas.
    const float factor =
        delta.y() > 0 ? kWheelZoomStep : 1.0f / kWheelZoomStep;
    view_.zoomAt(factor, event->position());
    bumpView();
    emitZoomChanged();
    update();
    event->accept();
}

void AnimatorCanvas::keyPressEvent(QKeyEvent* event) {
    // Space is the temporary Hand modifier, handled before the active tool so a
    // tool never sees a Space it should ignore.
    if (event->key() == Qt::Key_Space && !event->isAutoRepeat()) {
        setSpaceHeld(true);
        event->accept();
        return;
    }
    if (activeTool_ != nullptr && activeTool_->onKey(*this, *event)) {
        event->accept();
        return;
    }
    QWidget::keyPressEvent(event);
}

void AnimatorCanvas::keyReleaseEvent(QKeyEvent* event) {
    if (event->key() == Qt::Key_Space && !event->isAutoRepeat()) {
        setSpaceHeld(false);
        event->accept();
        return;
    }
    QWidget::keyReleaseEvent(event);
}

void AnimatorCanvas::focusOutEvent(QFocusEvent* event) {
    // Losing focus with Space held would strand the canvas in pan mode with no
    // key release ever arriving.
    setSpaceHeld(false);
    QWidget::focusOutEvent(event);
}

