#pragma once
#include <QImage>
#include <QObject>
#include <QString>

// Owns the authoritative raster image and document state (dirty flag, path).
class DrawingDocument : public QObject {
    Q_OBJECT
public:
    explicit DrawingDocument(QObject *parent = nullptr);

    void newImage(const QSize &size, const QColor &fill = Qt::white);
    bool setImage(const QImage &img, const QString &path = {});

    const QImage &image() const { return m_image; }
    QImage &mutableImage() { return m_image; }

    bool isDirty() const { return m_dirty; }
    void setDirty(bool d);
    void markModified(const QRect &changed = QRect());
    // Repaint request that does NOT touch the dirty flag (live stroke feedback;
    // the dirty flag is owned by the undo stack when an edit is committed).
    void notifyChanged(const QRect &changed) { emit imageChanged(changed); }

    QString filePath() const { return m_path; }
    void setFilePath(const QString &p) { m_path = p; }

signals:
    void imageReplaced();                // whole image changed (new/open/resize)
    void imageChanged(const QRect &r);   // region changed
    void dirtyChanged(bool dirty);

private:
    QImage m_image;
    QString m_path;
    bool m_dirty = false;
};
