#include <QtTest>
#include <QPainter>
#include "drawing/Raster.h"
#include "drawing/ImageOps.h"
#include "drawing/DrawingDocument.h"
#include "history/UndoStack.h"

static QImage white(int w, int h) { QImage i(w, h, QImage::Format_ARGB32); i.fill(Qt::white); return i; }
static const QRgb BLACK = 0xFF000000u, WHITE = 0xFFFFFFFFu, RED = 0xFFFF0000u, BLUE = 0xFF0000FFu;
static int countNot(const QImage &i, QRgb c) { int n = 0; for (int y = 0; y < i.height(); ++y) for (int x = 0; x < i.width(); ++x) if (i.pixel(x, y) != c) ++n; return n; }

class TestRaster : public QObject {
    Q_OBJECT
private slots:
    // ---------------- pencil / eraser ----------------
    void pencil_horizontalLineExact() {
        QImage i = white(20, 10);
        Raster::pencilSegment(i, {2, 2}, {10, 2}, Qt::black, 1);
        for (int x = 2; x <= 10; ++x) QCOMPARE(i.pixel(x, 2), BLACK);
        QCOMPARE(i.pixel(1, 2), WHITE); QCOMPARE(i.pixel(11, 2), WHITE); QCOMPARE(i.pixel(5, 3), WHITE);
        QCOMPARE(countNot(i, WHITE), 9);
    }
    void pencil_diagonalAndSinglePoint() {
        QImage i = white(10, 10);
        Raster::pencilSegment(i, {0, 0}, {5, 5}, Qt::black, 1);
        for (int k = 0; k <= 5; ++k) QCOMPARE(i.pixel(k, k), BLACK);
        QCOMPARE(countNot(i, WHITE), 6);
        QImage j = white(5, 5);
        Raster::pencilSegment(j, {2, 2}, {2, 2}, Qt::black, 1);
        QCOMPARE(countNot(j, WHITE), 1);
    }
    void pencil_sizeAndClipping() {
        QImage i = white(10, 10);
        Raster::pencilSegment(i, {5, 5}, {5, 5}, Qt::black, 3);
        QCOMPARE(countNot(i, WHITE), 9);
        Raster::pencilSegment(i, {-20, -20}, {-10, -10}, Qt::black, 3);   // fully outside: no crash/no change
        QCOMPARE(countNot(i, WHITE), 9);
    }
    void pencil_alphaBlends() {
        QImage i = white(3, 3);
        Raster::pencilSegment(i, {1, 1}, {1, 1}, QColor(0, 0, 0, 128), 1);
        const QRgb p = i.pixel(1, 1);
        QVERIFY(qRed(p) > 100 && qRed(p) < 150);
        QCOMPARE(qAlpha(p), 255);
    }
    void eraser_backgroundAndTransparent() {
        QImage i(8, 8, QImage::Format_ARGB32); i.fill(Qt::black);
        Raster::eraseSegment(i, {3, 3}, {3, 3}, 3, Qt::white, false);
        QCOMPARE(countNot(i, BLACK), 9);
        QCOMPARE(i.pixel(3, 3), WHITE);
        Raster::eraseSegment(i, {6, 6}, {6, 6}, 1, Qt::white, true);
        QCOMPARE(qAlpha(i.pixel(6, 6)), 0);
    }
    // ---------------- brushes ----------------
    void brush_roundPaintsAlongPathOnly() {
        QImage i = white(60, 30); QRandomGenerator rng(1);
        Raster::brushSegment(i, BrushType::Round, 5, {10, 15}, {50, 15}, Qt::red, rng, true);
        QCOMPARE(i.pixel(30, 15), RED);
        QCOMPARE(i.pixel(30, 5), WHITE);
        QCOMPARE(i.pixel(2, 15), WHITE);
        QVERIFY(i.pixel(10, 15) != WHITE);
    }
    void brush_markerIsHardEdged() {
        QImage i = white(40, 20); QRandomGenerator rng(1);
        Raster::brushSegment(i, BrushType::Marker, 6, {10, 10}, {30, 10}, Qt::red, rng, false);
        for (int px : i.constBits() ? QList<int>{15, 20, 25} : QList<int>{}) QCOMPARE(i.pixel(px, 10), RED);
        for (int y = 0; y < 20; ++y) for (int x = 0; x < 40; ++x) { QRgb p = i.pixel(x, y); QVERIFY(p == WHITE || p == RED); }
    }
    void brush_calligraphyDirectionality() {
        QRandomGenerator rng(1);
        QImage thin = white(60, 60), thick = white(60, 60);
        // nib of Calligraphy1 lies along (1,-1): moving along it is thin, across it is thick
        Raster::brushSegment(thin, BrushType::Calligraphy1, 12, {10, 50}, {50, 10}, Qt::black, rng, true);
        Raster::brushSegment(thick, BrushType::Calligraphy1, 12, {10, 10}, {50, 50}, Qt::black, rng, true);
        auto colCount = [](const QImage &im, int x) { int n = 0; for (int y = 0; y < im.height(); ++y) if (qAlpha(im.pixel(x, y)) && im.pixel(x, y) != WHITE) ++n; return n; };
        QVERIFY2(colCount(thin, 30) <= 4, qPrintable(QString::number(colCount(thin, 30))));
        QVERIFY2(colCount(thick, 30) >= 8, qPrintable(QString::number(colCount(thick, 30))));
    }
    void brush_airbrushDeterministicAndBounded() {
        QImage a = white(80, 80), b = white(80, 80);
        QRandomGenerator r1(42), r2(42);
        Raster::brushSegment(a, BrushType::Airbrush, 10, {40, 40}, {40, 40}, Qt::black, r1, true);
        Raster::brushSegment(b, BrushType::Airbrush, 10, {40, 40}, {40, 40}, Qt::black, r2, true);
        QCOMPARE(a, b);
        QVERIFY(countNot(a, WHITE) > 0);
        for (int y = 0; y < 80; ++y) for (int x = 0; x < 80; ++x)
            if (a.pixel(x, y) != WHITE) QVERIFY(std::hypot(x - 40, y - 40) <= 10 * 1.5 + 1.5);
    }
    void brush_crayonIsStippled() {
        QImage i = white(40, 40); QRandomGenerator rng(7);
        Raster::brushSegment(i, BrushType::Crayon, 12, {20, 20}, {20, 20}, Qt::black, rng, true);
        const int n = countNot(i, WHITE);
        QVERIFY(n > 20 && n < 113);    // some, but not all, of the ~113-pixel disc
    }
    // ---------------- flood fill ----------------
    void fill_insideRectOnly_andBoundsAndCallbackOrder() {
        QImage i = white(30, 30);
        { QPainter p(&i); p.setPen(Qt::black); p.drawRect(5, 5, 19, 19); }
        QRect seenBounds; QRgb pixelAtCallback = 0;
        QRect r = Raster::floodFill(i, {15, 15}, Qt::red, 0, [&](const QRect &b) { seenBounds = b; pixelAtCallback = i.pixel(15, 15); });
        QCOMPARE(pixelAtCallback, WHITE);                  // callback ran BEFORE any write
        QCOMPARE(seenBounds, r);
        QCOMPARE(r, QRect(6, 6, 18, 18));
        QCOMPARE(i.pixel(15, 15), RED); QCOMPARE(i.pixel(6, 6), RED); QCOMPARE(i.pixel(23, 23), RED);
        QCOMPARE(i.pixel(5, 5), BLACK);                    // border untouched
        QCOMPARE(i.pixel(2, 2), WHITE); QCOMPARE(i.pixel(28, 28), WHITE);
        QCOMPARE(countNot(i, WHITE) - countNot(i, WHITE) + 18 * 18, 324);
    }
    void fill_noOpWhenSameColor() {
        QImage i = white(10, 10);
        bool called = false;
        QRect r = Raster::floodFill(i, {1, 1}, Qt::white, 0, [&](const QRect &) { called = true; });
        QVERIFY(r.isEmpty()); QVERIFY(!called);
    }
    void fill_doesNotLeakThroughDiagonal() {
        QImage i = white(12, 12);
        for (int k = 0; k < 12; ++k) i.setPixel(k, k, BLACK);           // 8-connected diagonal wall
        Raster::floodFill(i, {8, 2}, Qt::red, 0);                       // upper-right triangle
        QCOMPARE(i.pixel(8, 2), RED);
        QCOMPARE(i.pixel(2, 8), WHITE);                                 // lower-left must be untouched
    }
    void fill_wholeImageAndOutOfRange() {
        QImage i = white(16, 9);
        QCOMPARE(Raster::floodFill(i, {0, 0}, Qt::blue, 0), QRect(0, 0, 16, 9));
        QCOMPARE(countNot(i, BLUE), 0);
        QVERIFY(Raster::floodFill(i, {-1, 3}, Qt::red, 0).isEmpty());
        QVERIFY(Raster::floodFill(i, {16, 3}, Qt::red, 0).isEmpty());
    }
    void fill_toleranceMergesSimilarColours() {
        QImage i = white(10, 1); i.setPixel(5, 0, qRgb(250, 250, 250)); i.setPixel(6, 0, qRgb(100, 100, 100));
        QImage a = i, b = i;
        Raster::floodFill(a, {0, 0}, Qt::red, 0);
        QCOMPARE(a.pixel(5, 0), qRgb(250, 250, 250));      // exact: stops at the near-white pixel
        Raster::floodFill(b, {0, 0}, Qt::red, 10);
        QCOMPARE(b.pixel(5, 0), RED);                      // tolerant: includes it
        QCOMPARE(b.pixel(6, 0), qRgb(100, 100, 100));      // but not the dark one
    }
    void fill_largeImageCompletes() {
        QImage i = white(3000, 2000);
        QElapsedTimer t; t.start();
        QCOMPARE(Raster::floodFill(i, {10, 10}, Qt::black, 0), QRect(0, 0, 3000, 2000));
        QVERIFY2(t.elapsed() < 3000, "flood fill too slow");
    }
    // ---------------- shapes & lines ----------------
    void line_horizontalAndDiagonalExact() {
        QImage i = white(20, 20);
        Raster::drawLineShape(i, {0, 0}, {2, 3}, {12, 3}, Qt::black, 1, false);
        for (int x = 2; x <= 12; ++x) QCOMPARE(i.pixel(x, 3), BLACK);
        QCOMPARE(countNot(i, WHITE), 11);
        QImage j = white(20, 20);
        Raster::drawLineShape(j, {0, 0}, {0, 0}, {10, 10}, Qt::black, 1, false);
        for (int k = 0; k <= 10; ++k) QCOMPARE(j.pixel(k, k), BLACK);
    }
    void line_thickCoversWidth() {
        QImage i = white(40, 20);
        Raster::drawLineShape(i, {0, 0}, {5, 10}, {35, 10}, Qt::black, 5, false);
        for (int y = 8; y <= 12; ++y) QCOMPARE(i.pixel(20, y), BLACK);
        QCOMPARE(i.pixel(20, 5), WHITE); QCOMPARE(i.pixel(20, 15), WHITE);
    }
    void shape_rectOutline() {
        QImage i = white(40, 30);
        Raster::drawShape(i, {0, 0}, ShapeType::Rectangle, QRect(QPoint(10, 10), QPoint(29, 19)), Qt::black, Qt::red, 1, FillMode::Outline, false);
        for (int x = 10; x <= 29; ++x) { QCOMPARE(i.pixel(x, 10), BLACK); QCOMPARE(i.pixel(x, 19), BLACK); }
        for (int y = 10; y <= 19; ++y) { QCOMPARE(i.pixel(10, y), BLACK); QCOMPARE(i.pixel(29, y), BLACK); }
        QCOMPARE(i.pixel(20, 15), WHITE);            // interior untouched
        QCOMPARE(i.pixel(9, 10), WHITE); QCOMPARE(i.pixel(30, 10), WHITE); QCOMPARE(i.pixel(10, 20), WHITE);
        QCOMPARE(countNot(i, WHITE), 2 * 20 + 2 * 8);
    }
    void shape_rectFillModes() {
        const QRect r(QPoint(10, 10), QPoint(29, 19));
        QImage f = white(40, 30);
        Raster::drawShape(f, {0, 0}, ShapeType::Rectangle, r, Qt::black, Qt::red, 1, FillMode::Fill, false);
        QCOMPARE(countNot(f, WHITE), 20 * 10);       // fill only: primary colour, exact area
        QCOMPARE(f.pixel(10, 10), BLACK); QCOMPARE(f.pixel(29, 19), BLACK);
        QImage b = white(40, 30);
        Raster::drawShape(b, {0, 0}, ShapeType::Rectangle, r, Qt::black, Qt::red, 1, FillMode::OutlineAndFill, false);
        QCOMPARE(b.pixel(10, 10), BLACK); QCOMPARE(b.pixel(20, 15), RED);
        QCOMPARE(b.pixel(11, 11), RED);
    }
    void shape_thickOutlineStaysInsideBounds() {
        QImage i = white(40, 30);
        Raster::drawShape(i, {0, 0}, ShapeType::Rectangle, QRect(QPoint(10, 10), QPoint(29, 19)), Qt::black, Qt::red, 3, FillMode::Outline, false);
        QCOMPARE(i.pixel(10, 10), BLACK); QCOMPARE(i.pixel(12, 12), BLACK); QCOMPARE(i.pixel(13, 13), WHITE);
        QCOMPARE(i.pixel(9, 15), WHITE);
    }
    void shape_ellipseSymmetricAndHollow() {
        QImage i = white(41, 31);
        Raster::drawShape(i, {0, 0}, ShapeType::Ellipse, QRect(QPoint(5, 5), QPoint(35, 25)), Qt::black, Qt::red, 1, FillMode::OutlineAndFill, false);
        QCOMPARE(i.pixel(20, 15), RED);
        QCOMPARE(i.pixel(5, 5), WHITE);               // bbox corner is outside the ellipse
        QVERIFY(i.pixel(20, 5) == BLACK || i.pixel(20, 6) == BLACK);
        // Qt's aliased ellipse rasteriser is mirror-symmetric only to +/-1px (documented limitation):
        for (int y = 0; y < 31; ++y) { int diff = 0; for (int x = 0; x < 20; ++x) diff += i.pixel(x, y) != i.pixel(40 - x, y); QVERIFY2(diff <= 2, qPrintable(QString("row %1 asymmetry %2").arg(y).arg(diff))); }
        // Antialiased mode is symmetric to within rounding of the coverage values:
        QImage a = white(41, 31);
        Raster::drawShape(a, {0, 0}, ShapeType::Ellipse, QRect(QPoint(5, 5), QPoint(35, 25)), Qt::black, Qt::red, 1, FillMode::OutlineAndFill, true);
        for (int y = 0; y < 31; ++y) for (int x = 0; x < 20; ++x) QVERIFY(qAbs(qGray(a.pixel(x, y)) - qGray(a.pixel(40 - x, y))) <= 24);
    }
    void shape_polygonsFitBounds() {
        for (ShapeType t : {ShapeType::Triangle, ShapeType::Diamond, ShapeType::Pentagon, ShapeType::Hexagon, ShapeType::Star}) {
            QPolygonF p = Raster::shapePolygon(t, QRectF(10, 20, 50, 40));
            QVERIFY(!p.isEmpty());
            const QRectF b = p.boundingRect();
            QVERIFY(qAbs(b.left() - 10) < 1e-6 && qAbs(b.top() - 20) < 1e-6 && qAbs(b.right() - 60) < 1e-6 && qAbs(b.bottom() - 60) < 1e-6);
        }
        QImage i = white(40, 40);
        Raster::drawShape(i, {0, 0}, ShapeType::Triangle, QRect(5, 5, 30, 30), Qt::black, Qt::red, 1, FillMode::Fill, false);
        QCOMPARE(i.pixel(20, 30), BLACK);   // inside, near base
        QCOMPARE(i.pixel(6, 6), WHITE);     // top-left of bbox is outside a triangle
    }
    void shape_originTranslatesForPreviewOverlays() {
        QImage full = white(60, 60), overlay(30, 30, QImage::Format_ARGB32); overlay.fill(Qt::transparent);
        const QRect r(QPoint(20, 20), QPoint(39, 39));
        Raster::drawShape(full, {0, 0}, ShapeType::Rectangle, r, Qt::black, Qt::red, 1, FillMode::Outline, false);
        Raster::drawShape(overlay, {15, 15}, ShapeType::Rectangle, r, Qt::black, Qt::red, 1, FillMode::Outline, false);
        for (int y = 0; y < 30; ++y) for (int x = 0; x < 30; ++x)
            QCOMPARE(qAlpha(overlay.pixel(x, y)) != 0, full.pixel(x + 15, y + 15) != WHITE);   // preview == final
    }
    // ---------------- text ----------------
    void text_rendersColouredPixels() {
        QFont f("Noto Sans", 14);
        QImage t = Raster::renderText("Hi", f, Qt::red, false);
        QVERIFY(!t.isNull() && t.width() > 5 && t.height() > 5);
        int red = 0, other = 0;
        for (int y = 0; y < t.height(); ++y) for (int x = 0; x < t.width(); ++x) { QRgb p = t.pixel(x, y); if (qAlpha(p)) { if ((p & 0xFFFFFF) == 0xFF0000) ++red; else ++other; } }
        QVERIFY(red > 10); QCOMPARE(other, 0);          // aliased: only the chosen colour
        QVERIFY(Raster::renderText("", f, Qt::red, false).isNull());
        QImage two = Raster::renderText("Hi\nHi", f, Qt::red, false);
        QVERIFY(two.height() > t.height() + 5);          // multi-line grows
    }
    // ---------------- image operations ----------------
    void ops_cropClampsAndRejects() {
        QImage i = white(20, 10); i.setPixel(5, 5, RED);
        QImage c = ImageOps::cropped(i, QRect(4, 4, 3, 3));
        QCOMPARE(c.size(), QSize(3, 3)); QCOMPARE(c.pixel(1, 1), RED);
        QCOMPARE(ImageOps::cropped(i, QRect(15, 5, 100, 100)).size(), QSize(5, 5));    // clamped
        QVERIFY(ImageOps::cropped(i, QRect(30, 30, 5, 5)).isNull());
    }
    void ops_resizeAndCanvasResize() {
        QImage i = white(20, 10); i.setPixel(0, 0, RED);
        QCOMPARE(ImageOps::resized(i, QSize(40, 20), false).size(), QSize(40, 20));
        QCOMPARE(ImageOps::resized(i, QSize(40, 20), false).pixel(1, 1), RED);          // nearest: 2x2 block
        QVERIFY(ImageOps::resized(i, QSize(0, 5), true).isNull());
        QVERIFY(ImageOps::resized(i, QSize(ImageOps::kMaxDimension + 1, 5), true).isNull());
        QImage grown = ImageOps::canvasResized(i, QSize(30, 15), Qt::blue);
        QCOMPARE(grown.size(), QSize(30, 15));
        QCOMPARE(grown.pixel(0, 0), RED); QCOMPARE(grown.pixel(19, 9), WHITE);
        QCOMPARE(grown.pixel(25, 5), BLUE); QCOMPARE(grown.pixel(5, 12), BLUE);
        QImage shrunk = ImageOps::canvasResized(i, QSize(10, 5), Qt::blue);
        QCOMPARE(shrunk.size(), QSize(10, 5)); QCOMPARE(shrunk.pixel(0, 0), RED); QCOMPARE(shrunk.pixel(9, 4), WHITE);
    }
    void ops_rotateExactPixels() {
        QImage i = white(4, 2); i.setPixel(0, 0, RED); i.setPixel(3, 0, BLUE);   // 4 wide, 2 tall
        QImage cw = ImageOps::rotated(i, ImageOps::Rotation::Cw90);
        QCOMPARE(cw.size(), QSize(2, 4));
        QCOMPARE(cw.pixel(1, 0), RED);      // top-left -> top-right
        QCOMPARE(cw.pixel(1, 3), BLUE);     // top-right -> bottom-right
        QImage ccw = ImageOps::rotated(i, ImageOps::Rotation::Ccw90);
        QCOMPARE(ccw.pixel(0, 3), RED);     // top-left -> bottom-left
        QCOMPARE(ccw.pixel(0, 0), BLUE);    // top-right -> top-left
        QImage r180 = ImageOps::rotated(i, ImageOps::Rotation::Rotate180);
        QCOMPARE(r180.pixel(3, 1), RED); QCOMPARE(r180.pixel(0, 1), BLUE);
        QCOMPARE(ImageOps::rotated(cw, ImageOps::Rotation::Ccw90), i);                // inverse
        QCOMPARE(ImageOps::rotated(ImageOps::rotated(i, ImageOps::Rotation::Cw90), ImageOps::Rotation::Cw90), r180);
    }
    void ops_flip() {
        QImage i = white(4, 2); i.setPixel(0, 0, RED);
        QCOMPARE(ImageOps::flipped(i, true).pixel(3, 0), RED);
        QCOMPARE(ImageOps::flipped(i, false).pixel(0, 1), RED);
        QCOMPARE(ImageOps::flipped(ImageOps::flipped(i, true), true), i);
    }
    void ops_preserveAlpha() {
        QImage i(4, 4, QImage::Format_ARGB32); i.fill(Qt::transparent); i.setPixel(1, 1, qRgba(10, 20, 30, 77));
        QCOMPARE(qAlpha(ImageOps::rotated(i, ImageOps::Rotation::Cw90).pixel(2, 1)), 77);
        QCOMPARE(qAlpha(ImageOps::flipped(i, true).pixel(2, 1)), 77);
        QCOMPARE(qAlpha(ImageOps::canvasResized(i, QSize(6, 6), Qt::white).pixel(1, 1)), 77);
    }
    // ---------------- undo transactions ----------------
    void undo_tileTransactionRestoresExactlyAndIsSmall() {
        DrawingDocument d; d.newImage(QSize(4000, 4000));
        UndoStack u(&d);
        const QImage original = d.image();
        u.beginRegion();
        QRect r(100, 100, 30, 30);
        u.touch(r);
        QPainter p(&d.mutableImage()); p.fillRect(r, Qt::red); p.end();
        QVERIFY(u.commitRegion());
        QVERIFY(u.memoryUsage() < 200 * 1024);           // vs 64 MB for a full snapshot
        QVERIFY(d.image() != original);
        u.undo(); QCOMPARE(d.image(), original);
        u.redo(); QCOMPARE(d.image().pixel(110, 110), RED);
    }
    void undo_longStrokeAcrossTiles() {
        DrawingDocument d; d.newImage(QSize(500, 500));
        UndoStack u(&d);
        const QImage original = d.image();
        u.beginRegion();
        QPoint last(10, 10);
        for (int k = 1; k <= 40; ++k) {
            QPoint nx(10 + k * 11, 10 + k * 9);
            u.touch(Raster::strokeBounds(last, nx, 6));
            QRandomGenerator rng(1);
            Raster::brushSegment(d.mutableImage(), BrushType::Round, 5, last, nx, Qt::black, rng, true);
            last = nx;
        }
        QVERIFY(u.commitRegion());
        QCOMPARE(u.count(), 1);
        u.undo(); QCOMPARE(d.image(), original);
    }
    void undo_abortRestoresEverything() {
        DrawingDocument d; d.newImage(QSize(200, 200));
        UndoStack u(&d);
        const QImage original = d.image();
        u.beginRegion(); u.touch(QRect(0, 0, 150, 150));
        QPainter p(&d.mutableImage()); p.fillRect(0, 0, 150, 150, Qt::red); p.end();
        u.abortRegion();
        QCOMPARE(d.image(), original); QCOMPARE(u.count(), 0); QVERIFY(!d.isDirty());
    }
    void undo_noNetChangeRecordsNothing() {
        DrawingDocument d; d.newImage(QSize(100, 100));
        UndoStack u(&d);
        u.beginRegion(); u.touch(QRect(10, 10, 20, 20));
        QVERIFY(!u.commitRegion());
        QCOMPARE(u.count(), 0); QVERIFY(!d.isDirty());
    }
    void undo_semiTransparentPixelsRestoreBitExact() {
        DrawingDocument d; d.newImage(QSize(10, 10), Qt::transparent);
        d.mutableImage().setPixel(3, 3, qRgba(11, 22, 33, 44));
        UndoStack u(&d);
        const QImage original = d.image();
        u.beginRegion(); u.touch(QRect(0, 0, 10, 10));
        d.mutableImage().fill(Qt::red);
        QVERIFY(u.commitRegion());
        u.undo(); QCOMPARE(d.image(), original); QCOMPARE(d.image().pixel(3, 3), qRgba(11, 22, 33, 44));
    }
    void undo_dirtyTracksSavedPosition() {
        DrawingDocument d; d.newImage(QSize(50, 50));
        UndoStack u(&d);
        auto edit = [&](int x) { u.beginRegion(); u.touch(QRect(x, 0, 1, 1)); d.mutableImage().setPixel(x, 0, BLACK); u.commitRegion(); };
        QVERIFY(!d.isDirty());
        edit(1); QVERIFY(d.isDirty());
        u.markClean(); d.setDirty(false);
        edit(2); QVERIFY(d.isDirty());
        u.undo(); QVERIFY(!d.isDirty());            // back at saved state
        u.undo(); QVERIFY(d.isDirty());             // before saved state
        u.redo(); QVERIFY(!d.isDirty());
        u.undo(); edit(3);                          // new branch discards the saved state
        u.undo(); u.redo(); QVERIFY(d.isDirty());
        u.undo(); u.undo(); QVERIFY(d.isDirty());   // saved state is unreachable now
    }
    void undo_memoryCapEvictsOldest() {
        DrawingDocument d; d.newImage(QSize(256, 256));
        UndoStack u(&d, 300 * 1024);
        for (int k = 0; k < 20; ++k) { u.beginRegion(); u.touch(QRect(0, 0, 128, 128)); d.mutableImage().fill(QColor(k, 0, 0)); u.commitRegion(); }
        QVERIFY(u.memoryUsage() <= 300 * 1024 + 130 * 1024);
        QVERIFY(u.count() < 20 && u.count() >= 1);
    }
};
QTEST_MAIN(TestRaster)
#include "test_raster.moc"
