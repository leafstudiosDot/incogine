// Incogine Animator - hand tool (panning).
// Part of Incogine by leafstudiosDot (MPL-2.0). See LICENSE.
//
// The canvas header comes first: the tool definitions below call methods on
// AnimatorCanvas, so it must be complete before them, and it pulls in the Qt
// event headers the tools use.
#include "canvas.h"
#include "tools/tools.h"

#include <QMouseEvent>
#include <QWidget>

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
