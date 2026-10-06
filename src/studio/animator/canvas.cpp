#include "canvas.h"

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
        // the flattening cache and the selection both need revalidating.
        connect(document_, &AnimatorDocument::documentChanged, this, [this] {
            pathCache_.clear();
            pruneSelection();
            update();
        });
    }
}

void AnimatorCanvas::setView(const StageView& view) {
    view_ = view;
    emitZoomChanged();
    update();
}

void AnimatorCanvas::zoomBy(double factor) {
    view_.zoomAt(static_cast<float>(factor), QPointF(width() * 0.5, height() * 0.5));
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

void AnimatorCanvas::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
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
    update();
}

// ----------------------------------------------------------- draw / pick --

const icg::anim::FlatPath& AnimatorCanvas::flattenedPath(
    const AnimShape& shape) const {
    auto cached = pathCache_.find(shape.id);
    if (cached != pathCache_.end()) {
        return *cached;
    }
    return *pathCache_.insert(shape.id, icg::anim::Flatten(shape.path, kFlattenTol));
}

std::vector<ResolvedShape> AnimatorCanvas::drawList() const {
    std::vector<ResolvedShape> out;
    if (document_ == nullptr) {
        return out;
    }
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
            // Style is cheap (matrix + color mults); the subdivision reuses the
            // cache, so a repaint costs O(shapes), not O(segments).
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
            out.push_back(std::move(resolved));
        }
    }
    // Layers are stored top-first; painting must run bottom-first.
    std::reverse(out.begin(), out.end());
    return out;
}

uint64_t AnimatorCanvas::hitTest(const QPointF& stagePos,
                                 double toleranceStage) const {
    const auto shapes = drawList();
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
        icg::anim::Vec2 lo, hi;
        icg::anim::FlatBounds(shape.path, lo, hi);
        bool empty = false;
        if (hi.x < lo.x || hi.y < lo.y) {
            empty = true;
        }
        if (empty) {
            continue;
        }
        // Transform the local box's corners into stage space (handles
        // rotation/skew) and test containment against the marquee.
        const Vec2 corners[4] = {
            icg::anim::TransformPoint(shape.matrix, lo),
            icg::anim::TransformPoint(shape.matrix, Vec2(hi.x, lo.y)),
            icg::anim::TransformPoint(shape.matrix, hi),
            icg::anim::TransformPoint(shape.matrix, Vec2(lo.x, hi.y)),
        };
        bool allInside = true;
        for (const Vec2& corner : corners) {
            if (!stageRect.contains(QPointF(corner.x, corner.y))) {
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
    update();
}

// ------------------------------------------------------------- painting --

QPainterPath AnimatorCanvas::toPainterPath(const ResolvedShape& shape) const {
    QPainterPath path;
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

void AnimatorCanvas::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    paintBackdrop(painter);
    if (document_ == nullptr) {
        return;
    }
    const icg::anim::AnimDocument& model = document_->document();

    painter.save();
    // Stage units -> widget pixels. y-down needs no flip: Qt's y grows downward
    // too, so this is a plain scale.
    painter.translate(view_.offset.x(), view_.offset.y());
    painter.scale(view_.zoom, view_.zoom);

    paintStageFill(painter);
    paintShapes(painter);
    paintSelectionOutlines(painter);
    // Tool overlays (the marquee) draw last, in stage space.
    if (activeTool_ != nullptr) {
        activeTool_->paintOverlay(painter, *this);
    }
    painter.restore();

    // The stage border and the "outside the stage" dimming are drawn in widget
    // space so their line width stays constant at any zoom.
    paintStageOutline(painter);
    paintEmptyHint(painter);
}

void AnimatorCanvas::paintBackdrop(QPainter& painter) const {
    // Slightly darker than the widget background so the stage reads as the
    // page, and outside-stage content is visibly outside it.
    painter.fillRect(rect(), palette().window().color().darker(130));
}

void AnimatorCanvas::paintStageFill(QPainter& painter) const {
    const icg::anim::AnimDocument& model = document_->document();
    const QRectF stageRect(0.0, 0.0, model.stageWidth, model.stageHeight);

    // Transparency checkerboard, in stage units but sized in widget units so the
    // cells do not grow with zoom.
    painter.save();
    painter.setClipRect(stageRect);
    if (model.transparentBackground || model.background.a == 0) {
        const double cell = std::max(4.0, 8.0 / std::max(0.01f, view_.zoom));
        const int light = palette().color(QPalette::Light).darker(105).rgb();
        const int dark = palette().color(QPalette::Mid).rgb();
        int row = 0;
        for (double y = std::floor(stageRect.top() / cell);
             y * cell < stageRect.bottom(); ++y, ++row) {
            int column = 0;
            for (double x = std::floor(stageRect.left() / cell);
                 x * cell < stageRect.right(); ++x, ++column) {
                const bool isLight = ((row + column) % 2) == 0;
                painter.fillRect(QRectF(x * cell, y * cell, cell, cell),
                                 QColor(isLight ? light : dark));
            }
        }
    } else {
        painter.fillRect(stageRect, toQColor(model.background));
    }
    painter.restore();
}

void AnimatorCanvas::paintShapes(QPainter& painter) const {
    for (const ResolvedShape& shape : drawList()) {
        const QPainterPath path = toPainterPath(shape);
        if (path.isEmpty()) {
            continue;
        }
        // Fills and strokes are painted in stage space, so the pen width is
        // scaled manually (QPainter scales pen widths too, and ResolveShape
        // already applied the transform scale, so keep the pen unscaled).
        painter.save();
        painter.setPen(Qt::NoPen);
        if (shape.hasFill && shape.fill.a > 0) {
            painter.fillPath(path, toQColor(shape.fill));
        }
        painter.restore();

        if (shape.hasStroke && shape.stroke.a > 0 && shape.strokeWidth > 0.0) {
            QPen pen(toQColor(shape.stroke));
            pen.setWidthF(shape.strokeWidth);
            pen.setCapStyle(shape.cap == icg::anim::LineCap::Butt     ? Qt::FlatCap
                            : shape.cap == icg::anim::LineCap::Round ? Qt::RoundCap
                                                                     : Qt::SquareCap);
            pen.setJoinStyle(shape.join == icg::anim::LineJoin::Miter ? Qt::MiterJoin
                             : shape.join == icg::anim::LineJoin::Round ? Qt::RoundJoin
                                                                      : Qt::BevelJoin);
            // QPainter scales pen widths with the transform, but strokeWidth is
            // already in stage units, so pin the cosmetic width off.
            painter.save();
            painter.setPen(pen);
            painter.setBrush(Qt::NoBrush);
            painter.drawPath(path);
            painter.restore();
        }
    }
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
    for (const ResolvedShape& shape : drawList()) {
        if (!selection_.contains(shape.shapeId)) {
            continue;
        }
        painter.drawPath(toPainterPath(shape));
        // Bounding box + corner handles, so a move has an obvious affordance.
        icg::anim::Vec2 lo, hi;
        icg::anim::FlatBounds(shape.path, lo, hi);
        if (hi.x < lo.x || hi.y < lo.y) {
            continue;
        }
        const Vec2 corners[4] = {
            icg::anim::TransformPoint(shape.matrix, lo),
            icg::anim::TransformPoint(shape.matrix, Vec2(hi.x, lo.y)),
            icg::anim::TransformPoint(shape.matrix, hi),
            icg::anim::TransformPoint(shape.matrix, Vec2(lo.x, hi.y)),
        };
        for (const Vec2& corner : corners) {
            painter.drawRect(QRectF(corner.x - 3.0, corner.y - 3.0, 6.0, 6.0));
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
    if (selectionCount() > 0 || !drawList().empty() || !isEditable()) {
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
    update();
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

