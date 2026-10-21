#include "Editor.h"
#include <QClipboard>
#include <QGuiApplication>
#include <QPainter>

Editor::Editor(QObject *parent) : QObject(parent), m_doc(this), m_undo(&m_doc) {
    for (int i = 0; i < int(ToolId::Count); ++i) m_tools.push_back(makeTool(ToolId(i)));
    m_tool = m_tools[int(m_toolId)].get();
}

// ---------------- settings ----------------
void Editor::setForeground(const QColor &c) { if (m_set.fg == c) return; m_set.fg = c; emit settingsChanged(); }
void Editor::setBackground(const QColor &c) { if (m_set.bg == c) return; m_set.bg = c; emit settingsChanged(); }
void Editor::swapColors() { std::swap(m_set.fg, m_set.bg); emit settingsChanged(); }
void Editor::setSize(int s) { s = qBound(1, s, 500); if (m_set.size == s) return; m_set.size = s; emit settingsChanged(); }
void Editor::setBrush(BrushType b) { m_set.brush = b; emit settingsChanged(); }
void Editor::setShape(ShapeType s) { m_set.shape = s; emit settingsChanged(); }
void Editor::setFillMode(FillMode m) { m_set.fillMode = m; emit settingsChanged(); }
void Editor::setAntialias(bool on) { m_set.antialias = on; emit settingsChanged(); }
void Editor::setFillTolerance(int t) { m_set.fillTolerance = qBound(0, t, 255); emit settingsChanged(); }
void Editor::setEraserTransparent(bool on) { m_set.eraserTransparent = on; emit settingsChanged(); }

// ---------------- tools / pointer ----------------
void Editor::setTool(ToolId id) {
    if (id == m_toolId) return;
    cancelInteraction();
    m_toolId = id;
    m_tool = m_tools[int(id)].get();
    emit toolChanged(id);
}

void Editor::pointerPress(const QPoint &p, Qt::MouseButton b, Qt::KeyboardModifiers m) {
    if (b != Qt::LeftButton && b != Qt::RightButton) return;
    if (m_activeBtn != Qt::NoButton) { cancelInteraction(); return; }   // second button cancels, like Paint
    if (!isSelectionTool(m_toolId)) commitSelection();
    m_activeBtn = b;
    m_tool->press(*this, p, b, m);
    if (!m_tool->active()) m_activeBtn = Qt::NoButton;   // click-only tools (fill, text) finish on press
}

void Editor::pointerMove(const QPoint &p, Qt::KeyboardModifiers m) {
    if (m_activeBtn != Qt::NoButton) m_tool->move(*this, p, m);
}

void Editor::pointerRelease(const QPoint &p, Qt::MouseButton b, Qt::KeyboardModifiers m) {
    if (b != m_activeBtn) return;
    m_tool->release(*this, p, m);
    m_activeBtn = Qt::NoButton;
}

void Editor::cancelInteraction() {
    if (m_activeBtn == Qt::NoButton) return;
    m_tool->cancel(*this);
    m_activeBtn = Qt::NoButton;
    clearPreview();
    clearRubber();
}

// ---------------- edit transactions ----------------
void Editor::beginEdit() { m_undo.beginRegion(); }
void Editor::commitEdit() { m_undo.commitRegion(); emit historyChanged(); }
void Editor::abortEdit() { m_undo.abortRegion(); }

// ---------------- preview ----------------
void Editor::setPreview(const QImage &img, const QPoint &pos) { m_preview = img; m_previewPos = pos; emit previewChanged(); }
void Editor::clearPreview() { if (m_preview.isNull()) return; m_preview = QImage(); emit previewChanged(); }
void Editor::setRubber(const QPainterPath &p) { m_rubber = p; emit previewChanged(); }
void Editor::clearRubber() { if (m_rubber.isEmpty()) return; m_rubber = QPainterPath(); emit previewChanged(); }

void Editor::paintOverlay(QPainter &p) const {
    if (m_sel.active && m_sel.lifted) {
        p.save();
        if (m_sel.hasSource) p.fillPath(m_sel.path, m_set.bg);      // the "hole" left by a moved selection
        p.setClipPath(m_sel.destPath());
        p.drawImage(m_sel.pos, m_sel.floating);
        p.restore();
    }
    if (!m_preview.isNull()) p.drawImage(m_previewPos, m_preview);
}

// ---------------- history ----------------
void Editor::undo() {
    if (m_activeBtn != Qt::NoButton) return;
    if (m_sel.pending()) {                 // undo of an uncommitted move/paste: discard it, doc untouched
        m_sel = Selection();
        emit selectionChanged(); emit previewChanged(); emit historyChanged();
        return;
    }
    if (m_sel.active) { m_sel = Selection(); emit selectionChanged(); }
    m_undo.undo();
    emit historyChanged();
}

void Editor::redo() {
    if (m_activeBtn != Qt::NoButton) return;
    if (m_sel.active) { m_sel = Selection(); emit selectionChanged(); }
    m_undo.redo();
    emit historyChanged();
}

// ---------------- selection ----------------
bool Editor::selectionContains(const QPoint &p) const {
    return m_sel.active && m_sel.destPath().contains(QPointF(p) + QPointF(0.5, 0.5));
}

bool Editor::selectRect(const QRect &rIn) {
    commitSelection();
    const QRect r = rIn.normalized().intersected(m_doc.image().rect());
    if (r.isEmpty()) return false;
    m_sel = Selection();
    m_sel.active = true;
    m_sel.bounds = r;
    m_sel.pos = r.topLeft();
    m_sel.path.addRect(r);
    emit selectionChanged(); emit previewChanged();
    return true;
}

bool Editor::selectPath(const QPainterPath &path) {
    commitSelection();
    QPainterPath clip;
    clip.addRect(m_doc.image().rect());
    const QPainterPath p = path.intersected(clip);
    const QRect b = p.boundingRect().toAlignedRect().intersected(m_doc.image().rect());
    if (b.isEmpty()) return false;
    m_sel = Selection();
    m_sel.active = true;
    m_sel.path = p;
    m_sel.bounds = b;
    m_sel.pos = b.topLeft();
    emit selectionChanged(); emit previewChanged();
    return true;
}

void Editor::selectAll() { selectRect(m_doc.image().rect()); }

QImage Editor::grab(const QPainterPath &path, const QRect &bounds) const {
    QImage out(bounds.size(), QImage::Format_ARGB32);
    out.fill(Qt::transparent);
    QPainter p(&out);
    p.setClipPath(path.translated(-bounds.topLeft()));
    p.setCompositionMode(QPainter::CompositionMode_Source);
    p.drawImage(-bounds.topLeft(), m_doc.image());
    return out;
}

void Editor::ensureLifted() {
    if (!m_sel.active || m_sel.lifted) return;
    m_sel.floating = grab(m_sel.path, m_sel.bounds);
    m_sel.lifted = true;
}

void Editor::beginSelectionDrag(bool clone) {
    if (!m_sel.active) return;
    m_dragBackup = m_sel;
    ensureLifted();
    if (clone) m_sel.hasSource = false;   // Ctrl-drag: leave the original in place
    emit selectionChanged(); emit previewChanged();
}

void Editor::moveSelectionBy(const QPoint &d) {
    if (!m_sel.active || d.isNull()) return;
    m_sel.pos += d;
    emit selectionChanged(); emit previewChanged();
}

void Editor::cancelSelectionDrag() {
    if (!m_dragBackup.active) return;
    m_sel = m_dragBackup;
    m_dragBackup = Selection();
    emit selectionChanged(); emit previewChanged();
}

void Editor::commitSelection() {
    if (!m_sel.active) return;
    if (m_sel.pending()) {
        const QRect dst = m_sel.destRect();
        m_undo.beginRegion();
        if (m_sel.hasSource) m_undo.touch(m_sel.bounds);
        m_undo.touch(dst);
        {
            QPainter p(&m_doc.mutableImage());
            if (m_sel.hasSource) {
                p.setCompositionMode(QPainter::CompositionMode_Source);
                p.fillPath(m_sel.path, m_set.bg);
            }
            p.setClipPath(m_sel.destPath());
            p.setCompositionMode(m_sel.opaque ? QPainter::CompositionMode_Source : QPainter::CompositionMode_SourceOver);
            p.drawImage(m_sel.pos, m_sel.floating);
        }
        m_undo.commitRegion();
        m_doc.notifyChanged(m_sel.bounds.united(dst));
    }
    m_sel = Selection();
    m_dragBackup = Selection();
    emit selectionChanged(); emit previewChanged(); emit historyChanged();
}

bool Editor::deleteSelection() {
    if (!m_sel.active) return false;
    if (m_sel.hasSource) {
        m_undo.beginRegion();
        m_undo.touch(m_sel.bounds);
        {
            QPainter p(&m_doc.mutableImage());
            p.setCompositionMode(QPainter::CompositionMode_Source);
            p.fillPath(m_sel.path, m_set.bg);
        }
        m_undo.commitRegion();
        m_doc.notifyChanged(m_sel.bounds);
    }   // else: pasted/cloned floating content is simply discarded; document unchanged
    m_sel = Selection();
    emit selectionChanged(); emit previewChanged(); emit historyChanged();
    return true;
}

QImage Editor::selectionImage() {
    if (!m_sel.active) return {};
    return m_sel.lifted ? m_sel.floating : grab(m_sel.path, m_sel.bounds);
}

bool Editor::copy() {
    const QImage img = selectionImage();
    if (img.isNull()) return false;
    if (QClipboard *cb = QGuiApplication::clipboard()) cb->setImage(img);
    return true;
}

bool Editor::cut() { return copy() && deleteSelection(); }

bool Editor::paste() {
    QClipboard *cb = QGuiApplication::clipboard();
    return cb && pasteImage(cb->image());
}

bool Editor::pasteImage(const QImage &imgIn, const QPoint &at) {
    if (imgIn.isNull()) return false;
    commitSelection();
    const QImage img = imgIn.convertToFormat(QImage::Format_ARGB32);
    const QSize cur = m_doc.image().size();
    if (img.width() > cur.width() || img.height() > cur.height())   // enlarge canvas to fit (own undo step)
        if (!resizeCanvas(cur.expandedTo(img.size()))) return false;
    m_sel = Selection();
    m_sel.active = true;
    m_sel.lifted = true;
    m_sel.hasSource = false;
    m_sel.opaque = false;
    m_sel.floating = img;
    m_sel.bounds = QRect(QPoint(0, 0), img.size());
    m_sel.path.addRect(m_sel.bounds);
    m_sel.pos = at;
    emit selectionChanged(); emit previewChanged(); emit historyChanged();
    return true;
}

bool Editor::placeText(const QString &text, const QFont &font, const QPoint &at) {
    return pasteImage(Raster::renderText(text, font, m_set.fg, m_set.antialias), at);
}

// ---------------- whole-image operations ----------------
bool Editor::applyWhole(const std::function<QImage(const QImage &)> &fn) {
    cancelInteraction();
    commitSelection();
    const QImage before = m_doc.image();
    QImage after = fn(before);
    if (after.isNull()) return false;
    after = after.convertToFormat(QImage::Format_ARGB32);
    m_doc.mutableImage() = after;
    emit m_doc.imageReplaced();
    m_undo.pushFull(before);
    emit historyChanged();
    return true;
}

bool Editor::cropToSelection() {
    if (!m_sel.active) return false;
    const QRect r = m_sel.destRect();       // crop where the selection currently sits
    return applyWhole([&](const QImage &i) { return ImageOps::cropped(i, r); });
}
bool Editor::resizeImage(const QSize &s, bool smooth) {
    return applyWhole([&](const QImage &i) { return ImageOps::resized(i, s, smooth); });
}
bool Editor::resizeCanvas(const QSize &s) {
    return applyWhole([&](const QImage &i) { return ImageOps::canvasResized(i, s, m_set.bg); });
}
bool Editor::rotate(ImageOps::Rotation r) {
    return applyWhole([&](const QImage &i) { return ImageOps::rotated(i, r); });
}
bool Editor::flip(bool h) {
    return applyWhole([&](const QImage &i) { return ImageOps::flipped(i, h); });
}

// ---------------- lifecycle ----------------
void Editor::resetState() {
    cancelInteraction();
    m_sel = Selection();
    m_dragBackup = Selection();
    clearPreview();
    clearRubber();
    m_undo.clear();
    emit selectionChanged(); emit historyChanged();
}

void Editor::newDocument(const QSize &size, const QColor &fill) {
    resetState();
    m_doc.newImage(size, fill);
    m_undo.markClean();
}

bool Editor::loadImage(const QImage &img, const QString &path) {
    if (img.isNull()) return false;
    resetState();
    const bool ok = m_doc.setImage(img, path);
    m_undo.markClean();
    return ok;
}

void Editor::markSaved(const QString &path) {
    m_doc.setFilePath(path);
    m_undo.markClean();
    m_doc.setDirty(false);
}
