/*
 * Copyright Johannes Sixt
 * This file is licensed under the GNU General Public License Version 2.
 * See the file COPYING in the toplevel directory of the source directory.
 */

#ifndef MEMWINDOW_H
#define MEMWINDOW_H

#include <qpopupmenu.h>
#include <qlistview.h>
#include <qcombobox.h>
#include <qlayout.h>
#include <qmap.h>
#include "dbgdriver.h"

class KDebugger;
class KConfigBase;

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

    QListView m_memory;
    QMap<QString,QString> m_old_memory;

    QVBoxLayout m_layout;

    unsigned m_format;
    QMap<QString,unsigned> m_formatCache;

    QPopupMenu m_popup;

    virtual bool eventFilter(QObject* o, QEvent* ev);
    void handlePopup(QMouseEvent* ev);
    void displayNewExpression(const QString& expr);

public slots:
    void slotNewExpression(const QString&);
    void slotTypeChange(int id);
    void slotNewMemoryDump(const QString&, const std::list<MemoryDump>&);
    void saveProgramSpecific(KConfigBase* config);
    void restoreProgramSpecific(KConfigBase* config);
};

#endif // MEMWINDOW_H
