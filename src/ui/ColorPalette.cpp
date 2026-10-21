#include "ColorPalette.h"
#include "StyleTokens.h"
#include "drawing/Editor.h"
#include <QColorDialog>
#include <QMouseEvent>
#include <QPainter>

using namespace PaintStyle;
static constexpr int kCols = 10;

const QList<QColor> &ColorPalette::swatches() {
    // UNVERIFIED placeholder palette (not Paint's measured palette).
    static const QList<QColor> c = {
        QColor(0, 0, 0), QColor(127, 127, 127), QColor(136, 0, 21), QColor(237, 28, 36), QColor(255, 127, 39),
        QColor(255, 242, 0), QColor(34, 177, 76), QColor(0, 162, 232), QColor(63, 72, 204), QColor(163, 73, 164),
        QColor(255, 255, 255), QColor(195, 195, 195), QColor(185, 122, 87), QColor(255, 174, 201), QColor(255, 201, 14),
        QColor(239, 228, 176), QColor(181, 230, 29), QColor(153, 217, 234), QColor(112, 146, 190), QColor(200, 191, 231)};
    return c;
}

ColorPalette::ColorPalette(Editor *editor, QWidget *parent) : QWidget(parent), m_editor(editor) {
    connect(editor, &Editor::settingsChanged, this, qOverload<>(&QWidget::update));
    setToolTip(tr("Left-click: foreground colour. Right-click: background colour. Click the squares to edit."));
}

QRect ColorPalette::backgroundRect() const { return QRect(6 + 12, 6 + 12, kIndicatorSize, kIndicatorSize); }
QRect ColorPalette::foregroundRect() const { return QRect(6, 6, kIndicatorSize, kIndicatorSize); }
QRect ColorPalette::swatchRect(int i) const {
    const int x0 = 6 + kIndicatorSize + 12 + 16, y0 = 6;
    return QRect(x0 + (i % kCols) * (kSwatchSize + kSwatchGap), y0 + (i / kCols) * (kSwatchSize + kSwatchGap), kSwatchSize, kSwatchSize);
}
QSize ColorPalette::sizeHint() const {
    return QSize(swatchRect(kCols - 1).right() + 8, 2 * (kSwatchSize + kSwatchGap) + 10);
}

void ColorPalette::paintEvent(QPaintEvent *) {
    QPainter p(this);
    p.fillRect(rect(), palette().window());
    auto box = [&](const QRect &r, const QColor &c) {
        p.fillRect(r, QColor(255, 255, 255));
        // checkerboard hint for transparency
        if (c.alpha() < 255) { p.fillRect(r.adjusted(0, 0, -r.width() / 2, -r.height() / 2), QColor(200, 200, 200)); p.fillRect(r.adjusted(r.width() / 2, r.height() / 2, 0, 0), QColor(200, 200, 200)); }
        p.fillRect(r, c);
        p.setPen(QColor(0x80, 0x80, 0x80));
        p.drawRect(r.adjusted(0, 0, -1, -1));
    };
    box(backgroundRect(), m_editor->settings().bg);
    box(foregroundRect(), m_editor->settings().fg);
    for (int i = 0; i < swatches().size(); ++i) box(swatchRect(i), swatches()[i]);
}

void ColorPalette::mousePressEvent(QMouseEvent *e) {
    const QPoint pt = e->position().toPoint();
    if (foregroundRect().contains(pt) || backgroundRect().contains(pt)) {
        const bool fg = foregroundRect().contains(pt);
        const QColor cur = fg ? m_editor->settings().fg : m_editor->settings().bg;
        const QColor c = QColorDialog::getColor(cur, this, fg ? tr("Foreground colour") : tr("Background colour"), QColorDialog::ShowAlphaChannel);
        if (c.isValid()) fg ? m_editor->setForeground(c) : m_editor->setBackground(c);
        return;
    }
    for (int i = 0; i < swatches().size(); ++i)
        if (swatchRect(i).contains(pt)) { m_editor->setColorFor(e->button(), swatches()[i]); return; }
}
