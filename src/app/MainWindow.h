#pragma once
#include "drawing/DrawingTool.h"
#include <QMainWindow>
#include <QHash>

class Editor; class CanvasView; class ColorPalette; class QAction; class QLabel;
class QComboBox; class QSpinBox; class QCheckBox; class QScrollArea;

// Pixelle main window.
// NOTE: menus, tool bar and option bar are a FUNCTIONAL placeholder layout. They are
// deliberately NOT a reproduction of the Windows 11 Paint ribbon: no reference
// screenshots/assets exist yet (see docs/MISSING_REFERENCES.md).
class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    Editor *editor() const { return m_editor; }
    CanvasView *canvas() const { return m_canvas; }
    QAction *action(const QString &objectName) const { return m_actions.value(objectName); }

    bool openFile(const QString &path);
    bool saveFile(const QString &path, bool overwriteConfirmed);
    bool maybeSave();                       // false = user cancelled
    static QString appDisplayName() { return QStringLiteral("Pixelle"); }

protected:
    void closeEvent(QCloseEvent *) override;

private:
    QAction *addAction(QMenu *menu, const QString &text, const QString &objName,
                       const QKeySequence &key, std::function<void()> fn);
    void buildMenus(); void buildToolBars(); void buildStatusBar();
    void updateTitle(); void updateActions(); void syncOptionWidgets(); void updateStatusSelection();
    bool newDialog(); bool openDialog(); bool saveAsDialog(); bool saveCurrent();
    void promptText(const QPoint &imagePos);

    Editor *m_editor; CanvasView *m_canvas; QScrollArea *m_scroll; ColorPalette *m_palette;
    QHash<QString, QAction *> m_actions;
    QHash<int, QAction *> m_toolActions;
    QComboBox *m_brushBox, *m_shapeBox, *m_fillBox; QSpinBox *m_sizeSpin, *m_tolSpin; QCheckBox *m_aaCheck, *m_eraseTransparent;
    QLabel *m_posLabel, *m_selLabel, *m_sizeLabel, *m_zoomLabel;
    QString m_lastDir;
};
