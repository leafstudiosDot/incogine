// Incogine Animator - tool registry.
// Part of Incogine by leafstudiosDot (MPL-2.0). See LICENSE.
#include "tools/tools.h"

ToolSet::ToolSet() {
    // Registration point for future tools (Line, Rect, Ellipse,
    // Paint Bucket, Eraser, Transform). Each is one line here and one file
    // under tools/; nothing else changes. Order sets the toolbar order.
    tools_.push_back(std::unique_ptr<ITool>(new CursorTool()));
    tools_.push_back(std::unique_ptr<ITool>(new HandTool()));
    tools_.push_back(std::unique_ptr<ITool>(new BrushTool()));
    tools_.push_back(std::unique_ptr<ITool>(new PenTool()));
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
