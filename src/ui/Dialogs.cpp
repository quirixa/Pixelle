#include "Dialogs.h"
#include "drawing/ImageOps.h"
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QFontComboBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QPlainTextEdit>
#include <QRadioButton>
#include <QSpinBox>
#include <QVBoxLayout>

SizeDialog::SizeDialog(QWidget *parent, const QString &title, const QSize &cur, bool imageResize)
    : QDialog(parent), m_orig(cur), m_resize(imageResize) {
    setWindowTitle(title);
    auto *form = new QFormLayout;
    if (m_resize) {
        m_pct = new QRadioButton(tr("Percentage"));
        m_px = new QRadioButton(tr("Pixels"));
        m_pct->setChecked(true);
        m_pctSpin = new QSpinBox; m_pctSpin->setRange(1, 1000); m_pctSpin->setValue(100); m_pctSpin->setSuffix(" %");
        form->addRow(m_pct, m_pctSpin);
        form->addRow(m_px, new QWidget);
    }
    m_w = new QSpinBox; m_w->setRange(1, ImageOps::kMaxDimension); m_w->setValue(cur.width());
    m_h = new QSpinBox; m_h->setRange(1, ImageOps::kMaxDimension); m_h->setValue(cur.height());
    form->addRow(tr("Width (px):"), m_w);
    form->addRow(tr("Height (px):"), m_h);
    if (m_resize) {
        m_aspect = new QCheckBox(tr("Maintain aspect ratio")); m_aspect->setChecked(true);
        m_smooth = new QCheckBox(tr("Smooth resampling")); m_smooth->setChecked(true);
        form->addRow(m_aspect); form->addRow(m_smooth);
        connect(m_pct, &QRadioButton::toggled, this, [this] { updateEnabled(); });
        connect(m_w, qOverload<int>(&QSpinBox::valueChanged), this, [this] { syncFromWidth(); });
        connect(m_h, qOverload<int>(&QSpinBox::valueChanged), this, [this] { syncFromHeight(); });
        updateEnabled();
    }
    auto *bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(bb, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(bb, &QDialogButtonBox::rejected, this, &QDialog::reject);
    auto *lay = new QVBoxLayout(this); lay->addLayout(form); lay->addWidget(bb);
}

void SizeDialog::updateEnabled() {
    const bool pct = m_pct->isChecked();
    m_pctSpin->setEnabled(pct); m_w->setEnabled(!pct); m_h->setEnabled(!pct); m_aspect->setEnabled(!pct);
}
void SizeDialog::syncFromWidth() {
    if (!m_aspect->isChecked() || !m_w->hasFocus()) return;
    QSignalBlocker b(m_h); m_h->setValue(qMax(1, qRound(double(m_w->value()) * m_orig.height() / m_orig.width())));
}
void SizeDialog::syncFromHeight() {
    if (!m_aspect->isChecked() || !m_h->hasFocus()) return;
    QSignalBlocker b(m_w); m_w->setValue(qMax(1, qRound(double(m_h->value()) * m_orig.width() / m_orig.height())));
}
QSize SizeDialog::result() const {
    if (m_resize && m_pct->isChecked()) {
        const double f = m_pctSpin->value() / 100.0;
        return QSize(qMax(1, qRound(m_orig.width() * f)), qMax(1, qRound(m_orig.height() * f)));
    }
    return QSize(m_w->value(), m_h->value());
}
bool SizeDialog::smooth() const { return !m_smooth || m_smooth->isChecked(); }

TextDialog::TextDialog(QWidget *parent, const QFont &f) : QDialog(parent) {
    setWindowTitle(tr("Text"));
    m_text = new QPlainTextEdit; m_text->setMinimumSize(320, 120);
    m_family = new QFontComboBox; m_family->setCurrentFont(f);
    m_size = new QSpinBox; m_size->setRange(4, 400); m_size->setValue(f.pointSize() > 0 ? f.pointSize() : 14);
    m_bold = new QCheckBox(tr("Bold")); m_bold->setChecked(f.bold());
    m_italic = new QCheckBox(tr("Italic")); m_italic->setChecked(f.italic());
    m_underline = new QCheckBox(tr("Underline")); m_underline->setChecked(f.underline());
    auto *row = new QHBoxLayout; row->addWidget(m_family, 1); row->addWidget(m_size); row->addWidget(m_bold); row->addWidget(m_italic); row->addWidget(m_underline);
    auto *bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(bb, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(bb, &QDialogButtonBox::rejected, this, &QDialog::reject);
    auto *lay = new QVBoxLayout(this); lay->addLayout(row); lay->addWidget(m_text); lay->addWidget(bb);
    m_text->setFocus();
}
QString TextDialog::text() const { return m_text->toPlainText(); }
QFont TextDialog::font() const {
    QFont f = m_family->currentFont();
    f.setPointSize(m_size->value()); f.setBold(m_bold->isChecked()); f.setItalic(m_italic->isChecked()); f.setUnderline(m_underline->isChecked());
    return f;
}
