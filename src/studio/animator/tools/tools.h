// Incogine Animator - the tool interface and the Milestone 2 tools.
// Part of Incogine by leafstudiosDot (MPL-2.0). See LICENSE.
//
// Tool system: the canvas owns painting and the view transform,
// and forwards input to whichever ITool is active. Adding Line, Rect,
// Ellipse, Paint Bucket, Eraser, or Transform later means writing one more
// ITool subclass (one file under tools/) and registering it - no change to
// the canvas or the window.
//
// Ships four tools:
//   Hand    - pans the view.
//   Cursor  - select / move / delete / marquee-selection.
//   Brush   - freehand stroked paths with smoothing.
//   Pen     - Flash-style Bezier placement.
//
// Tools receive the canvas by reference and use its primitives (hitTest,
// selection, drag, view) rather than reaching into the document, so a tool can
// never edit geometry behind the command stack's back. Anything that mutates
// the document goes through AnimatorDocument, which pushes a command.
//
// The Qt event headers are included rather than forward-declared: the tools call
// methods on QMouseEvent/QKeyEvent/QWheelEvent, so a bare declaration would
// leave them incomplete.
#pragma once

#include <QCursor>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QObject>
#include <QPainter>
#include <QPointF>
#include <QRectF>
#include <QWheelEvent>
#include <QWidget>

#include <memory>
#include <string>
#include <vector>

// Engine path/scalar types for stroke point storage and the pen preview
// builder. Included directly (rather than relying on canvas.h first) so this
// header is self-contained no matter who includes it.
#include "animation/anim_path.h"
#include "animation/anim_types.h"

class AnimatorCanvas;

class ITool {
public:
    virtual ~ITool() = default;

    // Stable identifier used by the toolbar and QSettings ("cursor", "hand").
    virtual std::string id() const = 0;
    // Short display name for the toolbar button.
    virtual QString label() const = 0;
    // Shortcut hint shown in the tooltip.
    virtual QString shortcutHint() const { return QString(); }
    // Single-key shortcut for the toolbar action ("V", "B"). Empty means the
    // window falls back to the id's first letter. Kept on the tool (rather
    // than in the window) so a new tool brings its own shortcut.
    virtual QString keyShortcut() const { return QString(); }

    // Cursor shown while this tool is active over the canvas.
    virtual QCursor cursor() const { return Qt::ArrowCursor; }

    // Input. Return true when the tool consumed the event. Unhandled events
    // fall through to the canvas's default handling.
    virtual bool onPress(AnimatorCanvas& canvas, const QMouseEvent& event) {
        (void)canvas;
        (void)event;
        return false;
    }
    virtual bool onMove(AnimatorCanvas& canvas, const QMouseEvent& event) {
        (void)canvas;
        (void)event;
        return false;
    }
    virtual bool onRelease(AnimatorCanvas& canvas, const QMouseEvent& event) {
        (void)canvas;
        (void)event;
        return false;
    }
    virtual bool onDoubleClick(AnimatorCanvas& canvas, const QMouseEvent& event) {
        (void)canvas;
        (void)event;
        return false;
    }
    // Most tools ignore the wheel; the canvas owns zoom.
    virtual bool onWheel(AnimatorCanvas& canvas, const QWheelEvent& event) {
        (void)canvas;
        (void)event;
        return false;
    }
    virtual bool onKey(AnimatorCanvas& canvas, QKeyEvent& event) {
        (void)canvas;
        (void)event;
        return false;
    }

    // Overlay drawn after the shapes, in STAGE space (the canvas has already
    // applied the view transform). Used for the marquee rectangle.
    virtual void paintOverlay(QPainter& painter, AnimatorCanvas& canvas) {
        (void)painter;
        (void)canvas;
    }

    // Called when the tool becomes or stops being active, so transient state
    // (a marquee rect, a drag) cannot leak across tool switches.
    virtual void onActivated(AnimatorCanvas& canvas) { (void)canvas; }
    virtual void onDeactivated(AnimatorCanvas& canvas) { (void)canvas; }
};

// Panning tool. Also used temporarily whenever Space is held, regardless of
// the active tool, which is why the canvas can reach it directly.
class HandTool : public ITool {
public:
    std::string id() const override { return "hand"; }
    QString label() const override { return QObject::tr("Hand"); }
    QString shortcutHint() const override {
        return QObject::tr("H  -  or hold Space");
    }
    QString keyShortcut() const override { return QStringLiteral("H"); }
    QCursor cursor() const override { return QCursor(Qt::OpenHandCursor); }

    bool onPress(AnimatorCanvas& canvas, const QMouseEvent& event) override;
    bool onMove(AnimatorCanvas& canvas, const QMouseEvent& event) override;
    bool onRelease(AnimatorCanvas& canvas, const QMouseEvent& event) override;
    void onDeactivated(AnimatorCanvas& canvas) override;

    // Last mouse position while panning, in widget pixels.
    QPointF lastWidgetPos() const { return lastWidgetPos_; }

private:
    QPointF lastWidgetPos_;
    bool panning_ = false;
};

// Selection tool: click to select, Shift+click to add, Ctrl+click to toggle,
// drag on empty space for a marquee, drag a selected shape to move it, and
// Delete/Backspace to delete. Hidden and locked layers can be inspected but not
// modified.
class CursorTool : public ITool {
public:
    std::string id() const override { return "cursor"; }
    QString label() const override { return QObject::tr("Selection"); }
    QString shortcutHint() const override { return QObject::tr("V"); }
    QString keyShortcut() const override { return QStringLiteral("V"); }
    QCursor cursor() const override { return Qt::ArrowCursor; }

    bool onPress(AnimatorCanvas& canvas, const QMouseEvent& event) override;
    bool onMove(AnimatorCanvas& canvas, const QMouseEvent& event) override;
    bool onRelease(AnimatorCanvas& canvas, const QMouseEvent& event) override;
    bool onKey(AnimatorCanvas& canvas, QKeyEvent& event) override;
    void paintOverlay(QPainter& painter, AnimatorCanvas& canvas) override;
    void onDeactivated(AnimatorCanvas& canvas) override;

    bool isMarqueeActive() const { return marqueeActive_; }

private:
    // Marquee rect in stage units; invalid while no marquee is active.
    QRectF marqueeStage_;
    QPointF pressStage_;
    bool marqueeActive_ = false;
    bool marqueeAdditive_ = false;
    // Selection captured before an additive marquee, so Shift-drag can restore
    // it when the marquee turns out to be empty.
    std::vector<uint64_t> selectionBeforeMarquee_;
};

// Freehand brush: drag to paint a stroked path. Raw input points are throttled
// to ~2 screen px, then simplified (RDP) and fitted to Beziers on release, so
// a shaky hand produces a clean selectable stroke. A bare click makes a dot.
class BrushTool : public ITool {
public:
    std::string id() const override { return "brush"; }
    QString label() const override { return QObject::tr("Brush"); }
    QString shortcutHint() const override { return QObject::tr("B"); }
    QString keyShortcut() const override { return QStringLiteral("B"); }
    QCursor cursor() const override { return Qt::CrossCursor; }

    bool onPress(AnimatorCanvas& canvas, const QMouseEvent& event) override;
    bool onMove(AnimatorCanvas& canvas, const QMouseEvent& event) override;
    bool onRelease(AnimatorCanvas& canvas, const QMouseEvent& event) override;
    bool onKey(AnimatorCanvas& canvas, QKeyEvent& event) override;
    void paintOverlay(QPainter& painter, AnimatorCanvas& canvas) override;
    void onDeactivated(AnimatorCanvas& canvas) override;

    bool isStroking() const { return stroking_; }

private:
    bool finishStroke(AnimatorCanvas& canvas);
    void appendIfSpaced(const icg::anim::Vec2& point, float spacing);

    bool stroking_ = false;
    std::vector<icg::anim::Vec2> raw_;
    // Hover position for the brush ring, in stage units. Valid when set.
    QPointF hoverStage_;
    bool hasHover_ = false;
};

// Flash-style pen: click places corner points, click-drag pulls symmetric
// Bezier handles for smooth points, clicking the start point closes the path,
// double-click or Enter finishes an open path, Esc cancels, Backspace drops
// the last point. Strokes only in M3; closed paths stay open strokes (no fill)
// until shape tooling lands.
class PenTool : public ITool {
public:
    std::string id() const override { return "pen"; }
    QString label() const override { return QObject::tr("Pen"); }
    QString shortcutHint() const override { return QObject::tr("P"); }
    QString keyShortcut() const override { return QStringLiteral("P"); }
    QCursor cursor() const override { return Qt::CrossCursor; }

    bool onPress(AnimatorCanvas& canvas, const QMouseEvent& event) override;
    bool onMove(AnimatorCanvas& canvas, const QMouseEvent& event) override;
    bool onRelease(AnimatorCanvas& canvas, const QMouseEvent& event) override;
    bool onDoubleClick(AnimatorCanvas& canvas, const QMouseEvent& event) override;
    bool onKey(AnimatorCanvas& canvas, QKeyEvent& event) override;
    void paintOverlay(QPainter& painter, AnimatorCanvas& canvas) override;
    void onDeactivated(AnimatorCanvas& canvas) override;

    bool hasPoints() const { return !points_.empty(); }

private:
    struct PenPoint {
        icg::anim::Vec2 anchor;
        icg::anim::Vec2 inHandle;
        icg::anim::Vec2 outHandle;
        bool hasIn = false;
        bool hasOut = false;
    };

    bool finishOpen(AnimatorCanvas& canvas);
    bool finishClosed(AnimatorCanvas& canvas);
    icg::anim::AnimPath buildPath(bool closed) const;
    // Screen-pixel close radius converted to stage units at the given zoom.
    static float closeTolerance(float zoom);
    static float handleThreshold(float zoom);

    std::vector<PenPoint> points_;
    bool draggingHandle_ = false;
    QPointF hoverStage_;
    bool hasHover_ = false;
};

// Owns the available tools. Adding a tool is one registration in the
// constructor plus one subclass above.
class ToolSet {
public:
    ToolSet();
    ~ToolSet();

    // Returns nullptr only when the set is empty. An unknown id (a stale
    // QSettings value, say) falls back to the first tool rather than leaving the
    // canvas with no active tool.
    ITool* find(const std::string& id) const;
    const std::vector<std::unique_ptr<ITool>>& all() const { return tools_; }

private:
    std::vector<std::unique_ptr<ITool>> tools_;
};