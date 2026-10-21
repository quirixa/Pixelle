#include "ImageFileService.h"
#include <QFileInfo>
#include <QImageReader>
#include <QImageWriter>
#include <QPainter>
#include <QSaveFile>

FileResult ImageFileService::load(const QString &path, QImage *out) {
    QImageReader r(path);
    r.setAutoTransform(true);
    QImage img = r.read();
    if (img.isNull()) return {false, r.errorString()};
    *out = img;
    return {true, {}};
}

QString ImageFileService::normalizedExtension(const QString &path) {
    QString ext = QFileInfo(path).suffix().toLower();
    if (ext == "jpeg") ext = "jpg";
    return supportedWriteExtensions().contains(ext) ? ext : QStringLiteral("png");
}

bool ImageFileService::wouldLoseTransparency(const QImage &img, const QString &ext) {
    if (ext != "jpg" && ext != "bmp") return false;
    if (!img.hasAlphaChannel()) return false;
    for (int y = 0; y < img.height(); ++y)
        for (int x = 0; x < img.width(); ++x)
            if (qAlpha(img.pixel(x, y)) != 255) return true;
    return false;
}

FileResult ImageFileService::save(const QImage &img, const QString &path, bool overwrite) {
    if (img.isNull()) return {false, "Nothing to save."};
    if (QFileInfo::exists(path) && !overwrite)
        return {false, "File exists and overwrite was not confirmed."};

    const QString ext = normalizedExtension(path);
    QImage toWrite = img;
    if (ext == "jpg" || ext == "bmp") {
        // These formats have no alpha: flatten explicitly onto white
        // (callers should warn via wouldLoseTransparency()).
        QImage flat(img.size(), QImage::Format_RGB32);
        flat.fill(Qt::white);
        QPainter p(&flat);
        p.drawImage(0, 0, img);
        p.end();
        toWrite = flat;
    }

    QSaveFile f(path);   // writes to a temp file, commit() renames: no partial files
    if (!f.open(QIODevice::WriteOnly)) return {false, f.errorString()};
    QImageWriter w(&f, ext.toLatin1());
    if (ext == "jpg") w.setQuality(95);
    if (!w.write(toWrite)) {
        f.cancelWriting();
        return {false, w.errorString()};
    }
    if (!f.commit()) return {false, f.errorString()};
    return {true, {}};
}

QStringList ImageFileService::supportedWriteExtensions() {
    QStringList out;
    for (const QByteArray &b : QImageWriter::supportedImageFormats()) {
        QString s = QString::fromLatin1(b).toLower();
        if (s == "png" || s == "jpg" || s == "jpeg" || s == "bmp" || s == "webp" || s == "tiff")
            out << (s == "jpeg" ? QStringLiteral("jpg") : s);
    }
    out.removeDuplicates();
    return out;
}

QString ImageFileService::fileDialogFilter() {
    return "PNG (*.png);;JPEG (*.jpg *.jpeg);;BMP (*.bmp);;WebP (*.webp);;TIFF (*.tif *.tiff);;All files (*)";
}
