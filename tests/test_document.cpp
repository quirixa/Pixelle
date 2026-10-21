#include <QtTest>
#include <cmath>
#include <QPainter>
#include <QTemporaryDir>
#include "drawing/DrawingDocument.h"
#include "history/UndoStack.h"
#include "io/ImageFileService.h"
#include "ui/CanvasView.h"

class TestDocument : public QObject {
    Q_OBJECT
private slots:
    void newImage_sizeAndWhite() {
        DrawingDocument d; d.newImage(QSize(40, 30));
        QCOMPARE(d.image().size(), QSize(40, 30));
        QCOMPARE(d.image().pixel(0, 0), 0xFFFFFFFFu);
        QVERIFY(!d.isDirty());
    }
    void dirtyTransitions() {
        DrawingDocument d; d.newImage(QSize(10, 10));
        QSignalSpy spy(&d, &DrawingDocument::dirtyChanged);
        d.markModified();
        QVERIFY(d.isDirty());
        d.setDirty(false);
        QCOMPARE(spy.count(), 2);
    }
    void undoRedoRegion() {
        DrawingDocument d; d.newImage(QSize(20, 20));
        UndoStack u(&d);
        QRect r(5, 5, 4, 4);
        QImage before = d.image().copy(r);
        QPainter p(&d.mutableImage()); p.fillRect(r, Qt::red); p.end();
        u.pushRegion(r, before);
        QCOMPARE(d.image().pixel(6, 6), QColor(Qt::red).rgba());
        u.undo();
        QCOMPARE(d.image().pixel(6, 6), 0xFFFFFFFFu);
        u.redo();
        QCOMPARE(d.image().pixel(6, 6), QColor(Qt::red).rgba());
    }
    void zoomCoordinateRoundTrip() {
        DrawingDocument d; d.newImage(QSize(100, 100));
        CanvasView v(&d);
        for (double z : {1.0, 2.0, 3.0, 8.0}) {            // magnified: exact
            v.setZoom(z);
            QCOMPARE(d.image().size(), QSize(100, 100));   // zoom never resizes image
            for (QPoint ip : {QPoint(0, 0), QPoint(7, 13), QPoint(99, 99)}) {
                QPoint vp = v.imageToViewport(ip);
                QCOMPARE(v.viewportToImage(vp), ip);
                // every viewport pixel inside this image pixel's span maps back to it
                int span = int(z);
                QCOMPARE(v.viewportToImage(vp + QPoint(span - 1, span - 1)), ip);
                QCOMPARE(v.viewportToImage(vp + QPoint(span, 0)), ip + QPoint(1, 0));
            }
        }
        for (double z : {0.5, 0.25}) {                     // minified: many-to-one
            v.setZoom(z);
            QCOMPARE(d.image().size(), QSize(100, 100));
            const int tol = int(std::ceil(1.0 / z));
            for (QPoint ip : {QPoint(0, 0), QPoint(7, 13), QPoint(99, 99)}) {
                QPoint back = v.viewportToImage(v.imageToViewport(ip));
                QVERIFY(std::abs(back.x() - ip.x()) <= tol);
                QVERIFY(std::abs(back.y() - ip.y()) <= tol);
            }
        }
    }
    void canvasBounds() {
        DrawingDocument d; d.newImage(QSize(10, 10));
        CanvasView v(&d);
        QVERIFY(v.imageContains(QPoint(9, 9)));
        QVERIFY(!v.imageContains(QPoint(10, 0)));
        QVERIFY(!v.imageContains(QPoint(-1, 0)));
    }
    void pngPreservesAlpha() {
        QTemporaryDir dir; QVERIFY(dir.isValid());
        QImage img(8, 8, QImage::Format_ARGB32); img.fill(Qt::transparent);
        img.setPixel(1, 1, qRgba(10, 20, 30, 128));
        QString path = dir.filePath("a.png");
        QVERIFY(ImageFileService::save(img, path, false).ok);
        QImage back; QVERIFY(ImageFileService::load(path, &back).ok);
        QCOMPARE(qAlpha(back.pixel(0, 0)), 0);
        QCOMPARE(qAlpha(back.pixel(1, 1)), 128);
    }
    void saveRefusesOverwriteWithoutConfirm() {
        QTemporaryDir dir;
        QImage img(4, 4, QImage::Format_ARGB32); img.fill(Qt::white);
        QString path = dir.filePath("x.png");
        QVERIFY(ImageFileService::save(img, path, false).ok);
        QVERIFY(!ImageFileService::save(img, path, false).ok);
        QVERIFY(ImageFileService::save(img, path, true).ok);
    }
    void failedFileOps() {
        QImage out;
        QVERIFY(!ImageFileService::load("/nonexistent/file.png", &out).ok);
        QImage img(4, 4, QImage::Format_ARGB32); img.fill(Qt::white);
        FileResult r = ImageFileService::save(img, "/nonexistent_dir/x.png", false);
        QVERIFY(!r.ok); QVERIFY(!r.error.isEmpty());
    }
};
QTEST_MAIN(TestDocument)
#include "test_document.moc"
