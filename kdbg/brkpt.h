/*
 * Copyright Johannes Sixt
 * This file is licensed under the GNU General Public License Version 2.
 * See the file COPYING in the toplevel directory of the source directory.
 */

#ifndef BRKPT_H
#define BRKPT_H

#include <QIcon>
#include <QEvent>
#include <vector>

#include "ui_brkptbase.h"

class KDebugger;
class BreakpointItem;


class BreakpointTable : public QWidget
{
    Q_OBJECT
public:
    BreakpointTable(QWidget* parent);
    ~BreakpointTable();
    void setDebugger(KDebugger* deb) { m_debugger = deb; }

protected:
    KDebugger* m_debugger;
    Ui::BrkPtBase m_ui;
    std::vector<QIcon> m_icons;

    void insertBreakpoint(int num, bool temp, bool enabled, QString location,
			  QString fileName = 0, int lineNo = -1,
			  int hits = 0, uint ignoreCount = 0,
			  QString condition = QString());
    void initListAndIcons();
    virtual bool eventFilter(QObject* ob, QEvent* ev);

    friend class BreakpointItem;
    
signals:
    /**
     * This signal is emitted when the user wants to go to the source code
     * where the current breakpoint is in.
     * 
     * @param file specifies the file; this is not necessarily a full path
     * name, and if it is relative, you won't know relative to what, you
     * can only guess.
     * @param lineNo specifies the line number (0-based!).
     * @param address specifies the exact address of the breakpoint.
     */
    void activateFileLine(const QString& file, int lineNo, const DbgAddr& address);
public slots:
    void on_btAddBP_clicked();
    void on_btAddWP_clicked();
    void on_btRemove_clicked();
    void on_btEnaDis_clicked();
    void on_btViewCode_clicked();
    void on_btConditional_clicked();
    void updateUI();
    void updateBreakList();
};

#endif // BRKPT_H
