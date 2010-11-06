/*
 * Copyright Johannes Sixt
 * This file is licensed under the GNU General Public License Version 2.
 * See the file COPYING in the toplevel directory of the source directory.
 */

#ifndef MEMWINDOW_H
#define MEMWINDOW_H

#include <QBoxLayout>
#include <QComboBox>
#include <QMap>
#include <QMenu>
#include <QTreeWidget>
#include "dbgdriver.h"

class KDebugger;
class KConfigBase;

class MemoryWindow : public QWidget
{
    Q_OBJECT
public:
    MemoryWindow(QWidget* parent);
    ~MemoryWindow();

    void setDebugger(KDebugger* deb) { m_debugger = deb; }

protected:
    KDebugger* m_debugger;
    QComboBox m_expression;

    QTreeWidget m_memory;
    QMap<QString,QString> m_old_memory;

    QBoxLayout m_layout;

    unsigned m_format;
    QMap<QString,unsigned> m_formatCache;

    QMenu m_popup;

    virtual void contextMenuEvent(QContextMenuEvent* ev);
    void displayNewExpression(const QString& expr);

public slots:
    void slotNewExpression(const QString&);
    void slotNewExpression();
    void slotTypeChange(QAction*);
    void slotNewMemoryDump(const QString&, const std::list<MemoryDump>&);
    void saveProgramSpecific(KConfigBase* config);
    void restoreProgramSpecific(KConfigBase* config);
};

#endif // MEMWINDOW_H
