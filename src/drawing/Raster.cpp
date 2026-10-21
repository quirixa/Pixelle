#include "Raster.h"
#include <QPainter>
#include <QPainterPath>
#include <QFontMetrics>
#include <QtMath>
#include <algorithm>
#include <cmath>
#include <vector>

namespace {

template <class F> void forEachLinePoint(QPoint a, QPoint b, F f) {
    int dx = std::abs(b.x() - a.x()), dy = -std::abs(b.y() - a.y());
    const int sx = a.x() < b.x() ? 1 : -1, sy = a.y() < b.y() ? 1 : -1;
    int err = dx + dy, x = a.x(), y = a.y();
    for (;;) {
        f(x, y);
        if (x == b.x() && y == b.y()) break;
        const int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x += sx; }
        if (e2 <= dx) { err += dx; y += sy; }
    }
}

// Source-over for non-premultiplied ARGB.
QRgb over(QRgb dst, QRgb src) {
    const int sa = qAlpha(src);
    if (sa == 255) return src;
    if (sa == 0) return dst;
    const int da = qAlpha(dst);
    const int oa = sa + da * (255 - sa) / 255;
    if (oa == 0) return 0;
    auto ch = [&](int s, int d) { return (s * sa * 255 + d * da * (255 - sa) + oa * 127) / (oa * 255); };
    return qRgba(ch(qRed(src), qRed(dst)), ch(qGreen(src), qGreen(dst)), ch(qBlue(src), qBlue(dst)), oa);
}

inline bool ok(const QImage &img) { return img.format() == QImage::Format_ARGB32; }

inline void plot(QImage &img, int x, int y, QRgb c) {
    if (x < 0 || y < 0 || x >= img.width() || y >= img.height()) return;
    QRgb *line = reinterpret_cast<QRgb *>(img.scanLine(y));
    line[x] = over(line[x], c);
}
inline void setRaw(QImage &img, int x, int y, QRgb c) {
    if (x < 0 || y < 0 || x >= img.width() || y >= img.height()) return;
    reinterpret_cast<QRgb *>(img.scanLine(y))[x] = c;
}

template <class F> void stampSquare(int cx, int cy, int size, F f) {
    const int x0 = cx - size / 2, y0 = cy - size / 2;
    for (int y = y0; y < y0 + size; ++y)
        for (int x = x0; x < x0 + size; ++x) f(x, y);
}

} // namespace

namespace Raster {

QRect strokeBounds(const QPoint &a, const QPoint &b, int margin) {
    return QRect(a, b).normalized().adjusted(-margin, -margin, margin, margin);
}

int brushMargin(BrushType t, int size) {
    switch (t) {
    case BrushType::Airbrush: return size * 2 + 3;
    default: return size + 2;
    }
}

void pencilSegment(QImage &img, QPoint a, QPoint b, const QColor &c, int size) {
    if (!ok(img)) return;
    size = std::max(1, size);
    const QRgb rgb = c.rgba();
    forEachLinePoint(a, b, [&](int x, int y) {
        stampSquare(x, y, size, [&](int px, int py) { plot(img, px, py, rgb); });
    });
}

void eraseSegment(QImage &img, QPoint a, QPoint b, int size, const QColor &bg, bool toTransparent) {
    if (!ok(img)) return;
    size = std::max(1, size);
    const QRgb rgb = toTransparent ? 0u : bg.rgba();
    forEachLinePoint(a, b, [&](int x, int y) {
        stampSquare(x, y, size, [&](int px, int py) { setRaw(img, px, py, rgb); });
    });
}

void brushSegment(QImage &img, BrushType t, int size, QPoint a, QPoint b, const QColor &c,
                  QRandomGenerator &rng, bool aa) {
    if (!ok(img)) return;
    size = std::max(1, size);
    const QRgb rgb = c.rgba();
    const QPointF A(a.x() + 0.5, a.y() + 0.5), B(b.x() + 0.5, b.y() + 0.5);

    switch (t) {
    case BrushType::Round: {
        QPainter p(&img);
        p.setRenderHint(QPainter::Antialiasing, aa);
        p.setPen(QPen(c, size, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        p.drawLine(A, B);
        p.setPen(Qt::NoPen);
        p.setBrush(c);
        p.drawEllipse(B, size / 2.0, size / 2.0);
        p.drawEllipse(A, size / 2.0, size / 2.0);
        break;
    }
    case BrushType::Marker: {   // flat square nib
        QPainter p(&img);
        p.setRenderHint(QPainter::Antialiasing, false);
        p.setPen(QPen(c, size, Qt::SolidLine, Qt::SquareCap, Qt::MiterJoin));
        p.drawLine(A, B);
        break;
    }
    case BrushType::Calligraphy1:
    case BrushType::Calligraphy2: {   // 1px-thick nib of length 'size' at +/-45 degrees
        const double h = size / 2.0 * 0.70710678;
        const QPointF n(h, t == BrushType::Calligraphy1 ? -h : h);
        QPainter p(&img);
        p.setRenderHint(QPainter::Antialiasing, aa);
        p.setPen(QPen(c, 1));
        p.setBrush(c);
        p.drawPolygon(QPolygonF({A + n, A - n, B - n, B + n}));
        break;
    }
    case BrushType::Airbrush: {
        const double len = std::hypot(double(b.x() - a.x()), double(b.y() - a.y()));
        const int steps = std::max(1, int(len / 2.0));
        const double radius = size * 1.5;
        for (int i = 0; i <= steps; ++i) {
            const double f = steps ? double(i) / steps : 0.0;
            const double cx = a.x() + (b.x() - a.x()) * f, cy = a.y() + (b.y() - a.y()) * f;
            for (int k = 0; k < size; ++k) {
                const double ang = rng.generateDouble() * 2 * M_PI;
                const double r = std::sqrt(rng.generateDouble()) * radius;
                plot(img, qRound(cx + std::cos(ang) * r), qRound(cy + std::sin(ang) * r), rgb);
            }
        }
        break;
    }
    case BrushType::Crayon: {
        const int step = std::max(1, size / 4);
        const double len = std::hypot(double(b.x() - a.x()), double(b.y() - a.y()));
        const int steps = std::max(1, int(len / step));
        const double r = size / 2.0;
        for (int i = 0; i <= steps; ++i) {
            const double f = double(i) / steps;
            const int cx = qRound(a.x() + (b.x() - a.x()) * f), cy = qRound(a.y() + (b.y() - a.y()) * f);
            for (int dy = -int(r); dy <= int(r); ++dy)
                for (int dx = -int(r); dx <= int(r); ++dx)
                    if (dx * dx + dy * dy <= r * r && rng.bounded(100) < 55) plot(img, cx + dx, cy + dy, rgb);
        }
        break;
    }
    }
}

QRect floodFill(QImage &img, QPoint seed, const QColor &fill, int tol,
                const std::function<void(const QRect &)> &beforeWrite) {
    if (!ok(img) || !img.rect().contains(seed)) return {};
    const int W = img.width(), H = img.height();
    const QRgb target = img.pixel(seed), nf = fill.rgba();
    if (tol == 0 && target == nf) return {};

    auto match = [&](QRgb c) {
        return std::abs(qAlpha(c) - qAlpha(target)) <= tol && std::abs(qRed(c) - qRed(target)) <= tol &&
               std::abs(qGreen(c) - qGreen(target)) <= tol && std::abs(qBlue(c) - qBlue(target)) <= tol;
    };
    std::vector<uchar> seen(size_t(W) * H, 0);
    auto at = [&](int x, int y) { return reinterpret_cast<const QRgb *>(img.constScanLine(y))[x]; };
    auto free_ = [&](int x, int y) { return !seen[size_t(y) * W + x] && match(at(x, y)); };

    std::vector<QPoint> stack{seed};
    QRect bounds;
    int minX = W, minY = H, maxX = -1, maxY = -1;
    while (!stack.empty()) {
        const QPoint p = stack.back();
        stack.pop_back();
        if (!free_(p.x(), p.y())) continue;
        int xl = p.x(), xr = p.x();
        while (xl > 0 && free_(xl - 1, p.y())) --xl;
        while (xr < W - 1 && free_(xr + 1, p.y())) ++xr;
        for (int x = xl; x <= xr; ++x) seen[size_t(p.y()) * W + x] = 1;
        minX = std::min(minX, xl); maxX = std::max(maxX, xr);
        minY = std::min(minY, p.y()); maxY = std::max(maxY, p.y());
        for (int ny : {p.y() - 1, p.y() + 1}) {
            if (ny < 0 || ny >= H) continue;
            bool inSpan = false;
            for (int x = xl; x <= xr; ++x) {
                const bool f = free_(x, ny);
                if (f && !inSpan) stack.push_back(QPoint(x, ny));
                inSpan = f;
            }
        }
    }
    if (maxX < 0) return {};
    bounds = QRect(QPoint(minX, minY), QPoint(maxX, maxY));
    if (beforeWrite) beforeWrite(bounds);
    for (int y = bounds.top(); y <= bounds.bottom(); ++y) {
        QRgb *line = reinterpret_cast<QRgb *>(img.scanLine(y));
        for (int x = bounds.left(); x <= bounds.right(); ++x)
            if (seen[size_t(y) * W + x]) line[x] = nf;
    }
    return bounds;
}

QRect shapeBounds(const QRect &r, int width) {
    const int m = width + 2;
    return r.normalized().adjusted(-m, -m, m, m);
}

QPolygonF shapePolygon(ShapeType t, const QRectF &r) {
    std::vector<QPointF> u;   // unit-space points, then fitted to r
    auto ngon = [&](int n, double startDeg) {
        for (int k = 0; k < n; ++k) {
            const double a = qDegreesToRadians(startDeg + 360.0 * k / n);
            u.push_back(QPointF(std::cos(a), std::sin(a)));
        }
    };
    switch (t) {
    case ShapeType::Triangle: u = {{0.5, 0}, {1, 1}, {0, 1}}; break;
    case ShapeType::Diamond: u = {{0.5, 0}, {1, 0.5}, {0.5, 1}, {0, 0.5}}; break;
    case ShapeType::Pentagon: ngon(5, -90); break;
    case ShapeType::Hexagon: ngon(6, 0); break;
    case ShapeType::Star:
        for (int k = 0; k < 10; ++k) {
            const double a = qDegreesToRadians(-90.0 + 36.0 * k), rad = (k % 2) ? 0.382 : 1.0;
            u.push_back(QPointF(std::cos(a) * rad, std::sin(a) * rad));
        }
        break;
    default: break;
    }
    if (u.empty()) return {};
    double x0 = 1e9, y0 = 1e9, x1 = -1e9, y1 = -1e9;
    for (const QPointF &p : u) { x0 = std::min(x0, p.x()); y0 = std::min(y0, p.y()); x1 = std::max(x1, p.x()); y1 = std::max(y1, p.y()); }
    QPolygonF poly;
    for (const QPointF &p : u)
        poly << QPointF(r.left() + (p.x() - x0) / (x1 - x0) * r.width(), r.top() + (p.y() - y0) / (y1 - y0) * r.height());
    return poly;
}

void drawLineShape(QImage &img, QPoint origin, QPoint a, QPoint b, const QColor &c, int width, bool aa) {
    if (!ok(img)) return;
    width = std::max(1, width);
    if (!aa && width == 1) {   // exact Bresenham, endpoints included
        const QRgb rgb = c.rgba();
        forEachLinePoint(a - origin, b - origin, [&](int x, int y) { plot(img, x, y, rgb); });
        return;
    }
    QPainter p(&img);
    p.setRenderHint(QPainter::Antialiasing, aa);
    p.translate(-origin);
    p.setPen(QPen(c, width, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p.drawLine(QPointF(a.x() + 0.5, a.y() + 0.5), QPointF(b.x() + 0.5, b.y() + 0.5));
}

void drawShape(QImage &img, QPoint origin, ShapeType t, const QRect &rIn, const QColor &outline,
               const QColor &fill, int width, FillMode mode, bool aa) {
    if (!ok(img)) return;
    width = std::max(1, width);
    const QRect r = rIn.normalized();
    const bool doOutline = mode != FillMode::Fill, doFill = mode != FillMode::Outline;
    const QColor fc = (mode == FillMode::Fill) ? outline : fill;   // fill-only uses the primary colour

    QRectF rf(r);
    const double half = doOutline ? width / 2.0 : 0.0;
    rf.adjust(half, half, -half, -half);
    if (rf.width() < 0) rf.setWidth(0);
    if (rf.height() < 0) rf.setHeight(0);

    QPainter p(&img);
    p.setRenderHint(QPainter::Antialiasing, aa);
    p.translate(-origin);
    p.setPen(doOutline ? QPen(outline, width, Qt::SolidLine, Qt::SquareCap, Qt::MiterJoin) : QPen(Qt::NoPen));
    p.setBrush(doFill ? QBrush(fc) : QBrush(Qt::NoBrush));
    switch (t) {
    case ShapeType::Rectangle: p.drawRect(rf); break;
    case ShapeType::RoundedRectangle: {
        const double rad = std::min(rf.width(), rf.height()) * 0.25;
        p.drawRoundedRect(rf, rad, rad);
        break;
    }
    case ShapeType::Ellipse: p.drawEllipse(rf); break;
    default: p.drawPolygon(shapePolygon(t, rf)); break;
    }
}

QImage renderText(const QString &text, const QFont &fontIn, const QColor &color, bool aa) {
    if (text.isEmpty()) return {};
    QFont font = fontIn;
    // The TextAntialiasing render hint alone is ignored by the FreeType engine;
    // the style strategy is what actually yields crisp (aliased) glyphs.
    font.setStyleStrategy(aa ? QFont::PreferDefault : QFont::NoAntialias);
    const int flags = Qt::AlignLeft | Qt::AlignTop | Qt::TextDontClip;
    const QRect br = QFontMetrics(font).boundingRect(QRect(0, 0, 100000, 100000), flags, text);
    if (br.isEmpty()) return {};
    QImage img(br.size(), QImage::Format_ARGB32);
    img.fill(Qt::transparent);
    QPainter p(&img);
    p.setRenderHint(QPainter::TextAntialiasing, aa);
    p.setRenderHint(QPainter::Antialiasing, aa);
    p.setFont(font);
    p.setPen(color);
    p.translate(-br.topLeft());
    p.drawText(QRect(0, 0, 100000, 100000), flags, text);
    return img;
}

} // namespace Raster
