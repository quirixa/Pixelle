#pragma once
#include <QWidget>
class Editor;

// Foreground/background indicator + swatch grid. Left click = foreground,
// right click = background; clicking an indicator opens the colour dialog.
// Layout values are UNVERIFIED placeholders (see StyleTokens.h).
class ColorPalette : public QWidget {
    Q_OBJECT
public:
    explicit ColorPalette(Editor *editor, QWidget *parent = nullptr);
    QSize sizeHint() const override;
    static const QList<QColor> &swatches();
    QRect swatchRect(int index) const;
    QRect foregroundRect() const;
    QRect backgroundRect() const;
protected:
    void paintEvent(QPaintEvent *) override;
    void mousePressEvent(QMouseEvent *) override;
private:
    Editor *m_editor;
};
