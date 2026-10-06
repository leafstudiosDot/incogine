// Incogine Animator - Flash-style pen tool.
// Part of Incogine by leafstudiosDot (MPL-2.0). See LICENSE.
//
// Click places corner points, click-drag pulls symmetric Bezier handles for
// smooth points, clicking the start point closes the path, double-click or
// Enter finishes an open path, Esc cancels, Backspace drops the last point.
// Strokes only in M3; closed paths stay open strokes (no fill) until shape
// tooling lands.
//
// The canvas header comes first: the tool definitions below call methods on
// AnimatorCanvas, so it must be complete before them, and it pulls in the Qt
// event headers the tools use.
#include "canvas.h"
#include "tools/tools.h"

#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QWidget>

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

float PenTool::closeTolerance(float zoom) {
    return 10.0f / std::max(0.02f, zoom);
}

float PenTool::handleThreshold(float zoom) {
    return 3.0f / std::max(0.02f, zoom);
}

bool PenTool::onPress(AnimatorCanvas& canvas, const QMouseEvent& event) {
    if (event.button() != Qt::LeftButton) {
        return false;
    }
    if (!canvas.isEditable()) {
        canvas.reportStatus(
            QObject::tr("Layer is locked - nothing drawn. Unlock it first."));
        return true;
    }
    if (canvas.activeLayerId() == 0) {
        canvas.reportStatus(QObject::tr("No visible layer - nothing drawn."));
        return true;
    }
    const Vec2 pos = toVec(canvas.view().toStage(event.position()));
    hasHover_ = true;
    hoverStage_ = toPoint(pos);

    // Clicking near the start point closes the path (needs at least 2 points
    // to form an area; a 1-point "close" would be a dot, which is the brush's
    // job).
    if (points_.size() >= 2) {
        const float tol = closeTolerance(canvas.view().zoom);
        if (icg::anim::Distance(points_.front().anchor, pos) <= tol) {
            return finishClosed(canvas);
        }
    }

    // New point starts as a corner; a drag before release promotes it to a
    // smooth point with symmetric handles (Flash behavior).
    PenPoint point;
    point.anchor = pos;
    points_.push_back(point);
    draggingHandle_ = true;
    canvas.update();
    return true;
}

bool PenTool::onMove(AnimatorCanvas& canvas, const QMouseEvent& event) {
    hasHover_ = true;
    hoverStage_ = canvas.view().toStage(event.position());
    if (!draggingHandle_ || points_.empty()) {
        if (!points_.empty()) {
            canvas.update();
            return true; // refresh the rubber band
        }
        return false;
    }
    const Vec2 cur = toVec(hoverStage_);
    PenPoint& point = points_.back();
    const Vec2 drag = cur - point.anchor;
    if (icg::anim::Length(drag) >= handleThreshold(canvas.view().zoom)) {
        point.hasIn = true;
        point.hasOut = true;
        point.outHandle = point.anchor + drag;
        point.inHandle = point.anchor - drag;
    } else {
        point.hasIn = false;
        point.hasOut = false;
    }
    canvas.update();
    return true;
}

bool PenTool::onRelease(AnimatorCanvas& canvas, const QMouseEvent& event) {
    if (event.button() != Qt::LeftButton || !draggingHandle_) {
        return false;
    }
    draggingHandle_ = false;
    // A press-release without a drag stays a corner point; the press already
    // recorded the anchor, so there is nothing more to do until the next click.
    hasHover_ = true;
    hoverStage_ = event.position();
    canvas.update();
    return true;
}

bool PenTool::onDoubleClick(AnimatorCanvas& canvas, const QMouseEvent& event) {
    (void)event;
    // Double-click ends the second click's drag first (press added a duplicate
    // anchor); drop it when it landed on the previous point.
    if (!points_.empty() && !draggingHandle_) {
        return finishOpen(canvas);
    }
    return false;
}

bool PenTool::onKey(AnimatorCanvas& canvas, QKeyEvent& event) {
    if (event.key() == Qt::Key_Escape && !points_.empty()) {
        points_.clear();
        draggingHandle_ = false;
        canvas.update();
        return true;
    }
    if ((event.key() == Qt::Key_Return || event.key() == Qt::Key_Enter) &&
        !points_.empty()) {
        return finishOpen(canvas);
    }
    if (event.key() == Qt::Key_Backspace && !points_.empty() &&
        !draggingHandle_) {
        points_.pop_back();
        canvas.update();
        return true;
    }
    return false;
}

AnimPath PenTool::buildPath(bool closed) const {
    AnimPath path;
    if (points_.size() < 2) {
        return path;
    }
    path.segments.reserve(points_.size() + 1);
    AnimSegment move(AnimSegment::Kind::Move);
    move.p[0] = points_.front().anchor;
    path.segments.push_back(move);

    const size_t runs = closed ? points_.size() : points_.size() - 1;
    for (size_t i = 0; i < runs; ++i) {
        const PenPoint& prev = points_[i];
        const PenPoint& cur = points_[(i + 1) % points_.size()];
        if (prev.hasOut || cur.hasIn) {
            AnimSegment cubic(AnimSegment::Kind::Cubic);
            cubic.p[0] = prev.hasOut ? prev.outHandle : prev.anchor;
            cubic.p[1] = cur.hasIn ? cur.inHandle : cur.anchor;
            cubic.p[2] = cur.anchor;
            path.segments.push_back(cubic);
        } else {
            AnimSegment line(AnimSegment::Kind::Line);
            line.p[0] = cur.anchor;
            path.segments.push_back(line);
        }
    }
    if (closed) {
        path.segments.push_back(AnimSegment(AnimSegment::Kind::Close));
    }
    return path;
}

bool PenTool::finishOpen(AnimatorCanvas& canvas) {
    draggingHandle_ = false;
    if (points_.size() < 2) {
        canvas.reportStatus(
            QObject::tr("Pen needs at least two points - click again or press Esc."));
        return points_.size() >= 1;
    }
    const DrawingOptions& options = canvas.drawingOptions();
    AnimPath path = buildPath(false);
    points_.clear();
    if (path.IsEmpty()) {
        return false;
    }
    AnimStyle style;
    style.hasFill = false;
    style.hasStroke = true;
    style.stroke = styledStroke(canvas);
    style.strokeWidth = options.strokeWidth;
    style.cap = icg::anim::LineCap::Round;
    style.join = icg::anim::LineJoin::Round;
    const bool ok =
        canvas.addDrawnShape(std::move(path), style, "Pen Path") != 0;
    canvas.update();
    return ok;
}

bool PenTool::finishClosed(AnimatorCanvas& canvas) {
    draggingHandle_ = false;
    if (points_.size() < 2) {
        points_.clear();
        canvas.update();
        return true;
    }
    const DrawingOptions& options = canvas.drawingOptions();
    AnimPath path = buildPath(true);
    points_.clear();
    if (path.IsEmpty()) {
        return false;
    }
    AnimStyle style;
    style.hasFill = false;
    style.hasStroke = true;
    style.stroke = styledStroke(canvas);
    style.strokeWidth = options.strokeWidth;
    style.cap = icg::anim::LineCap::Round;
    style.join = icg::anim::LineJoin::Round;
    const bool ok =
        canvas.addDrawnShape(std::move(path), style, "Pen Path") != 0;
    canvas.update();
    return ok;
}

void PenTool::paintOverlay(QPainter& painter, AnimatorCanvas& canvas) {
    if (points_.empty()) {
        return;
    }
    const float zoom = std::max(0.02f, canvas.view().zoom);
    const double anchorPx = 4.0 / zoom;
    const double handlePx = 3.0 / zoom;

    // Placed path so far.
    {
        QPen pen(QColor(60, 140, 255));
        pen.setCosmetic(true);
        painter.setPen(pen);
        painter.setBrush(Qt::NoBrush);
        const AnimPath preview = buildPath(false);
        bool first = true;
        Vec2 cursor;
        for (const AnimSegment& segment : preview.segments) {
            if (segment.kind == AnimSegment::Kind::Move) {
                cursor = segment.p[0];
                first = false;
                (void)first;
            } else if (segment.kind == AnimSegment::Kind::Line) {
                painter.drawLine(toPoint(cursor), toPoint(segment.p[0]));
                cursor = segment.p[0];
            } else if (segment.kind == AnimSegment::Kind::Cubic) {
                QPainterPath curve;
                curve.moveTo(toPoint(cursor));
                curve.cubicTo(toPoint(segment.p[0]), toPoint(segment.p[1]),
                              toPoint(segment.p[2]));
                painter.drawPath(curve);
                cursor = segment.p[2];
            }
        }
    }

    // Handles.
    {
        QPen pen(QColor(90, 90, 200));
        pen.setCosmetic(true);
        painter.setPen(pen);
        painter.setBrush(Qt::NoBrush);
        for (const PenPoint& point : points_) {
            const QPointF anchor = toPoint(point.anchor);
            if (point.hasIn) {
                const QPointF in = toPoint(point.inHandle);
                painter.drawLine(anchor, in);
                painter.drawEllipse(in, handlePx, handlePx);
            }
            if (point.hasOut) {
                const QPointF out = toPoint(point.outHandle);
                painter.drawLine(anchor, out);
                painter.drawEllipse(out, handlePx, handlePx);
            }
        }
    }

    // Anchors on top.
    {
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(255, 255, 255));
        QPen outline(QColor(60, 140, 255));
        outline.setCosmetic(true);
        painter.setPen(outline);
        for (size_t i = 0; i < points_.size(); ++i) {
            const QPointF anchor = toPoint(points_[i].anchor);
            painter.drawRect(
                QRectF(anchor.x() - anchorPx, anchor.y() - anchorPx,
                       anchorPx * 2.0, anchorPx * 2.0));
        }
    }

    // Rubber band from the last anchor to the cursor.
    if (hasHover_ && !draggingHandle_) {
        QPen pen(QColor(60, 140, 255, 160));
        pen.setStyle(Qt::DashLine);
        pen.setCosmetic(true);
        painter.setPen(pen);
        painter.setBrush(Qt::NoBrush);
        painter.drawLine(toPoint(points_.back().anchor), hoverStage_);
    }

    // Close halo when hovering the start point.
    if (points_.size() >= 2 && hasHover_) {
        const Vec2 hover = toVec(hoverStage_);
        if (icg::anim::Distance(points_.front().anchor, hover) <=
            closeTolerance(zoom)) {
            QPen pen(QColor(60, 200, 90));
            pen.setCosmetic(true);
            painter.setPen(pen);
            painter.setBrush(Qt::NoBrush);
            const QPointF start = toPoint(points_.front().anchor);
            const double halo = (anchorPx + 4.0 / zoom);
            painter.drawEllipse(start, halo, halo);
        }
    }
}

void PenTool::onDeactivated(AnimatorCanvas& canvas) {
    if (!points_.empty() || draggingHandle_) {
        // A half-drawn path is abandoned, not committed: committing on tool
        // switch would surprise the user with a shape they did not finish.
        points_.clear();
        draggingHandle_ = false;
        canvas.update();
    }
    hasHover_ = false;
}
