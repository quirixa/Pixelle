#include "DrawingDocument.h"

DrawingDocument::DrawingDocument(QObject *parent) : QObject(parent) {
    newImage(QSize(1152, 648));
}

void DrawingDocument::newImage(const QSize &size, const QColor &fill) {
    QSize s = size.expandedTo(QSize(1, 1));
    m_image = QImage(s, QImage::Format_ARGB32);
    m_image.fill(fill);
    m_path.clear();
    emit imageReplaced();
    setDirty(false);
}

bool DrawingDocument::setImage(const QImage &img, const QString &path) {
    if (img.isNull()) return false;
    m_image = img.convertToFormat(QImage::Format_ARGB32);
    m_path = path;
    emit imageReplaced();
    setDirty(false);
    return true;
}

void DrawingDocument::setDirty(bool d) {
    if (m_dirty == d) return;
    m_dirty = d;
    emit dirtyChanged(d);
}

void DrawingDocument::markModified(const QRect &changed) {
    emit imageChanged(changed.isNull() ? m_image.rect() : changed);
    setDirty(true);
}
