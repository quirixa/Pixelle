#include "ImageOps.h"
#include <cstring>

namespace ImageOps {

static QImage argb(const QImage &i) { return i.format() == QImage::Format_ARGB32 ? i : i.convertToFormat(QImage::Format_ARGB32); }

bool validSize(const QSize &s) {
    return s.width() >= 1 && s.height() >= 1 && s.width() <= kMaxDimension && s.height() <= kMaxDimension;
}

QImage cropped(const QImage &img, const QRect &r) {
    const QRect c = r.normalized().intersected(img.rect());
    if (c.isEmpty()) return {};
    return argb(img.copy(c));
}

QImage resized(const QImage &img, const QSize &size, bool smooth) {
    if (!validSize(size) || img.isNull()) return {};
    return argb(img.scaled(size, Qt::IgnoreAspectRatio, smooth ? Qt::SmoothTransformation : Qt::FastTransformation));
}

QImage canvasResized(const QImage &img, const QSize &size, const QColor &fill) {
    if (!validSize(size) || img.isNull()) return {};
    QImage out(size, QImage::Format_ARGB32);
    out.fill(fill);
    const QImage src = argb(img);
    const int w = std::min(src.width(), size.width()), h = std::min(src.height(), size.height());
    for (int y = 0; y < h; ++y) std::memcpy(out.scanLine(y), src.constScanLine(y), size_t(w) * 4);
    return out;
}

QImage rotated(const QImage &in, Rotation r) {
    const QImage src = argb(in);
    const int w = src.width(), h = src.height();
    if (r == Rotation::Rotate180) return src.mirrored(true, true);
    QImage out(h, w, QImage::Format_ARGB32);   // swapped dimensions
    for (int y = 0; y < h; ++y) {
        const QRgb *s = reinterpret_cast<const QRgb *>(src.constScanLine(y));
        for (int x = 0; x < w; ++x) {
            if (r == Rotation::Cw90) reinterpret_cast<QRgb *>(out.scanLine(x))[h - 1 - y] = s[x];
            else                     reinterpret_cast<QRgb *>(out.scanLine(w - 1 - x))[y] = s[x];
        }
    }
    return out;
}

QImage flipped(const QImage &img, bool horizontal) {
    return argb(img).mirrored(horizontal, !horizontal);
}

} // namespace ImageOps
