// $Id$

// Copyright by Johannes Sixt
// This file is under GPL, the GNU General Public Licence

#ifndef MEMWINDOW_H
#define MEMWINDOW_H

#include <qpopupmenu.h>
#if QT_VERSION >= 200
#include <qmultilineedit.h>
#else
#include <qmultilinedit.h>
#endif
#include <qcombobox.h>
#include <qlayout.h>
#include <qdict.h>

class KDebugger;
class KSimpleConfig;

class MemoryWindow : public QWidget
{
    Q_OBJECT
public:
    MemoryWindow(QWidget* parent, const char* name);
    ~MemoryWindow();

    void setDebugger(KDebugger* deb) { m_debugger = deb; }

protected:
    KDebugger* m_debugger;
    QComboBox m_expression;
    QMultiLineEdit m_memory;
    QVBoxLayout m_layout;

    unsigned m_format;
    QDict<unsigned> m_formatCache;

    QPopupMenu m_popup;

    virtual void paletteChange(const QPalette& oldPal);
    virtual void mousePressEvent(QMouseEvent* ev);
    virtual bool eventFilter(QObject* o, QEvent* ev);

    void handlePopup(QMouseEvent* ev);
    void displayNewExpression(const QString& expr);

public slots:
    void slotNewExpression(const char*);
    void slotNewExpression(const QString&);
    void slotTypeChange(int id);
    void slotNewMemoryDump(const QString&);
    void saveProgramSpecific(KSimpleConfig* config);
    void restoreProgramSpecific(KSimpleConfig* config);
};

#endif // MEMWINDOW_H
