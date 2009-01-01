/*
 * Copyright Johannes Sixt
 * This file is licensed under the GNU General Public License Version 2.
 * See the file COPYING in the toplevel directory of the source directory.
 */

#ifndef BRKPT_H
#define BRKPT_H

#include <qlistview.h>
#include <qlayout.h>
#include <qpushbutton.h>
#include <qlineedit.h>
#include <qvaluevector.h>

class KDebugger;
class BreakpointItem;


class BreakpointTable : public QWidget
{
    Q_OBJECT
public:
    BreakpointTable(QWidget* parent, const char* name);
    ~BreakpointTable();
    void setDebugger(KDebugger* deb) { m_debugger = deb; }

protected:
    KDebugger* m_debugger;
    QLineEdit m_bpEdit;
    QListView m_list;
    QPushButton m_btAddBP;
    QPushButton m_btAddWP;
    QPushButton m_btRemove;
    QPushButton m_btEnaDis;
    QPushButton m_btViewCode;
    QPushButton m_btConditional;
    QHBoxLayout m_layout;
    QVBoxLayout m_listandedit;
    QVBoxLayout m_buttons;
    QValueVector<QPixmap> m_icons;

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
    virtual void addBP();
    virtual void addWP();
    virtual void removeBP();
    virtual void enadisBP();
    virtual void viewBP();
    virtual void conditionalBP();
    void updateUI();
    void updateBreakList();
};

#endif // BRKPT_H
