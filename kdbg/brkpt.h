// $Id$

// Copyright by Johannes Sixt
// This file is under GPL, the GNU General Public Licence

// the list of breakpoints

#ifndef BRKPT_H
#define BRKPT_H

#include <qdialog.h>
#include <qlistview.h>
#include <qlayout.h>
#include <qpushbutton.h>
#include <qlineedit.h>
#include "valarray.h"

class KSimpleConfig;

/*
 * This widget is so closely related to the KDebugger main widget that it
 * in fact is part of it. However, since it's technically easily possible
 * to keep this part of the debugger in it's own class, we do so. But now
 * we need a backpointer to the debugger (which means that debugger.cpp and
 * brkpt.cpp #include each other's header - mutal and recursive #includes!)
 * and we must be a friend of KDebugger.
 */
class KDebugger;

// todo: a forward should be sufficient
struct WndBreakpoint;
#include "dbgdriver.h"
struct WndBreakpoint : Breakpoint
{
    QString conditionInput;		/* condition as input by the user */

    bool del;				/* used when list is parsed */
    QListViewItem* listItem;		/* the corresponding table item */
};



/*
 * The class BreakpointListBox provides a customized interface to the
 * QListView.
 */
class BreakpointListBox : public QListView
{
public:
    BreakpointListBox(QWidget* parent, const char* name);
    ~BreakpointListBox();

    void changeItem(WndBreakpoint* bp);

protected:
    ValArray<QPixmap> m_icons;
};


class BreakpointTable : public QDialog
{
    Q_OBJECT
public:
    BreakpointTable(KDebugger& deb);
    ~BreakpointTable();

    void insertBreakpoint(int num, const QString& fileName, int lineNo);
    void updateBreakList(const char* output);
    int numBreakpoints() const { return m_brkpts.size(); }
    const WndBreakpoint* breakpoint(int i) const { return m_brkpts[i]; }
    void doBreakpoint(QString file, int lineNo, bool temporary);
    void doEnableDisableBreakpoint(const QString& file, int lineNo);
    WndBreakpoint* breakpointByFilePos(QString file, int lineNo);
    bool haveTemporaryBP() const;

    void saveBreakpoints(KSimpleConfig* config);
    void restoreBreakpoints(KSimpleConfig* config);

protected:
    KDebugger& m_debugger;
    QLineEdit m_bpEdit;
    BreakpointListBox m_list;
    QPushButton m_btAdd;
    QPushButton m_btRemove;
    QPushButton m_btViewCode;
    QPushButton m_btConditional;
    QPushButton m_btClose;
    QHBoxLayout m_layout;
    QVBoxLayout m_listandedit;
    QVBoxLayout m_buttons;

    // table of breakpoints; must correspond to items in list box
    QArray<WndBreakpoint*> m_brkpts;

    void insertBreakpoint(int num, bool temp, bool enabled, QString location,
			  QString fileName = 0, int lineNo = -1,
			  int hits = 0, uint ignoreCount = 0,
			  QString condition = QString());
    WndBreakpoint* breakpointById(int id);
    WndBreakpoint* breakpointByItem(QListViewItem* item);
    void updateBreakpointCondition(int id, const QString& condition,
				   int ignoreCount);

    void closeEvent(QCloseEvent*);
    
signals:
    /**
     * This signal is emitted when the user wants to go to the source code
     * where the current breakpoint is in.
     * 
     * @param file specifies the file; this is not necessarily a full path
     * name, and if it is relative, you won't know relative to what, you
     * can only guess.
     * @param lineNo specifies the line number (0-based!).
     */
    void activateFileLine(const QString& file, int lineNo);
    void closed();
public slots:
    virtual void hide();
    virtual void addBP();
    virtual void removeBP();
    virtual void viewBP();
    virtual void conditionalBP();
    void updateUI();
};

#endif // BRKPT_H
