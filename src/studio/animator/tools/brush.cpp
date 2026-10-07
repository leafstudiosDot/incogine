// Incogine Animator - Flash-style freeform brush tool.
// Part of Incogine by leafstudiosDot (MPL-2.0). See LICENSE.
//
// Drag to paint a brush stroke (a freeform Pen: input is fitted to smooth
// Beziers on release and stored as a compact centerline + width; the canvas
// paints it as union-correct fill pieces). A bare click makes a dot.
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
#include <QPainterPath>
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

// Minimum distance between sampled input points, in STAGE units.
//
// The screen-space part (2px / zoom) keeps sampling resolution visually
// constant as you zoom. The floor is what stops that from running away: at high
// zoom 2/zoom gets tiny, so a stroke picks up points far denser than the brush
// is wide - measured at zoom 64, ~26x denser than the stroke can resolve.
//
// That density was the cause of BOTH reported problems:
//   - The stroker offsets the centerline by half the width. When neighbouring
//     points are closer together than that, the offset outline folds back on
//     itself and the fill leaves pinholes: the "cuts" in a stroke.
//   - Vertex count, and so paint cost, grew without limit as you zoomed in -
//     the lag that got worse the closer you looked.
// Flooring the spacing at a fraction of the stroke width bounds vertex count
// independently of zoom, so zooming in no longer costs more to draw. Below the
// floor the extra points carried no visible detail anyway.
float BrushTool::sampleSpacing(const AnimatorCanvas& canvas) const {
    const float screenSpacing = 2.0f / std::max(0.02f, canvas.view().zoom);
    const float widthFloor = canvas.drawingOptions().strokeWidth * 0.2f;
    return std::max(screenSpacing, widthFloor);
}

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
    appendIfSpaced(toVec(hoverStage_), sampleSpacing(canvas));
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

    // Freeform pen: fit the raw input to smooth Beziers first (least-squares
    // cubics smooth through hand jitter on their own; a separate RDP pass only
    // replaces smooth dense points with angular zigzag that fragments the fit,
    // measured 2.5x more segments for the same error). The smoothing slider IS
    // the fit tolerance, so the knob is honest: curves stay within it of the
    // drawn input.
    Vec2 start;
    std::vector<AnimSegment> segments;
    icg::anim::FitBeziersToPolyline(
        raw_, std::max(0.25f, options.smoothing), start, segments);
    if (segments.empty()) {
        canvas.reportStatus(QObject::tr("Stroke too short - nothing drawn."));
        return false;
    }
    // Store the SOURCE geometry: the fitted centerline plus the stroke width.
    // A 100-point stroke fits to ~two dozen segments (~2 kB on disk); the
    // tessellated fill pieces it paints as would be ~12k segments (~1.3 MB).
    // Storing derived tessellation multiplied every downstream cost (JSON,
    // RAM, undo, paint subpaths) by ~500x, so the model keeps the compact
    // source and the canvas tessellates transiently (flatten + pieces caches).
    // Painting still never strokes: strokePiecesPath expands the cached
    // centerline to union-correct fill pieces, solid on overlap.
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

    // Live stroke preview while painting: the raw input expanded to fill
    // pieces, painted with a single fillPath. The old preview stroked every
    // raw segment with a QPen, which ran the ~20us-per-vertex raster stroker
    // on every mouse move - the zoomed-in lag while drawing. A fill is ~10x
    // cheaper for the same pixels, and pieces stay solid on overlap.
    //
    // The preview expands the RAW input, while commit fits Beziers first, so
    // the final shape is slightly smoother than the preview. That matches
    // Flash behaviour (ink first, smoothing on release) and keeps every mouse
    // move cheap: no fitting per move.
    if (stroking_ && raw_.size() >= 2) {
        icg::anim::FlatPath rawFlat;
        rawFlat.polylines.push_back(raw_);
        rawFlat.closed.push_back(false);
        icg::anim::StrokeOutlineOptions previewOpts;
        previewOpts.width = options.strokeWidth;
        previewOpts.cap = icg::anim::LineCap::Round;
        previewOpts.join = icg::anim::LineJoin::Round;
        const icg::anim::FlatPath band =
            icg::anim::StrokeToPieces(rawFlat, previewOpts);
        QPainterPath preview;
        preview.setFillRule(Qt::WindingFill);
        for (const std::vector<icg::anim::Vec2>& poly : band.polylines) {
            if (poly.size() < 3) {
                continue;
            }
            preview.moveTo(toPoint(poly[0]));
            for (size_t i = 1; i < poly.size(); ++i) {
                preview.lineTo(toPoint(poly[i]));
            }
            preview.closeSubpath();
        }
        if (!preview.isEmpty()) {
            painter.save();
            painter.setPen(Qt::NoPen);
            painter.setBrush(QColor(stroke.r, stroke.g, stroke.b, stroke.a));
            painter.drawPath(preview);
            painter.restore();
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
