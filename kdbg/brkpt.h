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

struct Breakpoint
{
    int id;				/* gdb's number */
    bool temporary;
    bool enabled;
    QString location;
    QString conditionInput;		/* condition as input by the user */
    QString condition;			/* condition as printed by gdb */
    uint ignoreCount;			/* ignore next that may hits */
    int hitCount;			/* as reported by gdb */
    // the following items repeat the location, but in a better usable way
    QString fileName;
    int lineNo;				/* zero-based line number */

    bool del;				/* used when list is parsed */
};


/*
 * The class BreakpointListBox provides a simpler interface to the
 * KTabListBox.
 */
class BreakpointListBox : public KTabListBox
{
public:
    BreakpointListBox(QWidget* parent, const char* name);
    ~BreakpointListBox();

    void appendItem(Breakpoint* bp);
    void changeItem(int id, Breakpoint* bp);

protected:
    static QString constructListText(Breakpoint* bp);
    virtual void keyPressEvent(QKeyEvent* e);
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
    void doBreakpoint(QString file, int lineNo, bool temporary);
    void doEnableDisableBreakpoint(const QString& file, int lineNo);
    Breakpoint* breakpointByFilePos(QString file, int lineNo);
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
    QArray<Breakpoint*> m_brkpts;

    void parseBreakList(const char* output);
    void insertBreakpoint(int num, char disp, char enable, const char* location,
			  const char* fileName = 0, int lineNo = -1,
			  int hits = 0, uint ignoreCount = 0,
			  QString condition = QString());
    void insertBreakpoint(int num, const QString& fileName, int lineNo);
    int breakpointById(int id);		/* returns index into m_brkpts */
    void updateBreakpointCondition(int id, const QString& condition,
				   uint ignoreCount);

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
};

#endif // BRKPT_H
