// The canvas header comes first: the tool definitions below call methods on
// AnimatorCanvas, so it must be complete before them, and it pulls in the Qt
// event headers the tools use.
#include "animator_canvas.h"
#include "animator_tools.h"

#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QWidget>

using icg::anim::AnimKeyframe;
using icg::anim::AnimShape;

// ------------------------------------------------------------- HandTool --

bool HandTool::onPress(AnimatorCanvas& canvas, const QMouseEvent& event) {
    if (event.button() != Qt::LeftButton && event.button() != Qt::MiddleButton) {
        return false;
    }
    panning_ = true;
    lastWidgetPos_ = event.position();
    canvas.setCursor(Qt::ClosedHandCursor);
    return true;
}

bool HandTool::onMove(AnimatorCanvas& canvas, const QMouseEvent& event) {
    if (!panning_) {
        return false;
    }
    // Pan in SCREEN pixels, independent of zoom, so the canvas tracks the cursor
    // exactly at any scale.
    canvas.panBy(event.position() - lastWidgetPos_);
    lastWidgetPos_ = event.position();
    return true;
}

bool HandTool::onRelease(AnimatorCanvas& canvas, const QMouseEvent& event) {
    if (!panning_) {
        return false;
    }
    panning_ = false;
    canvas.updateViewAfterPan();
    canvas.setToolCursor();
    return true;
}

void HandTool::onDeactivated(AnimatorCanvas& canvas) {
    // A tool switch mid-drag must not leave the canvas in a closed-hand state.
    panning_ = false;
    canvas.setToolCursor();
}

// ----------------------------------------------------------- CursorTool --

bool CursorTool::onPress(AnimatorCanvas& canvas, const QMouseEvent& event) {
    if (event.button() != Qt::LeftButton) {
        return false;
    }
    const QPointF stagePos = canvas.view().toStage(event.position());
    const Qt::KeyboardModifiers mods = event.modifiers();
    pressStage_ = stagePos;
    selectionBeforeMarquee_.clear();

    const uint64_t hit = canvas.hitTest(stagePos);
    if (hit != 0) {
        marqueeActive_ = false;
        if (mods.testFlag(Qt::ControlModifier)) {
            canvas.toggleInSelection(hit);
        } else if (mods.testFlag(Qt::ShiftModifier)) {
            canvas.addToSelection(QSet<uint64_t>{hit});
        } else if (!canvas.isSelected(hit)) {
            canvas.setSelection(QSet<uint64_t>{hit});
        }
        // A hidden or locked layer is still selectable for inspection, but not
        // movable, so no drag is started.
        if (canvas.isEditable()) {
            canvas.beginDrag(stagePos);
        }
        return true;
    }

    // Empty space: marquee. Shift keeps whatever was already selected.
    marqueeActive_ = true;
    marqueeAdditive_ = mods.testFlag(Qt::ShiftModifier);
    if (marqueeAdditive_) {
        selectionBeforeMarquee_.assign(canvas.selection().begin(),
                                      canvas.selection().end());
    } else if (!mods.testFlag(Qt::ControlModifier)) {
        canvas.clearSelection();
    }
    marqueeStage_ = QRectF(stagePos, stagePos);
    return true;
}

bool CursorTool::onMove(AnimatorCanvas& canvas, const QMouseEvent& event) {
    if (canvas.isDragging()) {
        // The canvas applies the live offset; nothing more to do here.
        return true;
    }
    if (!marqueeActive_) {
        return false;
    }
    const QPointF stagePos = canvas.view().toStage(event.position());
    marqueeStage_ = QRectF(pressStage_, stagePos).normalized();
    canvas.update();
    return true;
}

bool CursorTool::onRelease(AnimatorCanvas& canvas, const QMouseEvent& event) {
    if (event.button() != Qt::LeftButton) {
        return false;
    }
    if (canvas.isDragging()) {
        // A drag that actually moved becomes one undo step; a plain click does
        // not, because the controller refuses no-op moves.
        canvas.commitDrag();
        return true;
    }
    if (!marqueeActive_) {
        return false;
    }

    const std::vector<uint64_t> enclosed = canvas.shapesInRect(marqueeStage_);
    if (marqueeAdditive_) {
        // An empty Shift-marquee is ambiguous: clearing the selection would make
        // it impossible to deselect by dragging. Restore the pre-marquee set
        // instead, which is what every art tool does.
        if (enclosed.empty()) {
            QSet<uint64_t> restored;
            for (uint64_t id : selectionBeforeMarquee_) {
                restored.insert(id);
            }
            canvas.setSelection(restored);
        } else {
            QSet<uint64_t> merged;
            for (uint64_t id : selectionBeforeMarquee_) {
                merged.insert(id);
            }
            for (uint64_t id : enclosed) {
                merged.insert(id);
            }
            canvas.setSelection(merged);
        }
    } else {
        QSet<uint64_t> next;
        for (uint64_t id : enclosed) {
            next.insert(id);
        }
        canvas.setSelection(next);
    }

    marqueeActive_ = false;
    selectionBeforeMarquee_.clear();
    canvas.update();
    return true;
}

bool CursorTool::onKey(AnimatorCanvas& canvas, QKeyEvent& event) {
    if (event.key() != Qt::Key_Delete && event.key() != Qt::Key_Backspace) {
        return false;
    }
    if (canvas.selectionCount() == 0) {
        return false;
    }
    if (!canvas.isEditable()) {
        // Locked layer: say so instead of silently doing nothing.
        canvas.reportStatus(QObject::tr("Layer is locked - nothing deleted."));
        return true;
    }
    canvas.deleteSelection();
    return true;
}

void CursorTool::paintOverlay(QPainter& painter, AnimatorCanvas& canvas) {
    if (!marqueeActive_) {
        return;
    }
    // Called in stage space: the canvas has already applied the view transform.
    QPen pen(QColor(60, 140, 255));
    pen.setStyle(Qt::DashLine);
    // Cosmetic so the marquee keeps a 1px outline at any zoom.
    pen.setCosmetic(true);
    painter.setPen(pen);
    QColor fill(60, 140, 255, 40);
    painter.setBrush(fill);
    painter.drawRect(marqueeStage_);
}

void CursorTool::onDeactivated(AnimatorCanvas& canvas) {
    // Never carry a marquee or a drag into another tool.
    if (marqueeActive_) {
        marqueeActive_ = false;
        selectionBeforeMarquee_.clear();
    }
    if (canvas.isDragging()) {
        canvas.cancelDrag();
    }
    canvas.update();
}

// ---------------------------------------------------------------- ToolSet --

ToolSet::ToolSet() {
    // Registration point for future tools (Pen, Line, Rect, Ellipse,
    // Paint Bucket, Eraser, Transform). Each is one line here and one subclass
    // in animator_tools.cpp; nothing else changes.
    tools_.push_back(std::unique_ptr<ITool>(new CursorTool()));
    tools_.push_back(std::unique_ptr<ITool>(new HandTool()));
}

ToolSet::~ToolSet() = default;

ITool* ToolSet::find(const std::string& id) const {
    for (const auto& tool : tools_) {
        if (tool->id() == id) {
            return tool.get();
        }
    }
    // Unknown id (a stale QSettings value, say): fall back to the first tool
    // rather than leaving the canvas with no active tool.
    return tools_.empty() ? nullptr : tools_.front().get();
}
