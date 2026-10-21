#include "CanvasView.h"
#include "StyleTokens.h"
#include "drawing/DrawingDocument.h"
#include "drawing/Editor.h"
#include <QMouseEvent>
#include <QPainter>
#include <QScrollArea>
#include <QScrollBar>
#include <QWheelEvent>
#include <QtMath>
#include <algorithm>

CanvasView::CanvasView(DrawingDocument *doc, QWidget *parent) : QWidget(parent), m_doc(doc) {
    m_origin = QPoint(PaintStyle::kCanvasOffsetX, PaintStyle::kCanvasOffsetY);
    setMouseTracking(true);
    connect(doc, &DrawingDocument::imageReplaced, this, [this] { updateGeometryFromDoc(); });
    connect(doc, &DrawingDocument::imageChanged, this, [this](const QRect &) { update(); });
    updateGeometryFromDoc();
}

void CanvasView::setEditor(Editor *editor) {
    m_editor = editor;
    if (!editor) return;
    connect(editor, &Editor::previewChanged, this, qOverload<>(&QWidget::update));
    connect(editor, &Editor::selectionChanged, this, qOverload<>(&QWidget::update));
}

void CanvasView::setZoom(double z) {
    z = std::clamp(z, 0.0625, 16.0);
    if (qFuzzyCompare(z, m_zoom)) return;
    m_zoom = z;
    updateGeometryFromDoc();
    emit zoomChanged(m_zoom);
}

void CanvasView::zoomIn() {
    int n; const double *s = PaintStyle::zoomSteps(&n);
    for (int i = 0; i < n; ++i) if (s[i] > m_zoom + 1e-9) { setZoom(s[i]); return; }
}
void CanvasView::zoomOut() {
    int n; const double *s = PaintStyle::zoomSteps(&n);
    for (int i = n - 1; i >= 0; --i) if (s[i] < m_zoom - 1e-9) { setZoom(s[i]); return; }
}

QPoint CanvasView::viewportToImage(const QPoint &vp) const {
    return QPoint(qFloor((vp.x() - m_origin.x()) / m_zoom),
                  qFloor((vp.y() - m_origin.y()) / m_zoom));
}

QPoint CanvasView::imageToViewport(const QPoint &img) const {
    return QPoint(m_origin.x() + qRound(img.x() * m_zoom),
                  m_origin.y() + qRound(img.y() * m_zoom));
}

bool CanvasView::imageContains(const QPoint &p) const {
    return m_doc->image().rect().contains(p);
}

QSize CanvasView::sizeHint() const {
    const QSize s = m_doc->image().size();
    return QSize(qCeil(s.width() * m_zoom) + 2 * m_origin.x(),
                 qCeil(s.height() * m_zoom) + 2 * m_origin.y());
}

void CanvasView::updateGeometryFromDoc() {
    setMinimumSize(sizeHint());
    updateGeometry();
    update();
}

QScrollArea *CanvasView::scrollArea() const {
    for (QWidget *w = parentWidget(); w; w = w->parentWidget())
        if (auto *sa = qobject_cast<QScrollArea *>(w)) return sa;
    return nullptr;
}

void CanvasView::paintEvent(QPaintEvent *) {
    QPainter p(this);
    p.fillRect(rect(), PaintStyle::workspaceBackground());
    const QSize s = m_doc->image().size();
    const QRect dst(m_origin, QSize(qCeil(s.width() * m_zoom), qCeil(s.height() * m_zoom)));
    // Nearest-neighbour when magnified so pixels stay crisp; smooth when shrunk.
    p.setRenderHint(QPainter::SmoothPixmapTransform, m_zoom < 1.0);
    p.drawImage(dst, m_doc->image());
    p.setRenderHint(QPainter::SmoothPixmapTransform, false);
    p.setPen(PaintStyle::canvasBorder());
    p.drawRect(dst.adjusted(-1, -1, 0, 0));

    if (!m_editor) return;
    // Overlay (tool previews, floating selection) drawn in IMAGE space, clipped to the canvas.
    p.save();
    p.setClipRect(dst);
    p.translate(m_origin);
    p.scale(m_zoom, m_zoom);
    p.setRenderHint(QPainter::SmoothPixmapTransform, m_zoom < 1.0);
    m_editor->paintOverlay(p);
    p.restore();

    // Selection outline / rubber band: crisp 1px dashes in viewport space.
    QTransform tr;
    tr.translate(m_origin.x(), m_origin.y());
    tr.scale(m_zoom, m_zoom);
    auto outline = [&](const QPainterPath &path) {
        if (path.isEmpty()) return;
        const QPainterPath vp = tr.map(path);
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(Qt::white, 1));
        p.drawPath(vp);
        QPen dash(Qt::black, 1, Qt::DashLine);
        dash.setDashPattern({4, 4});
        p.setPen(dash);
        p.drawPath(vp);
    };
    outline(m_editor->selectionOutline());
    outline(m_editor->rubber());
}

void CanvasView::mousePressEvent(QMouseEvent *e) {
    setFocus();
    if (e->button() == Qt::MiddleButton) { m_panning = true; m_panLast = e->globalPosition().toPoint(); setCursor(Qt::ClosedHandCursor); return; }
    if (!m_editor) return;
    m_editor->pointerPress(viewportToImage(e->position().toPoint()), e->button(), e->modifiers());
}

void CanvasView::mouseMoveEvent(QMouseEvent *e) {
    if (m_panning) {
        if (QScrollArea *sa = scrollArea()) {
            const QPoint g = e->globalPosition().toPoint(), d = g - m_panLast;
            sa->horizontalScrollBar()->setValue(sa->horizontalScrollBar()->value() - d.x());
            sa->verticalScrollBar()->setValue(sa->verticalScrollBar()->value() - d.y());
            m_panLast = g;
        }
        return;
    }
    const QPoint ip = viewportToImage(e->position().toPoint());
    emit cursorMoved(ip);
    // Qt delivers moves to this widget while a button is held, even outside it (implicit grab).
    if (m_editor) m_editor->pointerMove(ip, e->modifiers());
}

void CanvasView::mouseReleaseEvent(QMouseEvent *e) {
    if (e->button() == Qt::MiddleButton) { m_panning = false; unsetCursor(); return; }
    if (!m_editor) return;
    m_editor->pointerRelease(viewportToImage(e->position().toPoint()), e->button(), e->modifiers());
}

void CanvasView::leaveEvent(QEvent *) { emit cursorLeft(); }

void CanvasView::wheelEvent(QWheelEvent *e) {
    if (e->modifiers() & Qt::ControlModifier) {
        if (e->angleDelta().y() > 0) zoomIn(); else if (e->angleDelta().y() < 0) zoomOut();
        e->accept();
    } else {
        e->ignore();   // let the scroll area scroll
    }
}
