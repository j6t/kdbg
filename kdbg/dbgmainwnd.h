// $Id$

// Copyright by Johannes Sixt
// This file is under GPL, the GNU General Public Licence

#ifndef DBGMAINWND_H
#define DBGMAINWND_H

#include <qlistbox.h>
#include <knewpanner.h>
#include "mainwndbase.h"
#include "winstack.h"


class DebuggerMainWnd : public DebuggerMainWndBase
{
    Q_OBJECT
public:
    DebuggerMainWnd(const char* name);
    ~DebuggerMainWnd();

protected:
    // session properties
    virtual void saveProperties(KConfig*);
    virtual void readProperties(KConfig*);
    // settings
    void saveSettings(KConfig*);
    void restoreSettings(KConfig*);

    // statusbar texts
    void updateLineStatus(int lineNo);	/* zero-based line number */

    void initMenu();
    void initToolbar();

    // view windows
    KNewPanner m_mainPanner;
    KNewPanner m_leftPanner;
    KNewPanner m_rightPanner;
    WinStack m_filesWindow;
    QListBox m_btWindow;
    ExprWnd m_localVariables;
    WatchWindow m_watches;

    // menus
    QPopupMenu m_menuFile;
    QPopupMenu m_menuView;
    QPopupMenu m_menuProgram;
    QPopupMenu m_menuBrkpt;
    QPopupMenu m_menuWindow;

protected:
    virtual void closeEvent(QCloseEvent* e);

public slots:
    virtual void menuCallback(int item);
    virtual void updateUIItem(UpdateUI* item);
    virtual void updateUI();
    virtual void updateLineItems();
    void slotFileChanged();
    void slotLineChanged();
    void slotAddWatch();
    void slotNewFileLoaded();
};

#endif // DBGMAINWND_H
