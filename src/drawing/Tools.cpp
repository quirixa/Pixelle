#include "DrawingTool.h"
#include "Editor.h"
#include <QPainterPath>
#include <cmath>

namespace {

// ---------- freehand tools: draw live into the document inside an undo transaction ----------
class FreehandTool : public Tool {
public:
    explicit FreehandTool(ToolId id) : m_id(id) {}
    ToolId id() const override { return m_id; }
    bool active() const override { return m_active; }

    void press(Editor &e, const QPoint &p, Qt::MouseButton b, Qt::KeyboardModifiers) override {
        m_btn = b; m_last = p; m_active = true;
        e.beginEdit();
        stamp(e, p, p);
    }
    void move(Editor &e, const QPoint &p, Qt::KeyboardModifiers) override {
        if (!m_active) return;
        stamp(e, m_last, p);
        m_last = p;
    }
    void release(Editor &e, const QPoint &p, Qt::KeyboardModifiers) override {
        if (!m_active) return;
        stamp(e, m_last, p);
        e.commitEdit();               // one undo entry per stroke
        m_active = false;
    }
    void cancel(Editor &e) override {
        if (!m_active) return;
        e.abortEdit();                // restores every touched tile
        m_active = false;
    }

protected:
    virtual int margin(const Editor &) const = 0;
    virtual void draw(Editor &, QImage &, const QPoint &a, const QPoint &b, const QColor &c) = 0;
    Qt::MouseButton m_btn = Qt::LeftButton;

private:
    void stamp(Editor &e, const QPoint &a, const QPoint &b) {
        const QRect r = Raster::strokeBounds(a, b, margin(e));
        e.touch(r);                                           // BEFORE modifying pixels
        draw(e, e.document().mutableImage(), a, b, e.colorFor(m_btn));
        e.document().notifyChanged(r);
    }
    ToolId m_id;
    QPoint m_last;
    bool m_active = false;
};

class PencilTool : public FreehandTool {
public:
    PencilTool() : FreehandTool(ToolId::Pencil) {}
protected:
    int margin(const Editor &e) const override { return e.settings().size + 1; }
    void draw(Editor &e, QImage &img, const QPoint &a, const QPoint &b, const QColor &c) override {
        Raster::pencilSegment(img, a, b, c, e.settings().size);
    }
};

class BrushTool : public FreehandTool {
public:
    BrushTool() : FreehandTool(ToolId::Brush) {}
protected:
    int margin(const Editor &e) const override { return Raster::brushMargin(e.settings().brush, e.settings().size); }
    void draw(Editor &e, QImage &img, const QPoint &a, const QPoint &b, const QColor &c) override {
        Raster::brushSegment(img, e.settings().brush, e.settings().size, a, b, c, e.rng(), e.settings().antialias);
    }
};

class EraserTool : public FreehandTool {
public:
    EraserTool() : FreehandTool(ToolId::Eraser) {}
protected:
    int margin(const Editor &e) const override { return e.settings().size + 1; }
    void draw(Editor &e, QImage &img, const QPoint &a, const QPoint &b, const QColor &) override {
        Raster::eraseSegment(img, a, b, e.settings().size, e.settings().bg, e.settings().eraserTransparent);
    }
};

// ---------- click tools ----------
class FillTool : public Tool {
public:
    ToolId id() const override { return ToolId::Fill; }
    bool active() const override { return false; }
    void press(Editor &e, const QPoint &p, Qt::MouseButton b, Qt::KeyboardModifiers) override {
        if (!e.document().image().rect().contains(p)) return;
        e.beginEdit();
        const QRect r = Raster::floodFill(e.document().mutableImage(), p, e.colorFor(b), e.settings().fillTolerance,
                                          [&](const QRect &bounds) { e.touch(bounds); });
        e.commitEdit();
        if (!r.isEmpty()) e.document().notifyChanged(r);
    }
    void move(Editor &, const QPoint &, Qt::KeyboardModifiers) override {}
    void release(Editor &, const QPoint &, Qt::KeyboardModifiers) override {}
    void cancel(Editor &) override {}
};

class EyedropperTool : public Tool {
public:
    ToolId id() const override { return ToolId::Eyedropper; }
    bool active() const override { return m_active; }
    void press(Editor &e, const QPoint &p, Qt::MouseButton b, Qt::KeyboardModifiers) override { m_btn = b; m_active = true; pick(e, p); }
    void move(Editor &e, const QPoint &p, Qt::KeyboardModifiers) override { if (m_active) pick(e, p); }
    void release(Editor &, const QPoint &, Qt::KeyboardModifiers) override { m_active = false; }
    void cancel(Editor &) override { m_active = false; }
private:
    void pick(Editor &e, const QPoint &p) {
        if (e.document().image().rect().contains(p)) e.setColorFor(m_btn, QColor::fromRgba(e.document().image().pixel(p)));
    }
    Qt::MouseButton m_btn = Qt::LeftButton;
    bool m_active = false;
};

class TextTool : public Tool {
public:
    ToolId id() const override { return ToolId::Text; }
    bool active() const override { return false; }
    void press(Editor &e, const QPoint &p, Qt::MouseButton, Qt::KeyboardModifiers) override { e.requestText(p); }
    void move(Editor &, const QPoint &, Qt::KeyboardModifiers) override {}
    void release(Editor &, const QPoint &, Qt::KeyboardModifiers) override {}
    void cancel(Editor &) override {}
};

// ---------- lines and shapes: preview in an overlay, commit once on release ----------
class ShapeTool : public Tool {
public:
    explicit ShapeTool(bool line) : m_line(line) {}
    ToolId id() const override { return m_line ? ToolId::Line : ToolId::Shape; }
    bool active() const override { return m_active; }

    void press(Editor &e, const QPoint &p, Qt::MouseButton b, Qt::KeyboardModifiers m) override {
        m_btn = b; m_start = m_end = p; m_active = true;
        update(e, m);
    }
    void move(Editor &e, const QPoint &p, Qt::KeyboardModifiers m) override {
        if (!m_active) return;
        m_raw = p; update(e, m);
    }
    void release(Editor &e, const QPoint &p, Qt::KeyboardModifiers m) override {
        if (!m_active) return;
        m_raw = p; compute(m);
        const QRect b = bounds(e).intersected(e.document().image().rect());
        e.clearPreview();
        e.beginEdit();
        e.touch(b);
        render(e, e.document().mutableImage(), QPoint(0, 0));
        e.commitEdit();
        e.document().notifyChanged(b);
        m_active = false;
    }
    void cancel(Editor &e) override { e.clearPreview(); m_active = false; }

private:
    void compute(Qt::KeyboardModifiers m) {
        QPoint end = m_raw;
        if (m & Qt::ShiftModifier) {
            const int dx = end.x() - m_start.x(), dy = end.y() - m_start.y();
            if (m_line) {   // snap to the nearest multiple of 45 degrees
                const double ang = std::round(std::atan2(double(dy), double(dx)) / (M_PI / 4)) * (M_PI / 4);
                const double len = std::hypot(double(dx), double(dy));
                end = m_start + QPoint(qRound(std::cos(ang) * len), qRound(std::sin(ang) * len));
            } else {        // square / circle
                const int s = std::max(std::abs(dx), std::abs(dy));
                end = m_start + QPoint(dx < 0 ? -s : s, dy < 0 ? -s : s);
            }
        }
        m_end = end;
    }
    QRect bounds(const Editor &e) const { return Raster::shapeBounds(QRect(m_start, m_end).normalized(), e.settings().size); }
    void render(Editor &e, QImage &target, const QPoint &origin) {
        const QColor primary = e.colorFor(m_btn), secondary = e.otherColorFor(m_btn);
        const ToolSettings &s = e.settings();
        if (m_line) Raster::drawLineShape(target, origin, m_start, m_end, primary, s.size, s.antialias);
        else Raster::drawShape(target, origin, s.shape, QRect(m_start, m_end).normalized(), primary, secondary, s.size, s.fillMode, s.antialias);
    }
    void update(Editor &e, Qt::KeyboardModifiers m) {
        compute(m);
        const QRect b = bounds(e).intersected(e.document().image().rect());
        if (b.isEmpty()) { e.clearPreview(); return; }
        QImage ov(b.size(), QImage::Format_ARGB32);
        ov.fill(Qt::transparent);
        render(e, ov, b.topLeft());          // same code path as the final commit
        e.setPreview(ov, b.topLeft());
    }
    bool m_line;
    bool m_active = false;
    Qt::MouseButton m_btn = Qt::LeftButton;
    QPoint m_start, m_end, m_raw;
};

// ---------- selection tools ----------
class SelectTool : public Tool {
public:
    explicit SelectTool(bool free) : m_free(free) {}
    ToolId id() const override { return m_free ? ToolId::SelectFree : ToolId::SelectRect; }
    bool active() const override { return m_mode != Mode::None; }

    void press(Editor &e, const QPoint &p, Qt::MouseButton, Qt::KeyboardModifiers m) override {
        if (e.selectionContains(p)) {
            m_mode = Mode::Dragging;
            m_last = p;
            e.beginSelectionDrag(m & Qt::ControlModifier);
            return;
        }
        e.commitSelection();
        m_mode = Mode::Drawing;
        m_start = p;
        m_pts = {p};
        rubber(e, p);
    }
    void move(Editor &e, const QPoint &p, Qt::KeyboardModifiers) override {
        if (m_mode == Mode::Dragging) { e.moveSelectionBy(p - m_last); m_last = p; }
        else if (m_mode == Mode::Drawing) { if (m_free) m_pts << p; rubber(e, p); }
    }
    void release(Editor &e, const QPoint &p, Qt::KeyboardModifiers) override {
        if (m_mode == Mode::Dragging) { e.moveSelectionBy(p - m_last); }
        else if (m_mode == Mode::Drawing) {
            e.clearRubber();
            if (m_free) {
                m_pts << p;
                if (m_pts.size() >= 3) e.selectPath(polygon());
            } else {
                const QRect r = QRect(m_start, p).normalized();    // inclusive corners
                if (r.width() > 1 || r.height() > 1) e.selectRect(r);
            }
        }
        m_mode = Mode::None;
    }
    void cancel(Editor &e) override {
        if (m_mode == Mode::Dragging) e.cancelSelectionDrag();
        e.clearRubber();
        m_mode = Mode::None;
    }

private:
    QPainterPath polygon() const {
        QPolygonF poly;
        for (const QPoint &q : m_pts) poly << QPointF(q) + QPointF(0.5, 0.5);
        QPainterPath path;
        path.addPolygon(poly);
        path.closeSubpath();
        return path;
    }
    void rubber(Editor &e, const QPoint &p) {
        if (m_free) { e.setRubber(polygon()); return; }
        QPainterPath path;
        path.addRect(QRect(m_start, p).normalized());
        e.setRubber(path);
    }
    enum class Mode { None, Drawing, Dragging };
    bool m_free;
    Mode m_mode = Mode::None;
    QPoint m_start, m_last;
    QList<QPoint> m_pts;
};

} // namespace

std::unique_ptr<Tool> makeTool(ToolId id) {
    switch (id) {
    case ToolId::Pencil: return std::make_unique<PencilTool>();
    case ToolId::Brush: return std::make_unique<BrushTool>();
    case ToolId::Eraser: return std::make_unique<EraserTool>();
    case ToolId::Fill: return std::make_unique<FillTool>();
    case ToolId::Eyedropper: return std::make_unique<EyedropperTool>();
    case ToolId::Line: return std::make_unique<ShapeTool>(true);
    case ToolId::Shape: return std::make_unique<ShapeTool>(false);
    case ToolId::Text: return std::make_unique<TextTool>();
    case ToolId::SelectRect: return std::make_unique<SelectTool>(false);
    case ToolId::SelectFree: return std::make_unique<SelectTool>(true);
    default: return nullptr;
    }
}
