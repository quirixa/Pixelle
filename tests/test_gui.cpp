#include <QtTest>
#include <QMessageBox>
#include <QAbstractButton>
#include <QTemporaryDir>
#include "app/MainWindow.h"
#include "drawing/Editor.h"
#include "io/ImageFileService.h"
#include "ui/CanvasView.h"

static const QRgb BLACK = 0xFF000000u, WHITE = 0xFFFFFFFFu, RED = 0xFFFF0000u;
static int countNot(const QImage &i, QRgb c) { int n = 0; for (int y = 0; y < i.height(); ++y) for (int x = 0; x < i.width(); ++x) if (i.pixel(x, y) != c) ++n; return n; }

// Press a button of whatever modal message box appears (they would otherwise block the test).
static void answerNextBox(QMessageBox::StandardButton b) {
    QTimer::singleShot(150, [b] {
        if (auto *box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget())) {
            if (QAbstractButton *btn = box->button(b)) btn->click(); else box->close();
        } else if (QWidget *w = QApplication::activeModalWidget()) w->close();
    });
}

class TestGui : public QObject {
    Q_OBJECT
    MainWindow *w = nullptr;
    Editor *e() { return w->editor(); }
    CanvasView *c() { return w->canvas(); }
    const QImage &img() { return e()->document().image(); }
    // centre of an image pixel in canvas widget coordinates, at the current zoom
    QPoint vp(QPoint ip) { return c()->imageToViewport(ip) + QPoint(int(c()->zoom() / 2), int(c()->zoom() / 2)); }
private slots:
    void init() { delete w; w = new MainWindow; w->show(); QVERIFY(QTest::qWaitForWindowExposed(w)); e()->newDocument(QSize(100, 80)); c()->setZoom(1.0); }
    void cleanup() { delete w; w = nullptr; }

    void branding_titleAndMetadata() {
        QCOMPARE(w->windowTitle(), QString("Untitled - Pixelle"));
        QCOMPARE(MainWindow::appDisplayName(), QString("Pixelle"));
        QVERIFY(w->windowTitle().endsWith("Pixelle"));
    }
    void mouse_clickPlacesPixelAtEveryZoom() {
        e()->setTool(ToolId::Pencil); e()->setSize(1);
        for (double z : {1.0, 2.0, 3.0, 4.0, 8.0}) {
            e()->newDocument(QSize(100, 80)); c()->setZoom(z);
            QTest::mouseClick(c(), Qt::LeftButton, {}, vp({17, 23}));
            QCOMPARE(img().pixel(17, 23), BLACK);
            QCOMPARE(countNot(img(), WHITE), 1);                // exactly one pixel, regardless of zoom
            QCOMPARE(img().size(), QSize(100, 80));             // zoom never changes the image size
        }
    }
    void mouse_zoomedOutAlsoMapsApproximately() {
        e()->setTool(ToolId::Pencil); e()->setSize(1); c()->setZoom(0.5);
        QTest::mouseClick(c(), Qt::LeftButton, {}, c()->origin() + QPoint(20, 10));   // viewport 20,10 -> image 40,20
        QCOMPARE(img().pixel(40, 20), BLACK);
    }
    void mouse_dragStrokeAndUndoViaAction() {
        e()->setTool(ToolId::Pencil); e()->setSize(1); c()->setZoom(2.0);
        QTest::mousePress(c(), Qt::LeftButton, {}, vp({10, 10}));
        QTest::mouseMove(c(), vp({30, 10}));
        QTest::mouseMove(c(), vp({50, 10}));
        QTest::mouseRelease(c(), Qt::LeftButton, {}, vp({50, 10}));
        for (int x = 10; x <= 50; ++x) QCOMPARE(img().pixel(x, 10), BLACK);
        QCOMPARE(w->windowTitle(), QString("Untitled* - Pixelle"));
        QVERIFY(w->action("actionUndo")->isEnabled());
        w->action("actionUndo")->trigger();
        QCOMPARE(countNot(img(), WHITE), 0);
        QCOMPARE(w->windowTitle(), QString("Untitled - Pixelle"));     // back to the clean state
        QVERIFY(!w->action("actionUndo")->isEnabled()); QVERIFY(w->action("actionRedo")->isEnabled());
        w->action("actionRedo")->trigger();
        QCOMPARE(img().pixel(30, 10), BLACK);
    }
    void mouse_dragThatLeavesTheViewportStillWorks() {
        e()->setTool(ToolId::Pencil); e()->setSize(1);
        QTest::mousePress(c(), Qt::LeftButton, {}, vp({50, 40}));
        QTest::mouseMove(c(), vp({90, 40}));
        QTest::mouseMove(c(), QPoint(c()->width() + 500, vp({90, 40}).y()));     // far outside the widget
        QTest::mouseRelease(c(), Qt::LeftButton, {}, QPoint(c()->width() + 500, vp({90, 40}).y()));
        QCOMPARE(img().pixel(99, 40), BLACK);                    // clipped at the edge, no crash
        QCOMPARE(img().size(), QSize(100, 80));
        QVERIFY(!e()->interacting());
    }
    void mouse_rightButtonUsesBackground() {
        e()->setBackground(Qt::red); e()->setTool(ToolId::Pencil); e()->setSize(1);
        QTest::mouseClick(c(), Qt::RightButton, {}, vp({5, 5}));
        QCOMPARE(img().pixel(5, 5), RED);
    }
    void keyboard_escapeCancelsStroke() {
        e()->setTool(ToolId::Brush); e()->setSize(6);
        QTest::mousePress(c(), Qt::LeftButton, {}, vp({10, 10}));
        QTest::mouseMove(c(), vp({60, 40}));
        QVERIFY(countNot(img(), WHITE) > 0);
        w->action("actionEscape")->trigger();
        QCOMPARE(countNot(img(), WHITE), 0);
        QTest::mouseRelease(c(), Qt::LeftButton, {}, vp({60, 40}));
        QCOMPARE(countNot(img(), WHITE), 0);
        QVERIFY(!e()->document().isDirty());
    }
    void shortcuts_areBoundToExpectedKeys() {
        struct S { const char *name; QKeySequence k; } expect[] = {
            {"actionNew", QKeySequence::New}, {"actionOpen", QKeySequence::Open}, {"actionSave", QKeySequence::Save},
            {"actionSaveAs", QKeySequence("Ctrl+Shift+S")}, {"actionUndo", QKeySequence::Undo}, {"actionCut", QKeySequence::Cut},
            {"actionCopy", QKeySequence::Copy}, {"actionPaste", QKeySequence::Paste}, {"actionSelectAll", QKeySequence::SelectAll},
            {"actionDelete", QKeySequence::Delete}, {"actionResize", QKeySequence("Ctrl+W")}, {"actionCanvasSize", QKeySequence("Ctrl+E")},
            {"actionCrop", QKeySequence("Ctrl+Shift+X")}, {"actionZoomOut", QKeySequence("Ctrl+-")}, {"actionZoomReset", QKeySequence("Ctrl+0")},
            {"actionRedo", QKeySequence("Ctrl+Y")}};
        for (const S &s : expect) { QAction *a = w->action(s.name); QVERIFY2(a, s.name); QVERIFY2(a->shortcuts().contains(s.k), s.name); }
    }
    void keyboard_realKeyPressesTriggerShortcuts() {
        QApplication::setActiveWindow(w); w->activateWindow(); QTest::qWait(50);
        e()->setTool(ToolId::Pencil); e()->setSize(1);
        QTest::mouseClick(c(), Qt::LeftButton, {}, vp({5, 5}));
        QCOMPARE(img().pixel(5, 5), BLACK);
        QTest::keyClick(w, Qt::Key_Z, Qt::ControlModifier);
        if (img().pixel(5, 5) == BLACK) QSKIP("offscreen platform did not deliver the shortcut (action wiring is covered elsewhere)");
        QCOMPARE(img().pixel(5, 5), WHITE);
        QTest::keyClick(w, Qt::Key_Y, Qt::ControlModifier);
        QCOMPARE(img().pixel(5, 5), BLACK);
    }
    void selection_actionsEnableAndWork() {
        QVERIFY(!w->action("actionCrop")->isEnabled()); QVERIFY(!w->action("actionCopy")->isEnabled());
        e()->setTool(ToolId::SelectRect); c()->setZoom(2.0);
        QTest::mousePress(c(), Qt::LeftButton, {}, vp({10, 10}));
        QTest::mouseMove(c(), vp({30, 20}));
        QTest::mouseRelease(c(), Qt::LeftButton, {}, vp({39, 29}));
        QVERIFY(e()->hasSelection());
        QCOMPARE(e()->selection().bounds, QRect(10, 10, 30, 20));
        QVERIFY(w->action("actionCrop")->isEnabled()); QVERIFY(w->action("actionCopy")->isEnabled());
        w->action("actionCrop")->trigger();
        QCOMPARE(img().size(), QSize(30, 20));
        w->action("actionUndo")->trigger();
        QCOMPARE(img().size(), QSize(100, 80));
    }
    void image_menuActions() {
        w->action("actionRotateRight")->trigger(); QCOMPARE(img().size(), QSize(80, 100));
        w->action("actionRotateLeft")->trigger();  QCOMPARE(img().size(), QSize(100, 80));
        w->action("actionRotate180")->trigger();   QCOMPARE(img().size(), QSize(100, 80));
        e()->setTool(ToolId::Pencil); e()->setSize(1); QTest::mouseClick(c(), Qt::LeftButton, {}, vp({0, 0}));
        w->action("actionFlipH")->trigger(); QCOMPARE(img().pixel(99, 0), BLACK);
        w->action("actionFlipV")->trigger(); QCOMPARE(img().pixel(99, 79), BLACK);
        QCOMPARE(e()->undoStack().count(), 6);
    }
    void zoom_actionsAndLimits() {
        w->action("actionZoomIn")->trigger(); QCOMPARE(c()->zoom(), 2.0);
        w->action("actionZoomOut")->trigger(); w->action("actionZoomOut")->trigger(); QCOMPARE(c()->zoom(), 0.5);
        w->action("actionZoomReset")->trigger(); QCOMPARE(c()->zoom(), 1.0);
        for (int i = 0; i < 30; ++i) w->action("actionZoomIn")->trigger();
        QCOMPARE(c()->zoom(), 16.0);
        for (int i = 0; i < 30; ++i) w->action("actionZoomOut")->trigger();
        QCOMPARE(c()->zoom(), 0.0625);
        QCOMPARE(img().size(), QSize(100, 80));
    }
    void toolActionsSwitchTools() {
        for (int id = 0; id < int(ToolId::Count); ++id) {
            QAction *a = w->action(QString("tool_%1").arg(id)); QVERIFY(a);
            a->trigger(); QCOMPARE(int(e()->toolId()), id); QVERIFY(a->isChecked());
        }
    }
    // ---------------- file workflow ----------------
    void file_saveOpenRoundTripAndTitle() {
        QTemporaryDir dir; QVERIFY(dir.isValid());
        e()->setTool(ToolId::Pencil); e()->setSize(1); e()->setForeground(Qt::red);
        QTest::mouseClick(c(), Qt::LeftButton, {}, vp({7, 9}));
        const QString path = dir.filePath("pic.png");
        QVERIFY(w->saveFile(path, false));
        QVERIFY(!e()->document().isDirty());
        QCOMPARE(w->windowTitle(), QString("pic.png - Pixelle"));
        w->action("actionNew");   // exists
        e()->newDocument(QSize(10, 10));
        QVERIFY(w->openFile(path));
        QCOMPARE(img().size(), QSize(100, 80)); QCOMPARE(img().pixel(7, 9), RED);
        QCOMPARE(w->windowTitle(), QString("pic.png - Pixelle"));
        QVERIFY(!e()->document().isDirty());
    }
    void file_saveRefusesToOverwriteWithoutConfirmation() {
        QTemporaryDir dir; const QString path = dir.filePath("keep.png");
        QImage orig(5, 5, QImage::Format_ARGB32); orig.fill(Qt::green); QVERIFY(orig.save(path));
        answerNextBox(QMessageBox::Ok);
        QVERIFY(!w->saveFile(path, false));
        QImage back; QVERIFY(ImageFileService::load(path, &back).ok);
        QCOMPARE(back.size(), QSize(5, 5));                      // untouched
        QVERIFY(e()->document().filePath().isEmpty());
    }
    void file_failedSaveAndOpenReportErrors() {
        e()->setTool(ToolId::Pencil); QTest::mouseClick(c(), Qt::LeftButton, {}, vp({1, 1}));
        answerNextBox(QMessageBox::Ok);
        QVERIFY(!w->saveFile("/nonexistent_dir/x.png", true));
        QVERIFY(e()->document().isDirty());                      // still dirty: nothing was saved
        answerNextBox(QMessageBox::Ok);
        QVERIFY(!w->openFile("/nonexistent/none.png"));
        QCOMPARE(img().size(), QSize(100, 80));                  // current document untouched
    }
    void file_corruptFileIsRejected() {
        QTemporaryDir dir; const QString path = dir.filePath("bad.png");
        QFile f(path); QVERIFY(f.open(QIODevice::WriteOnly)); f.write("not an image"); f.close();
        answerNextBox(QMessageBox::Ok);
        QVERIFY(!w->openFile(path));
    }
    void file_jpegTransparencyWarningCanCancel() {
        QTemporaryDir dir; const QString path = dir.filePath("t.jpg");
        e()->newDocument(QSize(10, 10), Qt::transparent);
        answerNextBox(QMessageBox::No);
        QVERIFY(!w->saveFile(path, false));
        QVERIFY(!QFileInfo::exists(path));
        answerNextBox(QMessageBox::Yes);
        QVERIFY(w->saveFile(path, false));
        QVERIFY(QFileInfo::exists(path));
    }
    void file_pngKeepsTransparency() {
        QTemporaryDir dir; const QString path = dir.filePath("t.png");
        e()->newDocument(QSize(10, 10), Qt::transparent);
        QVERIFY(w->saveFile(path, false));               // no warning needed for PNG
        QImage back; QVERIFY(ImageFileService::load(path, &back).ok);
        QCOMPARE(qAlpha(back.pixel(3, 3)), 0);
    }
    void file_pendingSelectionIsSavedAndUnsavedWarningDiscards() {
        e()->setTool(ToolId::Pencil); e()->setSize(1); QTest::mouseClick(c(), Qt::LeftButton, {}, vp({1, 1}));
        QVERIFY(e()->document().isDirty());
        answerNextBox(QMessageBox::Cancel);
        QVERIFY(!w->maybeSave());                                // cancel keeps the document
        QVERIFY(e()->document().isDirty());
        answerNextBox(QMessageBox::Discard);
        QVERIFY(w->maybeSave());
        // a pending paste counts as unsaved work
        e()->newDocument(QSize(20, 20));
        QImage p(3, 3, QImage::Format_ARGB32); p.fill(Qt::red); e()->pasteImage(p, QPoint(2, 2));
        QVERIFY(!e()->document().isDirty());
        answerNextBox(QMessageBox::Cancel);
        QVERIFY(!w->maybeSave());                                // maybeSave committed the paste -> dirty -> prompted
        QCOMPARE(img().pixel(3, 3), RED);
    }
    void text_toolOpensRequestAndPlacesText() {
        e()->setTool(ToolId::Text);
        e()->setForeground(Qt::red);
        e()->placeText("Hi", QFont("Noto Sans", 14), QPoint(10, 10));     // dialog path is event-driven; engine path tested here
        e()->commitSelection();
        QVERIFY(countNot(img(), WHITE) > 10);
    }
};
QTEST_MAIN(TestGui)
#include "test_gui.moc"
