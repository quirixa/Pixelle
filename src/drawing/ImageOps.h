#pragma once
// Whole-image operations as pure functions (null QImage = invalid request).
#include <QColor>
#include <QImage>
#include <QRect>
#include <QSize>

namespace ImageOps {
constexpr int kMaxDimension = 16384;
enum class Rotation { Cw90, Ccw90, Rotate180 };

bool validSize(const QSize &s);
QImage cropped(const QImage &img, const QRect &r);
QImage resized(const QImage &img, const QSize &size, bool smooth);
// Top-left anchored; newly exposed area is filled with 'fill'.
QImage canvasResized(const QImage &img, const QSize &size, const QColor &fill);
QImage rotated(const QImage &img, Rotation r);
QImage flipped(const QImage &img, bool horizontal);
}
