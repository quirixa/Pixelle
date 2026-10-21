#pragma once
// Pure raster operations on ARGB32 QImages. No UI, no document: everything here
// is testable against pixels. Coordinates are image (document) coordinates.
#include <QColor>
#include <QFont>
#include <QImage>
#include <QPoint>
#include <QRandomGenerator>
#include <QRect>
#include <functional>

enum class BrushType { Round, Calligraphy1, Calligraphy2, Airbrush, Crayon, Marker };
enum class ShapeType { Rectangle, RoundedRectangle, Ellipse, Triangle, Diamond, Pentagon, Hexagon, Star };
enum class FillMode { Outline, Fill, OutlineAndFill };

namespace Raster {

// Rectangle that a stroke segment a->b can modify (callers touch() it for undo first).
QRect strokeBounds(const QPoint &a, const QPoint &b, int margin);
int brushMargin(BrushType t, int size);

// Aliased square-stamp line (exact pixels, both endpoints included).
void pencilSegment(QImage &img, QPoint a, QPoint b, const QColor &c, int size);
void eraseSegment(QImage &img, QPoint a, QPoint b, int size, const QColor &bg, bool toTransparent);
void brushSegment(QImage &img, BrushType t, int size, QPoint a, QPoint b, const QColor &c,
                  QRandomGenerator &rng, bool antialias);

// 4-connected flood fill. beforeWrite(bounds) is invoked once, with the bounding
// rect of the region that WILL change, before any pixel is modified (so the
// caller can record undo data). Returns that rect, or an empty rect if nothing to do.
QRect floodFill(QImage &img, QPoint seed, const QColor &fill, int tolerance,
                const std::function<void(const QRect &)> &beforeWrite = {});

// 'origin' is the document position of img's (0,0), so previews can be rendered
// into a small overlay image yet use document coordinates.
void drawLineShape(QImage &img, QPoint origin, QPoint a, QPoint b, const QColor &c, int width, bool aa);
void drawShape(QImage &img, QPoint origin, ShapeType t, const QRect &r, const QColor &outline,
               const QColor &fill, int width, FillMode mode, bool aa);
QRect shapeBounds(const QRect &r, int width);
QPolygonF shapePolygon(ShapeType t, const QRectF &r);

QImage renderText(const QString &text, const QFont &font, const QColor &color, bool aa);

} // namespace Raster
