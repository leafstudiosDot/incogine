// Incogine Animator - shared vector paint helpers (studio-side).
// Part of Incogine by leafstudiosDot (MPL-2.0). See LICENSE.
//
// One place that turns resolved shapes into QPainterPaths and paints them.
// Used by the canvas (scene bake, selection, onion skins) AND the RAM frame
// cache, so cached frames, the viewport, and onion ghosts all rasterize
// identical geometry. Header-inline: no .cpp, no moc, no extra target.
//
// ASCII-only by repo convention.

#pragma once

#include <QPainter>
#include <QPainterPath>
#include <QPointF>

#include "animation/anim_document.h"
#include "animation/anim_geometry.h"

// The flattened subpaths in stage space. WindingFill: multi-subpath fills
// sharing one winding must stay solid where they overlap; single loops render
// identically under either rule.
inline QPainterPath buildFillPath(const icg::anim::ResolvedShape& shape) {
    QPainterPath path;
    path.setFillRule(Qt::WindingFill);
    for (const auto& polyline : shape.path.polylines) {
        if (polyline.size() < 2) {
            continue;
        }
        bool first = true;
        for (const icg::anim::Vec2& point : polyline) {
            // Shape-local space carried into stage space by the matrix, which
            // is also what QPainter then scales to widgets.
            const icg::anim::Vec2 stageVec =
                icg::anim::TransformPoint(shape.matrix, point);
            if (first) {
                path.moveTo(stageVec.x, stageVec.y);
                first = false;
            } else {
                path.lineTo(stageVec.x, stageVec.y);
            }
        }
    }
    return path;
}

// The centerline expanded to union-correct fill pieces, in stage space.
// `tolerance` is the flatten/arc tolerance in shape-local units (Final
// quality for cache bakes, viewport quality for live paint).
inline QPainterPath buildPiecesPath(const icg::anim::ResolvedShape& shape,
                                    float tolerance) {
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
        for (const icg::anim::Vec2& point : polyline) {
            const icg::anim::Vec2 stageVec =
                icg::anim::TransformPoint(shape.matrix, point);
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

// The single outline loop, for STROKING selection highlights (winding never
// applies to a stroked highlight, so the loop's self-overlap holes cannot
// show there). Never used for fills - that is what buildPiecesPath is for.
inline QPainterPath buildOutlinePath(const icg::anim::ResolvedShape& shape,
                                     float tolerance) {
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
        for (const icg::anim::Vec2& point : polyline) {
            const icg::anim::Vec2 stageVec =
                icg::anim::TransformPoint(shape.matrix, point);
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

// Paints one resolved shape with a NoPen painter: fills (skipped in outline
// mode), then strokes as union-correct fill pieces - or, in outline mode, as
// a thin cosmetic centerline.
inline void paintResolvedShape(QPainter& painter,
                               const icg::anim::ResolvedShape& shape,
                               float tolerance) {
    const auto toColor = [](const icg::anim::AnimColor& c) {
        return QColor(c.r, c.g, c.b, c.a);
    };
    if (shape.layerOutline) {
        // Wireframe: no fills; every subpath traced as a 1px centerline.
        QPen pen(QColor(80, 80, 80));
        pen.setCosmetic(true);
        painter.save();
        painter.setPen(pen);
        painter.setBrush(Qt::NoBrush);
        painter.drawPath(buildFillPath(shape));
        painter.restore();
        return;
    }
    if (shape.hasFill && shape.fill.a > 0) {
        painter.fillPath(buildFillPath(shape), toColor(shape.fill));
    }
    if (shape.hasStroke && shape.stroke.a > 0 && shape.strokeWidth > 0.0f) {
        painter.fillPath(buildPiecesPath(shape, tolerance),
                         toColor(shape.stroke));
    }
}
