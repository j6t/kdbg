// $Id$

// Copyright by Johannes Sixt
// This file is under GPL, the GNU General Public Licence

#ifndef MEMWINDOW_H
#define MEMWINDOW_H

#if QT_VERSION >= 200
#include <qmultilineedit.h>
#else
#include <qmultilinedit.h>
#endif
#include <qlineedit.h>
#include <qlayout.h>
#include <qpopupmenu.h>

class KDebugger;

class MemoryWindow : public QWidget
{
    Q_OBJECT
public:
    MemoryWindow(QWidget* parent, const char* name);
    ~MemoryWindow();

    void setDebugger(KDebugger* deb) { m_debugger = deb; }

protected:
    KDebugger* m_debugger;
    QLineEdit m_expression;
    QMultiLineEdit m_memory;
    QVBoxLayout m_layout;

    unsigned m_format;

    QPopupMenu m_popup;

    virtual void paletteChange(const QPalette& oldPal);
    virtual void mousePressEvent(QMouseEvent* ev);
    virtual bool eventFilter(QObject* o, QEvent* ev);

    void handlePopup(QMouseEvent* ev);

public slots:
    void slotNewExpression();
    void slotTypeChange(int id);
    void slotNewMemoryDump(const QString&);
};

#endif // MEMWINDOW_H
