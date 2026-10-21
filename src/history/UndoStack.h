#pragma once
#include <QHash>
#include <QImage>
#include <QRect>
#include <QVector>

class DrawingDocument;

// Undo history. Three ways to record an edit:
//  * Region transaction (beginRegion/touch/commitRegion): for brush strokes,
//    fills, shapes, selection moves. The pixels of every 64x64 tile are saved
//    lazily the first time a tile is touched, so memory is proportional to the
//    area actually edited, never to the whole image.
//  * pushRegion(): caller already has the 'before' pixels of a rectangle.
//  * pushFull(): whole-image replacement (resize, rotate, crop, canvas size).
class UndoStack {
public:
    static constexpr int kTile = 64;

    explicit UndoStack(DrawingDocument *doc, qint64 maxBytes = 256LL * 1024 * 1024);

    void pushRegion(const QRect &rect, const QImage &before);
    void pushFull(const QImage &before);

    // Region transaction. touch() MUST be called for a rectangle before any
    // pixel inside it is modified.
    void beginRegion();
    void touch(const QRect &rect);
    bool commitRegion();   // true if an undo entry was recorded (false: no net change)
    void abortRegion();    // restores every touched pixel
    bool regionOpen() const { return m_regionOpen; }

    bool canUndo() const { return m_index > 0; }
    bool canRedo() const { return m_index < m_entries.size(); }
    void undo();
    void redo();
    void clear();          // also marks the current state clean
    int count() const { return m_entries.size(); }
    qint64 memoryUsage() const;

    // Saved-state tracking: dirty == "not at the saved history position".
    void markClean() { m_cleanIndex = m_index; }
    bool isClean() const { return m_index == m_cleanIndex; }

private:
    struct Entry {
        bool full = false;
        QRect rect;
        QImage before;
        QImage after;
        qint64 bytes() const { return before.sizeInBytes() + after.sizeInBytes(); }
    };
    struct Tile { QRect rect; QImage img; };

    void pushEntry(Entry &&e);
    void apply(const Entry &e, bool useAfter);
    void trim();

    DrawingDocument *m_doc;
    QVector<Entry> m_entries;
    int m_index = 0;
    int m_cleanIndex = 0;
    qint64 m_maxBytes;
    bool m_regionOpen = false;
    QHash<quint64, Tile> m_tiles;
};
