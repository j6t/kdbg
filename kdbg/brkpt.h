// $Id$

// Copyright by Johannes Sixt
// This file is under GPL, the GNU General Public Licence

// the list of breakpoints

#ifndef BRKPT_H
#define BRKPT_H

#include <qdialog.h>
#include <ktablistbox.h>
#include <qlayout.h>
#include <qpushbt.h>
#include <qlined.h>

/*
 * This widget is so closely related to the KDebugger main widget that it
 * in fact is part of it. However, since it's technically easily possible
 * to keep this part of the debugger in it's own class, we do so. But now
 * we need a backpointer to the debugger (which means that debugger.cpp and
 * brkpt.cpp #include each other's header - mutal and recursive #includes!)
 * and we must be a friend of KDebugger.
 */
class KDebugger;

struct Breakpoint
{
    int num;				/* gdb's number */
    bool temporary;
    bool enabled;
    QString location;
    // the following items repeat the location, but in a better usable way
    QString fileName;
    int lineNo;

    bool del;				/* used when list is parsed */
};

class BreakpointTable : public QDialog
{
    Q_OBJECT
public:
    BreakpointTable(KDebugger& deb);
    ~BreakpointTable();

    void updateBreakList(const char* output);
    void parseBreakpoint(const char* output);
    int numBreakpoints() const { return m_brkpts.size(); }
    const Breakpoint& breakpoint(int i) const { return *m_brkpts[i]; }
    
protected:
    KDebugger& m_debugger;
    QLineEdit m_bpEdit;
    KTabListBox m_list;
    QPushButton m_btAdd;
    QPushButton m_btRemove;
    QPushButton m_btViewCode;
    QPushButton m_btClose;
    QHBoxLayout m_layout;
    QVBoxLayout m_listandedit;
    QVBoxLayout m_buttons;

    // table of breakpoints; must correspond to items in list box
    QArray<Breakpoint*> m_brkpts;

    void parseBreakList(const char* output);
    void insertBreakpoint(int num, char disp, char enable, const char* location,
			  const char* fileName = 0, int lineNo = -1);
    void insertBreakpoint(int num, const QString& fileName, int lineNo);

    void closeEvent(QCloseEvent*);
    
signals:
    void closed();
public slots:
    virtual void hide();
    virtual void addBP();
};

#endif // BRKPT_H