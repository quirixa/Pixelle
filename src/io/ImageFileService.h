#pragma once
#include <QImage>
#include <QString>
#include <QStringList>

struct FileResult {
    bool ok = false;
    QString error;
};

class ImageFileService {
public:
    static FileResult load(const QString &path, QImage *out);
    // Writes atomically-ish: refuses empty images, reports Qt's error string.
    // 'overwrite' must be true to replace an existing file (caller confirms).
    static FileResult save(const QImage &img, const QString &path, bool overwrite);
    static QStringList supportedWriteExtensions();
    static QString fileDialogFilter();
    // Returns lowercase extension, or "png" if none/unsupported.
    static QString normalizedExtension(const QString &path);
    // JPEG/BMP cannot hold alpha: true if saving 'img' to 'ext' would lose it.
    static bool wouldLoseTransparency(const QImage &img, const QString &ext);
};
