// Incogine Animator - freehand brush tool.
// Part of Incogine by leafstudiosDot (MPL-2.0). See LICENSE.
//
// Drag to paint a stroked path. Raw input points are throttled to ~2 screen
// px, then RDP-simplified and fitted to Beziers on release, so a shaky hand
// produces a clean selectable stroke. A bare click makes a dot.
//
// NOTE: handlers take events by const reference, so members are accessed with
// '.' The '->' operator applies to pointers and objects, not references - which
// is why Qt hands out event pointers while these tools take references.
//
// The canvas header comes first: the tool definitions below call methods on
// AnimatorCanvas, so it must be complete before them, and it pulls in the Qt
// event headers the tools use.
#include "canvas.h"
#include "tools/tools.h"

#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QWidget>

#include <algorithm>

using icg::anim::AnimPath;
using icg::anim::AnimSegment;
using icg::anim::AnimStyle;
using icg::anim::Vec2;

namespace {
// QPointF <-> Vec2, stage space. Kept local: only the tools convert.
Vec2 toVec(const QPointF& p) {
    return Vec2(static_cast<float>(p.x()), static_cast<float>(p.y()));
}
QPointF toPoint(const Vec2& v) {
    return QPointF(v.x, v.y);
}
// Stroke color with the opacity slider applied. Opacity lives separately from
// the picked color so a user can fade a swatch without losing its RGB.
icg::anim::AnimColor styledStroke(const AnimatorCanvas& canvas) {
    const DrawingOptions& options = canvas.drawingOptions();
    icg::anim::AnimColor color = options.strokeColor;
    color.a = static_cast<int>(color.a * options.opacity + 0.5f);
    if (color.a < 0) {
        color.a = 0;
    }
    if (color.a > 255) {
        color.a = 255;
    }
    return color;
}
} // namespace

bool BrushTool::onPress(AnimatorCanvas& canvas, const QMouseEvent& event) {
    if (event.button() != Qt::LeftButton) {
        return false;
    }
    if (!canvas.isEditable()) {
        canvas.reportStatus(
            QObject::tr("Layer is locked - nothing drawn. Unlock it to paint."));
        return true;
    }
    if (canvas.activeLayerId() == 0) {
        canvas.reportStatus(QObject::tr("No visible layer - nothing drawn."));
        return true;
    }
    stroking_ = true;
    raw_.clear();
    raw_.push_back(toVec(canvas.view().toStage(event.position())));
    hasHover_ = true;
    hoverStage_ = canvas.view().toStage(event.position());
    canvas.update();
    return true;
}

bool BrushTool::onMove(AnimatorCanvas& canvas, const QMouseEvent& event) {
    hasHover_ = true;
    hoverStage_ = canvas.view().toStage(event.position());
    if (!stroking_) {
        // Hover only: refresh the brush ring, but do not consume the event.
        canvas.update();
        return false;
    }
    appendIfSpaced(toVec(hoverStage_),
                   2.0f / std::max(0.02f, canvas.view().zoom));
    canvas.update();
    return true;
}

void BrushTool::appendIfSpaced(const Vec2& point, float spacing) {
    if (raw_.empty()) {
        raw_.push_back(point);
        return;
    }
    if (icg::anim::Distance(raw_.back(), point) >= spacing) {
        raw_.push_back(point);
    }
}

bool BrushTool::onRelease(AnimatorCanvas& canvas, const QMouseEvent& event) {
    if (event.button() != Qt::LeftButton || !stroking_) {
        return false;
    }
    stroking_ = false;
    const Vec2 end = toVec(canvas.view().toStage(event.position()));
    appendIfSpaced(end, 0.0f);
    const bool committed = finishStroke(canvas);
    raw_.clear();
    canvas.update();
    return committed;
}

bool BrushTool::finishStroke(AnimatorCanvas& canvas) {
    const DrawingOptions& options = canvas.drawingOptions();
    const icg::anim::AnimColor stroke = styledStroke(canvas);

    if (raw_.empty()) {
        return false;
    }
    // Bare click (or a sub-pixel wiggle): a filled dot in the stroke color, so
    // it reads as a dot rather than a ring. Everything else is a stroked path.
    float total = 0.0f;
    for (size_t i = 1; i < raw_.size(); ++i) {
        total += icg::anim::Distance(raw_[i - 1], raw_[i]);
    }
    if (raw_.size() == 1 || total < options.strokeWidth * 0.25f) {
        const Vec2 center = raw_.front();
        const float radius = std::max(0.5f, options.strokeWidth * 0.5f);
        AnimPath dot =
            AnimPath::FromEllipse(center.x - radius, center.y - radius,
                                  radius, radius);
        AnimStyle style;
        style.hasFill = true;
        style.fill = stroke;
        style.hasStroke = false;
        return canvas.addDrawnShape(std::move(dot), style, "Brush Dot") != 0;
    }

    const std::vector<Vec2> simplified =
        icg::anim::SimplifyPolyline(raw_, options.smoothing);
    if (simplified.size() < 2) {
        canvas.reportStatus(QObject::tr("Stroke too short - nothing drawn."));
        return false;
    }
    Vec2 start;
    std::vector<AnimSegment> segments;
    icg::anim::FitBeziersToPolyline(simplified, 0.5f, start, segments);
    if (segments.empty()) {
        canvas.reportStatus(QObject::tr("Stroke too short - nothing drawn."));
        return false;
    }
    AnimPath path;
    path.segments.reserve(segments.size() + 1);
    AnimSegment move(AnimSegment::Kind::Move);
    move.p[0] = start;
    path.segments.push_back(move);
    for (AnimSegment& segment : segments) {
        path.segments.push_back(segment);
    }

    AnimStyle style;
    style.hasFill = false;
    style.hasStroke = true;
    style.stroke = stroke;
    style.strokeWidth = options.strokeWidth;
    style.cap = icg::anim::LineCap::Round;
    style.join = icg::anim::LineJoin::Round;
    return canvas.addDrawnShape(std::move(path), style, "Brush Stroke") != 0;
}

bool BrushTool::onKey(AnimatorCanvas& canvas, QKeyEvent& event) {
    if (event.key() != Qt::Key_Escape || !stroking_) {
        return false;
    }
    stroking_ = false;
    raw_.clear();
    canvas.update();
    return true;
}

void BrushTool::paintOverlay(QPainter& painter, AnimatorCanvas& canvas) {
    const DrawingOptions& options = canvas.drawingOptions();
    const float zoom = std::max(0.02f, canvas.view().zoom);
    const icg::anim::AnimColor stroke = styledStroke(canvas);

    // Live stroke preview while painting.
    if (stroking_ && raw_.size() >= 2) {
        QPen pen(QColor(stroke.r, stroke.g, stroke.b, stroke.a));
        pen.setWidthF(options.strokeWidth);
        pen.setCapStyle(Qt::RoundCap);
        pen.setJoinStyle(Qt::RoundJoin);
        painter.setPen(pen);
        painter.setBrush(Qt::NoBrush);
        for (size_t i = 1; i < raw_.size(); ++i) {
            painter.drawLine(toPoint(raw_[i - 1]), toPoint(raw_[i]));
        }
    }

    // Brush ring at the cursor: the actual nib size, so what-you-see matches
    // the committed width. Cosmetic pen keeps it 1px at any zoom.
    if (hasHover_) {
        QPen ring(QColor(60, 140, 255));
        ring.setCosmetic(true);
        painter.setPen(ring);
        painter.setBrush(Qt::NoBrush);
        const double radius = options.strokeWidth * 0.5;
        painter.drawEllipse(hoverStage_, radius, radius);
        (void)zoom;
    }
}

void BrushTool::onDeactivated(AnimatorCanvas& canvas) {
    if (stroking_) {
        stroking_ = false;
        raw_.clear();
        canvas.update();
    }
    hasHover_ = false;
}
