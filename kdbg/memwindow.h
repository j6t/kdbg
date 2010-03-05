/*
 * Copyright Johannes Sixt
 * This file is licensed under the GNU General Public License Version 2.
 * See the file COPYING in the toplevel directory of the source directory.
 */

#ifndef MEMWINDOW_H
#define MEMWINDOW_H

#include <Q3PopupMenu>
#include <Q3ListView>
#include <QComboBox>
#include <QMap>
#include <Q3VBoxLayout>
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

    Q3ListView m_memory;
    QMap<QString,QString> m_old_memory;

    Q3VBoxLayout m_layout;

    unsigned m_format;
    QMap<QString,unsigned> m_formatCache;

    Q3PopupMenu m_popup;

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
