#include "MainWindow.h"
#include "drawing/Editor.h"
#include "io/ImageFileService.h"
#include "ui/CanvasView.h"
#include "ui/ColorPalette.h"
#include "ui/Dialogs.h"
#include <QActionGroup>
#include <QApplication>
#include <QCheckBox>
#include <QCloseEvent>
#include <QComboBox>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QLabel>
#include <QMenuBar>
#include <QMessageBox>
#include <QScrollArea>
#include <QSpinBox>
#include <QStatusBar>
#include <QTimer>
#include <QToolBar>

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) {
    m_editor = new Editor(this);
    m_canvas = new CanvasView(&m_editor->document());
    m_canvas->setEditor(m_editor);
    m_scroll = new QScrollArea;
    m_scroll->setWidget(m_canvas);
    m_scroll->setFrameShape(QFrame::NoFrame);
    m_scroll->setWidgetResizable(true);   // canvas fills viewport; workspace colour is painted by CanvasView
    setCentralWidget(m_scroll);
    m_palette = new ColorPalette(m_editor);

    buildMenus();
    buildToolBars();
    buildStatusBar();

    connect(&m_editor->document(), &DrawingDocument::dirtyChanged, this, [this] { updateTitle(); });
    connect(&m_editor->document(), &DrawingDocument::imageReplaced, this, [this] { updateTitle(); updateActions(); });
    connect(m_editor, &Editor::historyChanged, this, [this] { updateActions(); updateTitle(); });
    connect(m_editor, &Editor::selectionChanged, this, [this] { updateActions(); updateStatusSelection(); });
    connect(m_editor, &Editor::settingsChanged, this, [this] { syncOptionWidgets(); });
    connect(m_editor, &Editor::toolChanged, this, [this](ToolId id) { if (auto *a = m_toolActions.value(int(id))) a->setChecked(true); });
    connect(m_editor, &Editor::textRequested, this, [this](QPoint p) { QTimer::singleShot(0, this, [this, p] { promptText(p); }); });
    connect(m_canvas, &CanvasView::cursorMoved, this, [this](const QPoint &p) {
        m_posLabel->setText(m_canvas->imageContains(p) ? QString("%1, %2 px").arg(p.x()).arg(p.y()) : QString());
    });
    connect(m_canvas, &CanvasView::cursorLeft, this, [this] { m_posLabel->clear(); });
    connect(m_canvas, &CanvasView::zoomChanged, this, [this](double z) { m_zoomLabel->setText(QString("%1%").arg(qRound(z * 100))); });
    connect(&m_editor->document(), &DrawingDocument::imageReplaced, this, [this] {
        const QSize s = m_editor->document().image().size();
        m_sizeLabel->setText(QString("%1 × %2 px").arg(s.width()).arg(s.height()));
    });

    syncOptionWidgets();
    updateTitle();
    updateActions();
    const QSize s = m_editor->document().image().size();
    m_sizeLabel->setText(QString("%1 × %2 px").arg(s.width()).arg(s.height()));
    resize(1280, 800);
}

QAction *MainWindow::addAction(QMenu *menu, const QString &text, const QString &objName,
                               const QKeySequence &key, std::function<void()> fn) {
    auto *a = new QAction(text, this);
    a->setObjectName(objName);
    if (!key.isEmpty()) a->setShortcut(key);
    connect(a, &QAction::triggered, this, [fn] { fn(); });
    menu->addAction(a);
    QWidget::addAction(a);    // QWidget::addAction so shortcuts work even if the menu is hidden
    m_actions.insert(objName, a);
    return a;
}

void MainWindow::buildMenus() {
    Editor *e = m_editor;
    QMenu *file = menuBar()->addMenu(tr("&File"));
    addAction(file, tr("&New…"), "actionNew", QKeySequence::New, [this] { newDialog(); });
    addAction(file, tr("&Open…"), "actionOpen", QKeySequence::Open, [this] { openDialog(); });
    file->addSeparator();
    addAction(file, tr("&Save"), "actionSave", QKeySequence::Save, [this] { saveCurrent(); });
    addAction(file, tr("Save &As…"), "actionSaveAs", QKeySequence("Ctrl+Shift+S"), [this] { saveAsDialog(); });
    file->addSeparator();
    addAction(file, tr("E&xit"), "actionExit", QKeySequence("Ctrl+Q"), [this] { close(); });

    QMenu *edit = menuBar()->addMenu(tr("&Edit"));
    addAction(edit, tr("&Undo"), "actionUndo", QKeySequence::Undo, [e] { e->undo(); });
    addAction(edit, tr("&Redo"), "actionRedo", QKeySequence("Ctrl+Y"), [e] { e->redo(); })
        ->setShortcuts({QKeySequence("Ctrl+Y"), QKeySequence("Ctrl+Shift+Z")});
    edit->addSeparator();
    addAction(edit, tr("Cu&t"), "actionCut", QKeySequence::Cut, [e] { e->cut(); });
    addAction(edit, tr("&Copy"), "actionCopy", QKeySequence::Copy, [e] { e->copy(); });
    addAction(edit, tr("&Paste"), "actionPaste", QKeySequence::Paste, [e] { e->paste(); });
    addAction(edit, tr("&Delete"), "actionDelete", QKeySequence::Delete, [e] { e->deleteSelection(); });
    edit->addSeparator();
    addAction(edit, tr("Select &All"), "actionSelectAll", QKeySequence::SelectAll, [e] { e->selectAll(); });
    addAction(edit, tr("D&eselect"), "actionDeselect", QKeySequence("Ctrl+Shift+A"), [e] { e->deselect(); });
    addAction(edit, tr("Cancel / Deselect"), "actionEscape", QKeySequence(Qt::Key_Escape), [e] {
        if (e->interacting()) e->cancelInteraction(); else e->deselect();
    });
    addAction(edit, tr("Commit Selection"), "actionCommit", QKeySequence(Qt::Key_Return), [e] { e->commitSelection(); });
    edit->addSeparator();
    addAction(edit, tr("S&wap Colours"), "actionSwapColors", QKeySequence(Qt::Key_X), [e] { e->swapColors(); });

    QMenu *image = menuBar()->addMenu(tr("&Image"));
    addAction(image, tr("Crop to Selection"), "actionCrop", QKeySequence("Ctrl+Shift+X"), [e] { e->cropToSelection(); });
    addAction(image, tr("&Resize…"), "actionResize", QKeySequence("Ctrl+W"), [this] {
        SizeDialog d(this, tr("Resize Image"), m_editor->document().image().size(), true);
        if (d.exec() == QDialog::Accepted && !m_editor->resizeImage(d.result(), d.smooth()))
            QMessageBox::warning(this, appDisplayName(), tr("That size is not valid (1 – %1 px).").arg(ImageOps::kMaxDimension));
    });
    addAction(image, tr("&Canvas Size…"), "actionCanvasSize", QKeySequence("Ctrl+E"), [this] {
        SizeDialog d(this, tr("Canvas Size"), m_editor->document().image().size(), false);
        if (d.exec() == QDialog::Accepted && !m_editor->resizeCanvas(d.result()))
            QMessageBox::warning(this, appDisplayName(), tr("That size is not valid (1 – %1 px).").arg(ImageOps::kMaxDimension));
    });
    image->addSeparator();
    addAction(image, tr("Rotate Right 90°"), "actionRotateRight", QKeySequence("Ctrl+]"), [e] { e->rotate(ImageOps::Rotation::Cw90); });
    addAction(image, tr("Rotate Left 90°"), "actionRotateLeft", QKeySequence("Ctrl+["), [e] { e->rotate(ImageOps::Rotation::Ccw90); });
    addAction(image, tr("Rotate 180°"), "actionRotate180", QKeySequence(), [e] { e->rotate(ImageOps::Rotation::Rotate180); });
    addAction(image, tr("Flip &Horizontal"), "actionFlipH", QKeySequence(), [e] { e->flip(true); });
    addAction(image, tr("Flip &Vertical"), "actionFlipV", QKeySequence(), [e] { e->flip(false); });

    QMenu *view = menuBar()->addMenu(tr("&View"));
    addAction(view, tr("Zoom &In"), "actionZoomIn", QKeySequence("Ctrl+="), [this] { m_canvas->zoomIn(); })
        ->setShortcuts({QKeySequence("Ctrl+="), QKeySequence("Ctrl++")});
    addAction(view, tr("Zoom &Out"), "actionZoomOut", QKeySequence("Ctrl+-"), [this] { m_canvas->zoomOut(); });
    addAction(view, tr("&Actual Size"), "actionZoomReset", QKeySequence("Ctrl+0"), [this] { m_canvas->setZoom(1.0); });

    menuBar()->addMenu(tr("&Tools"))->setObjectName("toolsMenu");   // populated in buildToolBars()

    QMenu *help = menuBar()->addMenu(tr("&Help"));
    addAction(help, tr("&About Pixelle"), "actionAbout", QKeySequence(), [this] {
        QMessageBox::about(this, tr("About Pixelle"),
            tr("<b>Pixelle</b> %1<br>A raster image editor built with Qt %2.<br><br>"
               "The interface is a functional layout; it is not yet a reproduction of any other application's UI.")
               .arg(QApplication::applicationVersion(), QString::fromLatin1(qVersion())));
    });
}

void MainWindow::buildToolBars() {
    struct T { ToolId id; const char *label; const char *tip; } tools[] = {
        {ToolId::SelectRect, "Select", "Rectangular selection (drag to move, Ctrl+drag to copy)"},
        {ToolId::SelectFree, "Free Select", "Free-form selection"},
        {ToolId::Pencil, "Pencil", "Pencil (aliased)"},
        {ToolId::Brush, "Brush", "Brush"},
        {ToolId::Eraser, "Eraser", "Eraser (paints the background colour)"},
        {ToolId::Fill, "Fill", "Fill with colour"},
        {ToolId::Eyedropper, "Pick", "Colour picker"},
        {ToolId::Line, "Line", "Line (Shift: 45° steps)"},
        {ToolId::Shape, "Shape", "Shape (Shift: square/circle)"},
        {ToolId::Text, "Text", "Text"},
    };
    auto *tb = new QToolBar(tr("Tools"), this);
    tb->setObjectName("toolsBar");
    tb->setOrientation(Qt::Vertical);
    tb->setToolButtonStyle(Qt::ToolButtonTextOnly);
    tb->setMovable(false);
    addToolBar(Qt::LeftToolBarArea, tb);
    auto *group = new QActionGroup(this);
    QMenu *toolsMenu = menuBar()->findChild<QMenu *>("toolsMenu");
    for (const T &t : tools) {
        auto *a = new QAction(tr(t.label), this);
        a->setCheckable(true);
        a->setToolTip(tr(t.tip));
        a->setObjectName(QString("tool_%1").arg(int(t.id)));
        group->addAction(a);
        tb->addAction(a);
        if (toolsMenu) toolsMenu->addAction(a);
        m_toolActions.insert(int(t.id), a);
        m_actions.insert(a->objectName(), a);
        const ToolId id = t.id;
        connect(a, &QAction::triggered, this, [this, id] { m_editor->setTool(id); });
        if (id == m_editor->toolId()) a->setChecked(true);
    }

    auto *ob = new QToolBar(tr("Options"), this);
    ob->setObjectName("optionsBar");
    ob->setMovable(false);
    addToolBar(Qt::TopToolBarArea, ob);
    m_brushBox = new QComboBox;
    m_brushBox->addItems({"Round", "Calligraphy 1", "Calligraphy 2", "Airbrush", "Crayon", "Marker"});
    m_sizeSpin = new QSpinBox; m_sizeSpin->setRange(1, 500); m_sizeSpin->setSuffix(" px");
    m_shapeBox = new QComboBox;
    m_shapeBox->addItems({"Rectangle", "Rounded rectangle", "Ellipse", "Triangle", "Diamond", "Pentagon", "Hexagon", "Star"});
    m_fillBox = new QComboBox; m_fillBox->addItems({"Outline", "Fill", "Outline + fill"});
    m_aaCheck = new QCheckBox(tr("Antialias"));
    m_eraseTransparent = new QCheckBox(tr("Erase to transparent"));
    m_tolSpin = new QSpinBox; m_tolSpin->setRange(0, 255); m_tolSpin->setToolTip(tr("Fill tolerance"));
    ob->addWidget(new QLabel(tr(" Brush ")));  ob->addWidget(m_brushBox);
    ob->addWidget(new QLabel(tr(" Size ")));   ob->addWidget(m_sizeSpin);
    ob->addWidget(new QLabel(tr(" Shape ")));  ob->addWidget(m_shapeBox);
    ob->addWidget(m_fillBox);
    ob->addWidget(m_aaCheck);
    ob->addWidget(m_eraseTransparent);
    ob->addWidget(new QLabel(tr(" Fill tol. "))); ob->addWidget(m_tolSpin);
    ob->addSeparator();
    ob->addWidget(m_palette);

    Editor *e = m_editor;
    connect(m_brushBox, &QComboBox::activated, this, [e](int i) { e->setBrush(BrushType(i)); });
    connect(m_sizeSpin, qOverload<int>(&QSpinBox::valueChanged), this, [e](int v) { e->setSize(v); });
    connect(m_shapeBox, &QComboBox::activated, this, [e](int i) { e->setShape(ShapeType(i)); });
    connect(m_fillBox, &QComboBox::activated, this, [e](int i) { e->setFillMode(FillMode(i)); });
    connect(m_aaCheck, &QCheckBox::toggled, this, [e](bool on) { e->setAntialias(on); });
    connect(m_eraseTransparent, &QCheckBox::toggled, this, [e](bool on) { e->setEraserTransparent(on); });
    connect(m_tolSpin, qOverload<int>(&QSpinBox::valueChanged), this, [e](int v) { e->setFillTolerance(v); });
}

void MainWindow::buildStatusBar() {
    m_posLabel = new QLabel; m_selLabel = new QLabel; m_sizeLabel = new QLabel; m_zoomLabel = new QLabel("100%");
    for (QLabel *l : {m_posLabel, m_selLabel, m_sizeLabel, m_zoomLabel}) l->setMinimumWidth(110);
    statusBar()->addWidget(m_posLabel);
    statusBar()->addWidget(m_selLabel);
    statusBar()->addPermanentWidget(m_sizeLabel);
    statusBar()->addPermanentWidget(m_zoomLabel);
}

void MainWindow::syncOptionWidgets() {
    const ToolSettings &s = m_editor->settings();
    for (QObject *o : QList<QObject *>{m_brushBox, m_sizeSpin, m_shapeBox, m_fillBox, m_aaCheck, m_eraseTransparent, m_tolSpin}) o->blockSignals(true);
    m_brushBox->setCurrentIndex(int(s.brush)); m_sizeSpin->setValue(s.size); m_shapeBox->setCurrentIndex(int(s.shape));
    m_fillBox->setCurrentIndex(int(s.fillMode)); m_aaCheck->setChecked(s.antialias);
    m_eraseTransparent->setChecked(s.eraserTransparent); m_tolSpin->setValue(s.fillTolerance);
    for (QObject *o : QList<QObject *>{m_brushBox, m_sizeSpin, m_shapeBox, m_fillBox, m_aaCheck, m_eraseTransparent, m_tolSpin}) o->blockSignals(false);
}

void MainWindow::updateStatusSelection() {
    const Selection &s = m_editor->selection();
    m_selLabel->setText(s.active ? QString("Selection: %1 × %2 px").arg(s.bounds.width()).arg(s.bounds.height()) : QString());
}

void MainWindow::updateActions() {
    const bool sel = m_editor->hasSelection();
    m_actions["actionUndo"]->setEnabled(m_editor->canUndo());
    m_actions["actionRedo"]->setEnabled(m_editor->canRedo());
    for (const char *n : {"actionCut", "actionCopy", "actionDelete", "actionCrop", "actionDeselect"}) m_actions[n]->setEnabled(sel);
}

void MainWindow::updateTitle() {
    const QString path = m_editor->document().filePath();
    const QString name = path.isEmpty() ? tr("Untitled") : QFileInfo(path).fileName();
    setWindowTitle(QString("%1%2 - %3").arg(name, m_editor->document().isDirty() ? "*" : "", appDisplayName()));
}

// ---------------- file workflow ----------------
bool MainWindow::maybeSave() {
    m_editor->commitSelection();               // a pending move/paste is part of the document
    if (!m_editor->document().isDirty()) return true;
    const QString name = m_editor->document().filePath().isEmpty() ? tr("Untitled") : QFileInfo(m_editor->document().filePath()).fileName();
    const auto r = QMessageBox::warning(this, appDisplayName(), tr("Do you want to save changes to %1?").arg(name),
                                        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel, QMessageBox::Save);
    if (r == QMessageBox::Cancel) return false;
    if (r == QMessageBox::Discard) return true;
    return saveCurrent();
}

bool MainWindow::newDialog() {
    if (!maybeSave()) return false;
    SizeDialog d(this, tr("New Image"), m_editor->document().image().size(), false);
    if (d.exec() != QDialog::Accepted) return false;
    if (!ImageOps::validSize(d.result())) return false;
    m_editor->newDocument(d.result(), Qt::white);
    return true;
}

bool MainWindow::openFile(const QString &path) {
    QImage img;
    const FileResult r = ImageFileService::load(path, &img);
    if (!r.ok) { QMessageBox::critical(this, appDisplayName(), tr("Could not open “%1”:\n%2").arg(QDir::toNativeSeparators(path), r.error)); return false; }
    m_editor->loadImage(img, path);
    m_lastDir = QFileInfo(path).absolutePath();
    return true;
}

bool MainWindow::openDialog() {
    if (!maybeSave()) return false;
    const QString path = QFileDialog::getOpenFileName(this, tr("Open"), m_lastDir, tr("Images (*.png *.jpg *.jpeg *.bmp *.webp *.tif *.tiff *.gif);;All files (*)"));
    return !path.isEmpty() && openFile(path);
}

bool MainWindow::saveFile(const QString &path, bool overwriteConfirmed) {
    m_editor->commitSelection();
    const QImage &img = m_editor->document().image();
    const QString ext = ImageFileService::normalizedExtension(path);
    if (ImageFileService::wouldLoseTransparency(img, ext) &&
        QMessageBox::warning(this, appDisplayName(), tr("The %1 format does not support transparency. Transparent areas will be filled with white. Save anyway?").arg(ext.toUpper()),
                             QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes)
        return false;
    const FileResult r = ImageFileService::save(img, path, overwriteConfirmed);
    if (!r.ok) { QMessageBox::critical(this, appDisplayName(), tr("Could not save “%1”:\n%2").arg(QDir::toNativeSeparators(path), r.error)); return false; }
    m_editor->markSaved(path);
    m_lastDir = QFileInfo(path).absolutePath();
    return true;
}

bool MainWindow::saveCurrent() {
    const QString path = m_editor->document().filePath();
    return path.isEmpty() ? saveAsDialog() : saveFile(path, true);    // re-saving the user's own file is the intended overwrite
}

bool MainWindow::saveAsDialog() {
    QString selected;
    const QString start = m_lastDir.isEmpty() ? QStringLiteral("Untitled.png") : m_lastDir + "/Untitled.png";
    QString path = QFileDialog::getSaveFileName(this, tr("Save As"), start, ImageFileService::fileDialogFilter(), &selected);
    if (path.isEmpty()) return false;
    bool confirmedByDialog = true;     // QFileDialog asks before replacing an existing file
    if (!ImageFileService::supportedWriteExtensions().contains(QFileInfo(path).suffix().toLower())) {
        path += ".png";                // no/unknown extension: append, then re-check for a clash ourselves
        confirmedByDialog = false;
    }
    if (!confirmedByDialog && QFileInfo::exists(path) &&
        QMessageBox::question(this, appDisplayName(), tr("“%1” already exists. Replace it?").arg(QFileInfo(path).fileName()),
                              QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes)
        return false;
    return saveFile(path, true);
}

void MainWindow::promptText(const QPoint &pos) {
    QFont f = m_editor->settings().size > 0 ? QFont("Noto Sans", 14) : QFont();
    TextDialog d(this, f);
    if (d.exec() == QDialog::Accepted && !d.text().isEmpty()) m_editor->placeText(d.text(), d.font(), pos);
}

void MainWindow::closeEvent(QCloseEvent *e) { maybeSave() ? e->accept() : e->ignore(); }
