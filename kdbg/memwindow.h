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
    QList<int> m_memoryColumnsWidth;
    int m_memoryRowHeight = 0;

    QBoxLayout m_layout;

    bool m_dumpMemRegionEnd = false;
    DbgAddr m_dumpLastAddr;
    unsigned m_dumpLength = 0;
    unsigned m_format;
    QMap<QString,unsigned> m_formatCache;

    QMenu m_popup;

    void contextMenuEvent(QContextMenuEvent* ev) override;
    void displayNewExpression(const QString& expr);
    void requestMemoryDump(const QString &expr);
    QString parseMemoryDumpLineToAscii(const QString& line, bool littleendian);

public slots:
    void verticalScrollBarMoved(int);
    void verticalScrollBarRangeChanged(int, int);
    void slotNewExpression(const QString&);
    void slotNewExpression();
    void slotTypeChange(QAction*);
    void slotNewMemoryDump(const QString&, const std::list<MemoryDump>&);
    void saveProgramSpecific(KConfigBase* config);
    void restoreProgramSpecific(KConfigBase* config);
};

#endif // MEMWINDOW_H
