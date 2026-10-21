#pragma once
#include <QPoint>
#include <QWidget>

class DrawingDocument;
class Editor;

// Renders the document at a zoom level, forwards pointer input to the Editor in
// IMAGE coordinates, and draws the editor's overlay (previews, floating selection,
// selection outline). Zoom never alters the image's pixel dimensions.
class CanvasView : public QWidget {
    Q_OBJECT
public:
    explicit CanvasView(DrawingDocument *doc, QWidget *parent = nullptr);

    void setEditor(Editor *editor);
    double zoom() const { return m_zoom; }
    void setZoom(double z);                 // clamped to [0.0625, 16]
    void zoomIn();
    void zoomOut();
    QPoint origin() const { return m_origin; } // viewport position of image (0,0)

    // Coordinate mapping (pure, testable).
    QPoint viewportToImage(const QPoint &vp) const;   // floor division
    QPoint imageToViewport(const QPoint &img) const;  // top-left of image pixel
    bool imageContains(const QPoint &imgPt) const;

signals:
    void zoomChanged(double zoom);
    void cursorMoved(const QPoint &imagePos);   // may lie outside the image
    void cursorLeft();

protected:
    void paintEvent(QPaintEvent *) override;
    QSize sizeHint() const override;
    void mousePressEvent(QMouseEvent *) override;
    void mouseMoveEvent(QMouseEvent *) override;
    void mouseReleaseEvent(QMouseEvent *) override;
    void leaveEvent(QEvent *) override;
    void wheelEvent(QWheelEvent *) override;

private:
    void updateGeometryFromDoc();
    class QScrollArea *scrollArea() const;
    DrawingDocument *m_doc;
    Editor *m_editor = nullptr;
    double m_zoom = 1.0;
    QPoint m_origin;
    bool m_panning = false;
    QPoint m_panLast;
};
