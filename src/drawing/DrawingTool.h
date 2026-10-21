#pragma once
#include <QPoint>
#include <Qt>
#include <memory>

class Editor;

enum class ToolId { Pencil, Brush, Eraser, Fill, Eyedropper, Line, Shape, Text, SelectRect, SelectFree, Count };

// A tool turns pointer input (in IMAGE coordinates, never viewport coordinates)
// into engine calls. Tools never see zoom or widget geometry.
class Tool {
public:
    virtual ~Tool() = default;
    virtual ToolId id() const = 0;
    virtual void press(Editor &, const QPoint &, Qt::MouseButton, Qt::KeyboardModifiers) = 0;
    virtual void move(Editor &, const QPoint &, Qt::KeyboardModifiers) = 0;
    virtual void release(Editor &, const QPoint &, Qt::KeyboardModifiers) = 0;
    virtual void cancel(Editor &) = 0;   // abandon the in-progress interaction, leaving the document untouched
    virtual bool active() const = 0;
};

std::unique_ptr<Tool> makeTool(ToolId id);
inline bool isSelectionTool(ToolId id) { return id == ToolId::SelectRect || id == ToolId::SelectFree; }
