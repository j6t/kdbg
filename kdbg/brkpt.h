// $Id$

// Copyright by Johannes Sixt
// This file is under GPL, the GNU General Public Licence

// the list of breakpoints

#ifndef BRKPT_H
#define BRKPT_H

#include <qlistview.h>
#include <qlayout.h>
#include <qpushbutton.h>
#include <qlineedit.h>
#include "valarray.h"

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
    QPushButton m_btAdd;
    QPushButton m_btRemove;
    QPushButton m_btViewCode;
    QPushButton m_btConditional;
    QHBoxLayout m_layout;
    QVBoxLayout m_listandedit;
    QVBoxLayout m_buttons;
    ValArray<QPixmap> m_icons;

    void insertBreakpoint(int num, bool temp, bool enabled, QString location,
			  QString fileName = 0, int lineNo = -1,
			  int hits = 0, uint ignoreCount = 0,
			  QString condition = QString());
    BreakpointItem* itemByBreakId(int id);
    void initListAndIcons();
    void updateBreakpointCondition(int id, const QString& condition,
				   int ignoreCount);

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
     */
    void activateFileLine(const QString& file, int lineNo);
public slots:
    virtual void addBP();
    virtual void removeBP();
    virtual void viewBP();
    virtual void conditionalBP();
    void updateUI();
    void updateBreakList();
};

#endif // BRKPT_H
