#pragma once
#include "DrawingDocument.h"
#include "DrawingTool.h"
#include "ImageOps.h"
#include "Raster.h"
#include "history/UndoStack.h"
#include <QColor>
#include <QFont>
#include <QObject>
#include <QPainterPath>
#include <QRandomGenerator>
#include <memory>
#include <vector>

struct ToolSettings {
    QColor fg = Qt::black, bg = Qt::white;
    int size = 2;
    BrushType brush = BrushType::Round;
    ShapeType shape = ShapeType::Rectangle;
    FillMode fillMode = FillMode::Outline;
    bool antialias = true;
    int fillTolerance = 0;
    bool eraserTransparent = false;
};

// Selection model. 'path'/'bounds' describe the SOURCE region in image
// coordinates; 'pos' is where the (possibly lifted) content currently sits.
// The document is not modified until the selection is committed, so moving a
// selection is a non-destructive preview.
struct Selection {
    bool active = false;
    QPainterPath path;
    QRect bounds;
    bool lifted = false;      // 'floating' holds the pixels
    bool hasSource = true;    // false for pasted/text/cloned content: source area is left alone
    bool opaque = true;       // true: replaces destination; false: alpha-composites (paste, text)
    QImage floating;
    QPoint pos;
    QPainterPath destPath() const { return path.translated(pos - bounds.topLeft()); }
    QRect destRect() const { return QRect(pos, bounds.size()); }
    bool pending() const { return active && lifted && (!hasSource || pos != bounds.topLeft()); }
};

class Editor : public QObject {
    Q_OBJECT
public:
    explicit Editor(QObject *parent = nullptr);

    DrawingDocument &document() { return m_doc; }
    const DrawingDocument &document() const { return m_doc; }
    UndoStack &undoStack() { return m_undo; }
    const ToolSettings &settings() const { return m_set; }

    // ---- settings ----
    void setForeground(const QColor &c);
    void setBackground(const QColor &c);
    void swapColors();
    void setColorFor(Qt::MouseButton b, const QColor &c) { b == Qt::RightButton ? setBackground(c) : setForeground(c); }
    QColor colorFor(Qt::MouseButton b) const { return b == Qt::RightButton ? m_set.bg : m_set.fg; }
    QColor otherColorFor(Qt::MouseButton b) const { return b == Qt::RightButton ? m_set.fg : m_set.bg; }
    void setSize(int s);
    void setBrush(BrushType b);
    void setShape(ShapeType s);
    void setFillMode(FillMode m);
    void setAntialias(bool on);
    void setFillTolerance(int t);
    void setEraserTransparent(bool on);
    void seedRandom(quint32 seed) { m_rng.seed(seed); }
    QRandomGenerator &rng() { return m_rng; }

    // ---- tools / pointer input (image coordinates) ----
    void setTool(ToolId id);
    ToolId toolId() const { return m_toolId; }
    void pointerPress(const QPoint &p, Qt::MouseButton b, Qt::KeyboardModifiers m = {});
    void pointerMove(const QPoint &p, Qt::KeyboardModifiers m = {});
    void pointerRelease(const QPoint &p, Qt::MouseButton b, Qt::KeyboardModifiers m = {});
    void cancelInteraction();
    bool interacting() const { return m_activeBtn != Qt::NoButton; }

    // ---- edit transactions used by tools ----
    void beginEdit();
    void touch(const QRect &r) { m_undo.touch(r); }
    void commitEdit();
    void abortEdit();

    // ---- preview overlay (never part of the document) ----
    void setPreview(const QImage &img, const QPoint &pos);
    void clearPreview();
    void setRubber(const QPainterPath &p);
    void clearRubber();
    const QPainterPath &rubber() const { return m_rubber; }
    void paintOverlay(QPainter &p) const;   // painter is in image coordinates

    // ---- history ----
    bool canUndo() const { return m_sel.pending() || m_undo.canUndo(); }
    bool canRedo() const { return m_undo.canRedo(); }
    void undo();
    void redo();

    // ---- selection / clipboard ----
    const Selection &selection() const { return m_sel; }
    bool hasSelection() const { return m_sel.active; }
    bool selectionContains(const QPoint &p) const;
    QPainterPath selectionOutline() const { return m_sel.active ? m_sel.destPath() : QPainterPath(); }
    bool selectRect(const QRect &r);
    bool selectPath(const QPainterPath &p);
    void selectAll();
    void beginSelectionDrag(bool clone);
    void moveSelectionBy(const QPoint &d);
    void cancelSelectionDrag();
    void commitSelection();            // apply floating pixels (one undo entry) and drop the selection
    void deselect() { commitSelection(); }
    bool deleteSelection();
    QImage selectionImage();           // pixels of the selection (null if none)
    bool copy();
    bool cut();
    bool paste();
    bool pasteImage(const QImage &img, const QPoint &at = QPoint(0, 0));
    void requestText(const QPoint &p) { emit textRequested(p); }
    bool placeText(const QString &text, const QFont &font, const QPoint &at);

    // ---- whole-image operations (each is one undo entry) ----
    bool cropToSelection();
    bool resizeImage(const QSize &s, bool smooth = true);
    bool resizeCanvas(const QSize &s);
    bool rotate(ImageOps::Rotation r);
    bool flip(bool horizontal);

    // ---- document lifecycle ----
    void newDocument(const QSize &size, const QColor &fill = Qt::white);
    bool loadImage(const QImage &img, const QString &path);
    void markSaved(const QString &path);

signals:
    void settingsChanged();
    void toolChanged(ToolId);
    void selectionChanged();
    void previewChanged();
    void historyChanged();
    void textRequested(QPoint imagePos);

private:
    bool applyWhole(const std::function<QImage(const QImage &)> &fn);
    void ensureLifted();
    QImage grab(const QPainterPath &path, const QRect &bounds) const;
    void resetState();

    DrawingDocument m_doc;
    UndoStack m_undo;
    ToolSettings m_set;
    std::vector<std::unique_ptr<Tool>> m_tools;
    Tool *m_tool = nullptr;
    ToolId m_toolId = ToolId::Pencil;
    Qt::MouseButton m_activeBtn = Qt::NoButton;
    Selection m_sel, m_dragBackup;
    QImage m_preview;
    QPoint m_previewPos;
    QPainterPath m_rubber;
    QRandomGenerator m_rng{12345u};
};
