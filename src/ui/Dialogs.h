#pragma once
#include <QDialog>
#include <QFont>
#include <QSize>

class QCheckBox; class QFontComboBox; class QPlainTextEdit; class QRadioButton; class QSpinBox;

// Resize image (percentage or pixels, optional aspect lock, smooth) or canvas size.
class SizeDialog : public QDialog {
    Q_OBJECT
public:
    SizeDialog(QWidget *parent, const QString &title, const QSize &current, bool imageResize);
    QSize result() const;
    bool smooth() const;
private:
    void syncFromWidth(); void syncFromHeight(); void updateEnabled();
    QSize m_orig; bool m_resize;
    QRadioButton *m_pct = nullptr, *m_px = nullptr;
    QSpinBox *m_pctSpin = nullptr, *m_w, *m_h;
    QCheckBox *m_aspect = nullptr, *m_smooth = nullptr;
};

class TextDialog : public QDialog {
    Q_OBJECT
public:
    explicit TextDialog(QWidget *parent, const QFont &initial = QFont());
    QString text() const;
    QFont font() const;
private:
    QPlainTextEdit *m_text; QFontComboBox *m_family; QSpinBox *m_size; QCheckBox *m_bold, *m_italic, *m_underline;
};
