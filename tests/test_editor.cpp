#include <QtTest>
#include <QPainter>
#include "drawing/Editor.h"

static const QRgb BLACK = 0xFF000000u, WHITE = 0xFFFFFFFFu, RED = 0xFFFF0000u, BLUE = 0xFF0000FFu, GREEN = 0xFF00FF00u;
static int countNot(const QImage &i, QRgb c) { int n = 0; for (int y = 0; y < i.height(); ++y) for (int x = 0; x < i.width(); ++x) if (i.pixel(x, y) != c) ++n; return n; }

struct Fx {
    Editor e;
    explicit Fx(int w = 100, int h = 80) { e.newDocument(QSize(w, h)); }
    const QImage &img() const { return e.document().image(); }
    void drag(QPoint a, QPoint b, Qt::MouseButton btn = Qt::LeftButton, Qt::KeyboardModifiers m = {}) {
        e.pointerPress(a, btn, m);
        e.pointerMove((a + b) / 2, m);
        e.pointerMove(b, m);
        e.pointerRelease(b, btn, m);
    }
    void fillRect(const QRect &r, QColor c) { QPainter p(&e.document().mutableImage()); p.fillRect(r, c); }
};

class TestEditor : public QObject {
    Q_OBJECT
private slots:
    // ================= pencil / brush / eraser =================
    void pencil_strokeUndoRedoDirty() {
        Fx f; f.e.setTool(ToolId::Pencil); f.e.setSize(1);
        const QImage before = f.img();
        QVERIFY(!f.e.document().isDirty());
        f.drag({10, 10}, {40, 10});
        for (int x = 10; x <= 40; ++x) QCOMPARE(f.img().pixel(x, 10), BLACK);
        QCOMPARE(countNot(f.img(), WHITE), 31);
        QVERIFY(f.e.document().isDirty());
        QCOMPARE(f.e.undoStack().count(), 1);             // one undo entry per stroke
        f.e.undo(); QCOMPARE(f.img(), before); QVERIFY(!f.e.document().isDirty());
        f.e.redo(); QCOMPARE(f.img().pixel(25, 10), BLACK); QVERIFY(f.e.document().isDirty());
    }
    void pencil_clickMakesDot_rightButtonUsesBackground() {
        Fx f; f.e.setTool(ToolId::Pencil); f.e.setSize(1);
        f.e.setForeground(Qt::red); f.e.setBackground(Qt::blue);
        f.e.pointerPress({5, 5}, Qt::LeftButton); f.e.pointerRelease({5, 5}, Qt::LeftButton);
        f.e.pointerPress({9, 9}, Qt::RightButton); f.e.pointerRelease({9, 9}, Qt::RightButton);
        QCOMPARE(f.img().pixel(5, 5), RED); QCOMPARE(f.img().pixel(9, 9), BLUE);
        QCOMPARE(f.e.undoStack().count(), 2);
    }
    void stroke_partiallyOutsideImageIsClipped() {
        Fx f(50, 50); f.e.setTool(ToolId::Pencil); f.e.setSize(3);
        const QImage before = f.img();
        f.drag({-30, 25}, {80, 25});
        QCOMPARE(f.img().pixel(0, 25), BLACK); QCOMPARE(f.img().pixel(49, 25), BLACK);
        QCOMPARE(f.img().size(), QSize(50, 50));
        f.e.undo(); QCOMPARE(f.img(), before);
    }
    void stroke_cancelRestoresDocument() {
        Fx f; f.e.setTool(ToolId::Brush); f.e.setSize(8);
        const QImage before = f.img();
        f.e.pointerPress({10, 10}, Qt::LeftButton);
        f.e.pointerMove({60, 40});
        QVERIFY(f.img() != before);                       // live drawing is visible
        f.e.cancelInteraction();
        QCOMPARE(f.img(), before);
        QCOMPARE(f.e.undoStack().count(), 0); QVERIFY(!f.e.document().isDirty());
        f.e.pointerRelease({60, 40}, Qt::LeftButton);      // stray release is harmless
        QCOMPARE(f.img(), before);
    }
    void stroke_secondButtonCancels() {
        Fx f; f.e.setTool(ToolId::Pencil);
        const QImage before = f.img();
        f.e.pointerPress({10, 10}, Qt::LeftButton);
        f.e.pointerMove({50, 10});
        f.e.pointerPress({50, 10}, Qt::RightButton);       // pressing the other button aborts
        QCOMPARE(f.img(), before); QVERIFY(!f.e.interacting());
    }
    void stroke_switchingToolMidStrokeCancels() {
        Fx f; f.e.setTool(ToolId::Pencil);
        const QImage before = f.img();
        f.e.pointerPress({10, 10}, Qt::LeftButton); f.e.pointerMove({30, 30});
        f.e.setTool(ToolId::Fill);
        QCOMPARE(f.img(), before);
    }
    void brush_allTypesDrawAndUndoExactly() {
        for (BrushType t : {BrushType::Round, BrushType::Calligraphy1, BrushType::Calligraphy2, BrushType::Airbrush, BrushType::Crayon, BrushType::Marker}) {
            Fx f; f.e.setTool(ToolId::Brush); f.e.setBrush(t); f.e.setSize(10); f.e.seedRandom(3);
            const QImage before = f.img();
            f.drag({10, 10}, {80, 60});
            QVERIFY2(countNot(f.img(), WHITE) > 20, qPrintable(QString("brush %1 drew nothing").arg(int(t))));
            f.e.undo(); QCOMPARE(f.img(), before);
        }
    }
    void brush_longStrokeUsesSmallUndo() {
        Fx f(3000, 3000); f.e.setTool(ToolId::Brush); f.e.setSize(6);
        f.e.pointerPress({100, 100}, Qt::LeftButton);
        for (int k = 1; k <= 30; ++k) f.e.pointerMove({100 + k * 5, 100 + k * 3});
        f.e.pointerRelease({250, 190}, Qt::LeftButton);
        QVERIFY2(f.e.undoStack().memoryUsage() < 1024 * 1024, "stroke undo is not tile-sized");   // 36 MB image
    }
    void eraser_paintsBackgroundOrTransparent() {
        Fx f; f.fillRect(QRect(0, 0, 100, 80), Qt::black);
        f.e.setBackground(Qt::blue); f.e.setTool(ToolId::Eraser); f.e.setSize(5);
        f.drag({20, 20}, {60, 20});
        QCOMPARE(f.img().pixel(40, 20), BLUE); QCOMPARE(f.img().pixel(40, 30), BLACK);
        f.e.setEraserTransparent(true);
        f.drag({20, 40}, {60, 40});
        QCOMPARE(qAlpha(f.img().pixel(40, 40)), 0);
        f.e.undo(); QCOMPARE(f.img().pixel(40, 40), BLACK);
        f.e.undo(); QCOMPARE(f.img().pixel(40, 20), BLACK);
    }
    // ================= fill / eyedropper =================
    void fill_regionUndoAndButtons() {
        Fx f; f.e.setForeground(Qt::red); f.e.setBackground(Qt::green); f.e.setTool(ToolId::Fill);
        { QPainter p(&f.e.document().mutableImage()); p.setPen(Qt::black); p.drawRect(10, 10, 40, 30); }
        const QImage before = f.img();
        f.e.pointerPress({20, 20}, Qt::LeftButton); f.e.pointerRelease({20, 20}, Qt::LeftButton);
        QCOMPARE(f.img().pixel(20, 20), RED); QCOMPARE(f.img().pixel(5, 5), WHITE); QCOMPARE(f.img().pixel(10, 10), BLACK);
        f.e.pointerPress({70, 60}, Qt::RightButton); f.e.pointerRelease({70, 60}, Qt::RightButton);
        QCOMPARE(f.img().pixel(70, 60), GREEN); QCOMPARE(f.img().pixel(20, 20), RED);
        f.e.undo(); f.e.undo(); QCOMPARE(f.img(), before);
    }
    void fill_noChangeLeavesHistoryAndDirtyAlone() {
        Fx f; f.e.setForeground(Qt::white); f.e.setTool(ToolId::Fill);
        f.e.pointerPress({5, 5}, Qt::LeftButton); f.e.pointerRelease({5, 5}, Qt::LeftButton);
        f.e.pointerPress({-4, 5}, Qt::LeftButton); f.e.pointerRelease({-4, 5}, Qt::LeftButton);   // outside
        QCOMPARE(f.e.undoStack().count(), 0); QVERIFY(!f.e.document().isDirty());
    }
    void eyedropper_setsColoursAndDoesNotEdit() {
        Fx f; f.fillRect(QRect(10, 10, 10, 10), Qt::red); f.fillRect(QRect(30, 10, 10, 10), Qt::blue);
        f.e.setTool(ToolId::Eyedropper);
        const QImage before = f.img();
        f.e.pointerPress({12, 12}, Qt::LeftButton); f.e.pointerRelease({12, 12}, Qt::LeftButton);
        QCOMPARE(f.e.settings().fg.rgba(), RED);
        f.e.pointerPress({32, 12}, Qt::RightButton); f.e.pointerMove({33, 13}); f.e.pointerRelease({33, 13}, Qt::RightButton);
        QCOMPARE(f.e.settings().bg.rgba(), BLUE);
        f.e.pointerPress({12, 12}, Qt::LeftButton); f.e.pointerMove({35, 12}); f.e.pointerRelease({35, 12}, Qt::LeftButton);   // drag updates
        QCOMPARE(f.e.settings().fg.rgba(), BLUE);
        QCOMPARE(f.img(), before); QCOMPARE(f.e.undoStack().count(), 0);
    }
    // ================= lines & shapes =================
    void line_previewIsSeparateThenCommits() {
        Fx f; f.e.setTool(ToolId::Line); f.e.setSize(1); f.e.setAntialias(false);
        const QImage before = f.img();
        f.e.pointerPress({10, 10}, Qt::LeftButton);
        f.e.pointerMove({60, 10});
        QCOMPARE(f.img(), before);                          // preview does NOT modify the document
        QCOMPARE(f.e.undoStack().count(), 0);
        f.e.pointerRelease({60, 10}, Qt::LeftButton);
        for (int x = 10; x <= 60; ++x) QCOMPARE(f.img().pixel(x, 10), BLACK);
        QCOMPARE(countNot(f.img(), WHITE), 51);
        QCOMPARE(f.e.undoStack().count(), 1);
        f.e.undo(); QCOMPARE(f.img(), before);
    }
    void line_shiftSnapsTo45Degrees() {
        Fx f; f.e.setTool(ToolId::Line); f.e.setSize(1); f.e.setAntialias(false);
        f.drag({10, 10}, {50, 14}, Qt::LeftButton, Qt::ShiftModifier);          // nearly horizontal -> horizontal
        QCOMPARE(f.img().pixel(50, 10), BLACK); QCOMPARE(f.img().pixel(30, 10), BLACK);
        QCOMPARE(f.img().pixel(50, 14), WHITE);
    }
    void shape_rectangleModesAndColours() {
        Fx f; f.e.setTool(ToolId::Shape); f.e.setShape(ShapeType::Rectangle); f.e.setSize(1); f.e.setAntialias(false);
        f.e.setForeground(Qt::black); f.e.setBackground(Qt::red);
        f.e.setFillMode(FillMode::OutlineAndFill);
        f.drag({10, 10}, {29, 19});
        QCOMPARE(f.img().pixel(10, 10), BLACK); QCOMPARE(f.img().pixel(20, 15), RED);
        f.drag({40, 10}, {59, 19}, Qt::RightButton);        // right button swaps roles
        QCOMPARE(f.img().pixel(40, 10), RED); QCOMPARE(f.img().pixel(50, 15), BLACK);
        f.e.setFillMode(FillMode::Fill);
        f.drag({10, 40}, {29, 49});
        QCOMPARE(f.img().pixel(10, 40), BLACK); QCOMPARE(f.img().pixel(29, 49), BLACK);
        QCOMPARE(f.e.undoStack().count(), 3);
    }
    void shape_previewSeparateCancelAndShiftSquare() {
        Fx f; f.e.setTool(ToolId::Shape); f.e.setFillMode(FillMode::Fill); f.e.setAntialias(false);
        const QImage before = f.img();
        f.e.pointerPress({10, 10}, Qt::LeftButton, Qt::ShiftModifier);
        f.e.pointerMove({40, 25}, Qt::ShiftModifier);
        QCOMPARE(f.img(), before);
        f.e.cancelInteraction();
        QCOMPARE(f.img(), before); QCOMPARE(f.e.undoStack().count(), 0);
        f.drag({10, 10}, {40, 25}, Qt::LeftButton, Qt::ShiftModifier);         // 30x15 drag -> 31x31 square
        QCOMPARE(f.img().pixel(40, 40), BLACK); QCOMPARE(f.img().pixel(41, 40), WHITE); QCOMPARE(f.img().pixel(40, 41), WHITE);
    }
    void shape_everyTypeDrawsAndUndoes() {
        for (int t = 0; t <= int(ShapeType::Star); ++t) {
            Fx f; f.e.setTool(ToolId::Shape); f.e.setShape(ShapeType(t)); f.e.setSize(2);
            const QImage before = f.img();
            f.drag({20, 10}, {80, 60});
            QVERIFY2(countNot(f.img(), WHITE) > 50, qPrintable(QString("shape %1").arg(t)));
            f.e.undo(); QCOMPARE(f.img(), before);
        }
    }
    // ================= selection =================
    void select_rectMoveCommitIsOneUndo() {
        Fx f; f.fillRect(QRect(10, 10, 20, 10), Qt::red);
        const QImage before = f.img();
        f.e.setTool(ToolId::SelectRect);
        f.drag({10, 10}, {29, 19});
        QVERIFY(f.e.hasSelection()); QCOMPARE(f.e.selection().bounds, QRect(10, 10, 20, 10));
        QCOMPARE(f.e.undoStack().count(), 0);
        f.drag({15, 15}, {25, 20});                         // move by (10,5)
        QCOMPARE(f.img(), before);                          // moving is a preview: document untouched
        QCOMPARE(f.e.selection().pos, QPoint(20, 15));
        QVERIFY(f.e.selection().pending());
        f.e.commitSelection();
        QVERIFY(!f.e.hasSelection());
        QCOMPARE(f.img().pixel(12, 12), WHITE);             // source cleared to background
        QCOMPARE(f.img().pixel(25, 17), RED); QCOMPARE(f.img().pixel(35, 20), RED); QCOMPARE(f.img().pixel(35, 12), WHITE);
        QCOMPARE(f.e.undoStack().count(), 1);
        f.e.undo(); QCOMPARE(f.img(), before);
        f.e.redo(); QCOMPARE(f.img().pixel(35, 20), RED);
    }
    void select_clickOutsideCommits_clickWithoutDragClears() {
        Fx f; f.fillRect(QRect(10, 10, 20, 10), Qt::red);
        f.e.setTool(ToolId::SelectRect);
        f.drag({10, 10}, {29, 19}); f.drag({15, 15}, {45, 15});
        QVERIFY(f.e.selection().pending());
        f.e.pointerPress({90, 70}, Qt::LeftButton); f.e.pointerRelease({90, 70}, Qt::LeftButton);   // click outside
        QVERIFY(!f.e.hasSelection()); QCOMPARE(f.img().pixel(50, 17), RED); QCOMPARE(f.e.undoStack().count(), 1);
    }
    void select_selectionWithoutMoveCreatesNoHistory() {
        Fx f; f.e.setTool(ToolId::SelectRect);
        f.drag({10, 10}, {29, 19});
        f.e.commitSelection();
        QCOMPARE(f.e.undoStack().count(), 0); QVERIFY(!f.e.document().isDirty());
    }
    void select_ctrlDragClones() {
        Fx f; f.fillRect(QRect(10, 10, 20, 10), Qt::red);
        f.e.setTool(ToolId::SelectRect);
        f.drag({10, 10}, {29, 19});
        f.drag({15, 15}, {15, 45}, Qt::LeftButton, Qt::ControlModifier);       // copy 30px down
        f.e.commitSelection();
        QCOMPARE(f.img().pixel(15, 15), RED);               // original still there
        QCOMPARE(f.img().pixel(15, 45), RED);               // clone
    }
    void select_cancelDragRestoresPosition() {
        Fx f; f.e.setTool(ToolId::SelectRect);
        f.drag({10, 10}, {29, 19});
        f.e.pointerPress({15, 15}, Qt::LeftButton); f.e.pointerMove({50, 50});
        QCOMPARE(f.e.selection().pos, QPoint(45, 45));
        f.e.cancelInteraction();
        QCOMPARE(f.e.selection().pos, QPoint(10, 10)); QVERIFY(f.e.hasSelection()); QVERIFY(!f.e.selection().pending());
    }
    void select_undoDiscardsPendingMove() {
        Fx f; f.fillRect(QRect(10, 10, 20, 10), Qt::red);
        const QImage before = f.img();
        f.e.setTool(ToolId::SelectRect);
        f.drag({10, 10}, {29, 19}); f.drag({15, 15}, {45, 45});
        f.e.undo();
        QVERIFY(!f.e.hasSelection()); QCOMPARE(f.img(), before); QCOMPARE(f.e.undoStack().count(), 0);
    }
    void select_otherToolCommitsFirst() {
        Fx f; f.fillRect(QRect(10, 10, 20, 10), Qt::red);
        f.e.setTool(ToolId::SelectRect);
        f.drag({10, 10}, {29, 19}); f.drag({15, 15}, {45, 45});
        f.e.setTool(ToolId::Pencil); f.e.setSize(1);
        f.e.pointerPress({90, 5}, Qt::LeftButton); f.e.pointerRelease({90, 5}, Qt::LeftButton);
        QVERIFY(!f.e.hasSelection());
        QCOMPARE(f.img().pixel(12, 12), WHITE); QCOMPARE(f.img().pixel(45, 45), RED); QCOMPARE(f.img().pixel(90, 5), BLACK);
        QCOMPARE(f.e.undoStack().count(), 2);               // selection commit + pencil dot, separately undoable
    }
    void select_freeformMovesOnlyInsidePolygon() {
        Fx f; f.fillRect(QRect(0, 0, 70, 70), Qt::blue); f.fillRect(QRect(12, 12, 1, 1), Qt::red);
        const QImage before = f.img();
        f.e.setTool(ToolId::SelectFree);
        f.e.pointerPress({10, 10}, Qt::LeftButton); f.e.pointerMove({40, 10}); f.e.pointerMove({10, 40}); f.e.pointerRelease({10, 40}, Qt::LeftButton);
        QVERIFY(f.e.hasSelection());
        QVERIFY(f.e.selectionContains({15, 15})); QVERIFY(!f.e.selectionContains({38, 38}));
        f.drag({15, 15}, {35, 35});                         // move by (20,20)
        f.e.commitSelection();
        QCOMPARE(f.img().pixel(12, 12), WHITE);             // inside source triangle -> background
        QCOMPARE(f.img().pixel(32, 32), RED);               // content moved
        QCOMPARE(f.img().pixel(55, 55), BLUE);              // inside dest bbox but outside triangle: untouched
        QCOMPARE(f.img().pixel(38, 38), BLUE);              // outside source triangle: untouched
        f.e.undo(); QCOMPARE(f.img(), before);
    }
    void select_deleteFillsBackground() {
        Fx f; f.fillRect(QRect(0, 0, 100, 80), Qt::red); f.e.setBackground(Qt::blue);
        const QImage before = f.img();
        QVERIFY(f.e.selectRect(QRect(10, 10, 20, 20)));
        QVERIFY(f.e.deleteSelection());
        QCOMPARE(f.img().pixel(15, 15), BLUE); QCOMPARE(f.img().pixel(9, 15), RED); QCOMPARE(f.img().pixel(30, 15), RED);
        QVERIFY(!f.e.hasSelection());
        f.e.undo(); QCOMPARE(f.img(), before);
        QVERIFY(!f.e.deleteSelection());                     // nothing selected
    }
    void select_copyCutPasteRoundTrip() {
        Fx f; f.fillRect(QRect(10, 10, 20, 20), Qt::red); f.fillRect(QRect(12, 12, 1, 1), Qt::green);
        f.e.selectRect(QRect(10, 10, 20, 20));
        QImage cut = f.e.selectionImage();
        QCOMPARE(cut.size(), QSize(20, 20)); QCOMPARE(cut.pixel(2, 2), GREEN);
        QVERIFY(f.e.cut());
        QCOMPARE(f.img().pixel(15, 15), WHITE);
        QVERIFY(f.e.pasteImage(cut, QPoint(50, 40)));
        QVERIFY(f.e.selection().pending());
        QCOMPARE(f.img().pixel(55, 45), WHITE);              // paste is a preview until committed
        f.e.commitSelection();
        QCOMPARE(f.img().pixel(55, 45), RED); QCOMPARE(f.img().pixel(52, 42), GREEN);
        QCOMPARE(f.e.undoStack().count(), 2);                // cut, paste
        f.e.undo(); QCOMPARE(f.img().pixel(55, 45), WHITE);
        f.e.undo(); QCOMPARE(f.img().pixel(15, 15), RED);
    }
    void select_clipboardApi() {
        Fx f; f.fillRect(QRect(5, 5, 10, 10), Qt::red);
        f.e.selectRect(QRect(5, 5, 10, 10));
        QVERIFY(f.e.copy());
        QVERIFY(f.e.paste());
        QCOMPARE(f.e.selection().floating.size(), QSize(10, 10));
        QVERIFY(!Editor().copy());                           // no selection -> false
    }
    void select_pasteAlphaCompositesAndTransparentIsKept() {
        Fx f; f.fillRect(QRect(0, 0, 100, 80), Qt::blue);
        QImage t(4, 4, QImage::Format_ARGB32); t.fill(Qt::transparent); t.setPixel(1, 1, RED);
        f.e.pasteImage(t, QPoint(10, 10)); f.e.commitSelection();
        QCOMPARE(f.img().pixel(11, 11), RED); QCOMPARE(f.img().pixel(10, 10), BLUE);   // transparent part shows through
    }
    void select_pasteLargerEnlargesCanvas() {
        Fx f(20, 20); f.e.setBackground(Qt::blue);
        QImage big(30, 25, QImage::Format_ARGB32); big.fill(Qt::red);
        QVERIFY(f.e.pasteImage(big));
        QCOMPARE(f.img().size(), QSize(30, 25));
        QCOMPARE(f.img().pixel(25, 22), BLUE);               // new area uses background colour
        f.e.commitSelection();
        QCOMPARE(f.img().pixel(25, 22), RED);
        f.e.undo(); f.e.undo(); QCOMPARE(f.img().size(), QSize(20, 20));
    }
    void select_selectionClampedToImage() {
        Fx f;
        QVERIFY(f.e.selectRect(QRect(-20, -20, 50, 50)));
        QCOMPARE(f.e.selection().bounds, QRect(0, 0, 30, 30));
        QVERIFY(!f.e.selectRect(QRect(500, 500, 5, 5)));
        f.e.selectAll(); QCOMPARE(f.e.selection().bounds, f.img().rect());
    }
    // ================= text =================
    void text_placeMoveCommitUndo() {
        Fx f; f.e.setForeground(Qt::red); f.e.setAntialias(false);
        const QImage before = f.img();
        QVERIFY(f.e.placeText("Hello", QFont("Noto Sans", 16), QPoint(10, 10)));
        QCOMPARE(f.img(), before);                           // text is a movable preview first
        f.e.moveSelectionBy(QPoint(5, 5));
        f.e.commitSelection();
        QVERIFY(countNot(f.img(), WHITE) > 30);
        int red = 0, other = 0; for (int y = 0; y < 80; ++y) for (int x = 0; x < 100; ++x) { QRgb p = f.img().pixel(x, y); if (p == RED) ++red; else if (p != WHITE) ++other; }
        QVERIFY(red > 30); QCOMPARE(other, 0);
        for (int y = 0; y < 15; ++y) for (int x = 0; x < 100; ++x) QCOMPARE(f.img().pixel(x, y), WHITE);   // placed at y>=15
        f.e.undo(); QCOMPARE(f.img(), before);
        QVERIFY(!f.e.placeText("", QFont(), QPoint(0, 0)));
    }
    void text_toolEmitsRequest() {
        Fx f; f.e.setTool(ToolId::Text);
        QSignalSpy spy(&f.e, &Editor::textRequested);
        f.e.pointerPress({33, 44}, Qt::LeftButton); f.e.pointerRelease({33, 44}, Qt::LeftButton);
        QCOMPARE(spy.count(), 1); QCOMPARE(spy.at(0).at(0).toPoint(), QPoint(33, 44));
    }
    // ================= image operations =================
    void ops_cropToSelection() {
        Fx f; f.fillRect(QRect(20, 20, 10, 10), Qt::red);
        const QImage before = f.img();
        QVERIFY(!f.e.cropToSelection());                     // no selection
        f.e.selectRect(QRect(20, 20, 10, 10));
        QVERIFY(f.e.cropToSelection());
        QCOMPARE(f.img().size(), QSize(10, 10)); QCOMPARE(countNot(f.img(), RED), 0);
        QVERIFY(!f.e.hasSelection());
        f.e.undo(); QCOMPARE(f.img(), before);
        f.e.redo(); QCOMPARE(f.img().size(), QSize(10, 10));
    }
    void ops_cropAppliesMovedSelectionFirst() {
        Fx f; f.fillRect(QRect(10, 10, 10, 10), Qt::red);
        f.e.setTool(ToolId::SelectRect);
        f.drag({10, 10}, {19, 19}); f.drag({12, 12}, {42, 42});   // moved by (30,30)
        QVERIFY(f.e.cropToSelection());
        QCOMPARE(f.img().size(), QSize(10, 10)); QCOMPARE(countNot(f.img(), RED), 0);
        QCOMPARE(f.e.undoStack().count(), 2);               // selection move + crop
    }
    void ops_resizeAndCanvasResize() {
        Fx f(40, 20); f.e.setBackground(Qt::blue);
        f.fillRect(QRect(0, 0, 40, 20), Qt::red);
        const QImage before = f.img();
        QVERIFY(f.e.resizeImage(QSize(80, 40), false));
        QCOMPARE(f.img().size(), QSize(80, 40)); QCOMPARE(f.img().pixel(70, 30), RED);
        f.e.undo(); QCOMPARE(f.img(), before);
        QVERIFY(f.e.resizeCanvas(QSize(60, 30)));
        QCOMPARE(f.img().size(), QSize(60, 30)); QCOMPARE(f.img().pixel(10, 10), RED); QCOMPARE(f.img().pixel(50, 25), BLUE);
        f.e.undo(); QCOMPARE(f.img(), before);
        QVERIFY(!f.e.resizeImage(QSize(0, 10))); QVERIFY(!f.e.resizeCanvas(QSize(100000, 10)));
        QCOMPARE(f.e.undoStack().count(), 1);               // new edit after undo dropped the old redo branch; invalid ops add nothing
        QVERIFY(f.e.canRedo());
    }
    void ops_rotateFlipUndoRedoDirty() {
        Fx f(30, 10); f.fillRect(QRect(0, 0, 5, 5), Qt::red);
        const QImage before = f.img();
        f.e.rotate(ImageOps::Rotation::Cw90);
        QCOMPARE(f.img().size(), QSize(10, 30)); QCOMPARE(f.img().pixel(9, 0), RED);
        f.e.flip(true);  QCOMPARE(f.img().pixel(0, 0), RED);
        f.e.flip(false); QCOMPARE(f.img().pixel(0, 29), RED);
        QCOMPARE(f.e.undoStack().count(), 3);
        f.e.undo(); f.e.undo(); f.e.undo();
        QCOMPARE(f.img(), before); QVERIFY(!f.e.document().isDirty());
        f.e.redo(); f.e.redo(); f.e.redo();
        QCOMPARE(f.img().pixel(0, 29), RED); QVERIFY(f.e.document().isDirty());
        f.e.undo(); f.e.undo(); f.e.undo();
        f.e.rotate(ImageOps::Rotation::Rotate180); QCOMPARE(f.img().pixel(29, 9), RED);
        f.e.rotate(ImageOps::Rotation::Ccw90);     QCOMPARE(f.img().size(), QSize(10, 30));
    }
    void ops_commitPendingSelectionBeforeTransform() {
        Fx f; f.fillRect(QRect(0, 0, 10, 10), Qt::red);
        f.e.setTool(ToolId::SelectRect);
        f.drag({0, 0}, {9, 9}); f.drag({5, 5}, {45, 45});
        f.e.flip(true);
        QVERIFY(!f.e.hasSelection());
        QCOMPARE(f.img().pixel(99 - 45, 45), RED);          // moved content was applied, then mirrored
    }
    // ================= lifecycle / dirty =================
    void lifecycle_newLoadSaveDirty() {
        Fx f; f.e.setTool(ToolId::Pencil);
        f.drag({1, 1}, {9, 9});
        QVERIFY(f.e.document().isDirty());
        f.e.markSaved("/tmp/x.png");
        QVERIFY(!f.e.document().isDirty()); QCOMPARE(f.e.document().filePath(), QString("/tmp/x.png"));
        f.drag({20, 20}, {30, 30}); QVERIFY(f.e.document().isDirty());
        f.e.undo(); QVERIFY(!f.e.document().isDirty());
        QImage other(33, 22, QImage::Format_ARGB32); other.fill(Qt::green);
        QVERIFY(f.e.loadImage(other, "/tmp/o.png"));
        QCOMPARE(f.img().size(), QSize(33, 22)); QVERIFY(!f.e.document().isDirty());
        QCOMPARE(f.e.undoStack().count(), 0); QVERIFY(!f.e.canUndo());
        QVERIFY(!f.e.loadImage(QImage(), "/x"));
        f.e.newDocument(QSize(7, 5), Qt::blue);
        QCOMPARE(f.img().size(), QSize(7, 5)); QCOMPARE(f.img().pixel(0, 0), BLUE); QVERIFY(f.e.document().filePath().isEmpty());
    }
    void lifecycle_newDocumentDropsSelectionAndInteraction() {
        Fx f; f.e.setTool(ToolId::SelectRect); f.drag({1, 1}, {20, 20});
        f.e.pointerPress({5, 5}, Qt::LeftButton);
        f.e.newDocument(QSize(10, 10));
        QVERIFY(!f.e.hasSelection()); QVERIFY(!f.e.interacting());
    }
    void settings_clampAndSignals() {
        Fx f; QSignalSpy spy(&f.e, &Editor::settingsChanged);
        f.e.setSize(0); QCOMPARE(f.e.settings().size, 1);
        f.e.setSize(100000); QCOMPARE(f.e.settings().size, 500);
        f.e.setForeground(Qt::red); f.e.setBackground(Qt::blue); f.e.swapColors();
        QCOMPARE(f.e.settings().fg, QColor(Qt::blue)); QCOMPARE(f.e.settings().bg, QColor(Qt::red));
        QVERIFY(spy.count() >= 4);
    }
    void coordinates_toolsUseImageSpaceOnly() {
        // Tools receive image coordinates; the document size is never affected by anything but ops.
        Fx f(64, 48); f.e.setTool(ToolId::Pencil); f.e.setSize(1);
        f.drag({0, 0}, {63, 47});
        QCOMPARE(f.img().size(), QSize(64, 48)); QCOMPARE(f.img().pixel(63, 47), BLACK); QCOMPARE(f.img().pixel(0, 0), BLACK);
    }
};
QTEST_MAIN(TestEditor)
#include "test_editor.moc"
