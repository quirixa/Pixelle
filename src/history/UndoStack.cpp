#include "UndoStack.h"
#include "drawing/DrawingDocument.h"
#include <cstring>

// Exact pixel copy (no compositing, so semi-transparent pixels restore bit-exactly).
static void blit(QImage &dst, const QPoint &at, const QImage &srcIn) {
    const QImage src = srcIn.format() == dst.format() ? srcIn : srcIn.convertToFormat(dst.format());
    const QRect r = QRect(at, src.size()).intersected(dst.rect());
    if (r.isEmpty()) return;
    const int bpp = dst.depth() / 8;
    for (int y = r.top(); y <= r.bottom(); ++y)
        std::memcpy(dst.scanLine(y) + r.left() * bpp,
                    src.constScanLine(y - at.y()) + (r.left() - at.x()) * bpp,
                    size_t(r.width()) * bpp);
}

UndoStack::UndoStack(DrawingDocument *doc, qint64 maxBytes) : m_doc(doc), m_maxBytes(maxBytes) {}

void UndoStack::pushEntry(Entry &&e) {
    if (m_cleanIndex > m_index) m_cleanIndex = -1;   // clean state lived in the dropped redo branch
    m_entries.resize(m_index);
    m_entries.push_back(std::move(e));
    m_index = m_entries.size();
    trim();
    m_doc->setDirty(true);
}

void UndoStack::pushRegion(const QRect &rect, const QImage &before) {
    const QRect r = rect.intersected(m_doc->image().rect());
    if (r.isEmpty()) return;
    Entry e;
    e.rect = r;
    e.before = before.size() == r.size() ? before : before.copy();
    e.after = m_doc->image().copy(r);
    pushEntry(std::move(e));
}

void UndoStack::pushFull(const QImage &before) {
    Entry e;
    e.full = true;
    e.before = before;
    e.after = m_doc->image();
    pushEntry(std::move(e));
}

void UndoStack::beginRegion() {
    if (m_regionOpen) abortRegion();
    m_tiles.clear();
    m_regionOpen = true;
}

void UndoStack::touch(const QRect &rect) {
    if (!m_regionOpen) return;
    const QImage &img = m_doc->image();
    const QRect r = rect.intersected(img.rect());
    if (r.isEmpty()) return;
    for (int ty = r.top() / kTile; ty <= r.bottom() / kTile; ++ty)
        for (int tx = r.left() / kTile; tx <= r.right() / kTile; ++tx) {
            const quint64 key = (quint64(ty) << 32) | quint32(tx);
            if (m_tiles.contains(key)) continue;
            const QRect tr = QRect(tx * kTile, ty * kTile, kTile, kTile).intersected(img.rect());
            m_tiles.insert(key, Tile{tr, img.copy(tr)});
        }
}

bool UndoStack::commitRegion() {
    if (!m_regionOpen) return false;
    m_regionOpen = false;
    if (m_tiles.isEmpty()) return false;
    QRect bb;
    for (const Tile &t : std::as_const(m_tiles)) bb |= t.rect;
    QImage before = m_doc->image().copy(bb);
    for (const Tile &t : std::as_const(m_tiles)) blit(before, t.rect.topLeft() - bb.topLeft(), t.img);
    QImage after = m_doc->image().copy(bb);
    m_tiles.clear();
    if (before == after) return false;            // no net change: no history entry
    Entry e;
    e.rect = bb;
    e.before = before;
    e.after = after;
    pushEntry(std::move(e));
    return true;
}

void UndoStack::abortRegion() {
    if (!m_regionOpen) return;
    m_regionOpen = false;
    QRect bb;
    for (const Tile &t : std::as_const(m_tiles)) {
        blit(m_doc->mutableImage(), t.rect.topLeft(), t.img);
        bb |= t.rect;
    }
    m_tiles.clear();
    if (!bb.isNull()) m_doc->notifyChanged(bb);
}

void UndoStack::apply(const Entry &e, bool useAfter) {
    const QImage &src = useAfter ? e.after : e.before;
    if (e.full) {
        m_doc->mutableImage() = src;
        emit m_doc->imageReplaced();
    } else {
        blit(m_doc->mutableImage(), e.rect.topLeft(), src);
        m_doc->notifyChanged(e.rect);
    }
}

void UndoStack::undo() {
    if (!canUndo()) return;
    apply(m_entries[--m_index], false);
    m_doc->setDirty(!isClean());
}

void UndoStack::redo() {
    if (!canRedo()) return;
    apply(m_entries[m_index++], true);
    m_doc->setDirty(!isClean());
}

void UndoStack::clear() {
    m_entries.clear();
    m_tiles.clear();
    m_regionOpen = false;
    m_index = 0;
    m_cleanIndex = 0;
}

qint64 UndoStack::memoryUsage() const {
    qint64 t = 0;
    for (const Entry &e : m_entries) t += e.bytes();
    return t;
}

void UndoStack::trim() {
    qint64 total = memoryUsage();
    while (total > m_maxBytes && m_entries.size() > 1) {
        total -= m_entries.first().bytes();
        m_entries.removeFirst();
        --m_index;
        if (m_cleanIndex >= 0) --m_cleanIndex;
        if (m_cleanIndex < 0) m_cleanIndex = -1;
    }
}
