// $Id$

// Copyright by Johannes Sixt
// This file is under GPL, the GNU General Public Licence

#ifndef DBGMAINWND_H
#define DBGMAINWND_H

#include <qlistbox.h>
#if QT_VERSION < 200
#include <knewpanner.h>
#else
#include <qsplitter.h>
#endif
#include <ktmainwindow.h>
#include "mainwndbase.h"
#include "winstack.h"
#include "brkpt.h"
#include "regwnd.h"


class DebuggerMainWnd : public KTMainWindow, public DebuggerMainWndBase
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
#if QT_VERSION < 200
    KNewPanner m_mainPanner;
    KNewPanner m_leftPanner;
    KNewPanner m_rightPanner;
#else
    QSplitter m_mainPanner;
    QSplitter m_leftPanner;
    QSplitter m_rightPanner;
#endif
    WinStack m_filesWindow;
    QListBox m_btWindow;
    ExprWnd m_localVariables;
    WatchWindow m_watches;
    BreakpointTable m_bpTable;
    RegisterView m_registers;

    // menus
    QPopupMenu m_menuFile;
    QPopupMenu m_menuView;
    QPopupMenu m_menuProgram;
    QPopupMenu m_menuBrkpt;
    QPopupMenu m_menuWindow;

protected:
    virtual void closeEvent(QCloseEvent* e);
    virtual KToolBar* dbgToolBar();
    virtual KStatusBar* dbgStatusBar();
    virtual QWidget* dbgMainWnd();

signals:
    void forwardMenuCallback(int item);

public slots:
    virtual void menuCallback(int item);
    void updateUIItem(UpdateUI* item);
    virtual void updateUI();
    virtual void updateLineItems();
    void slotFileChanged();
    void slotLineChanged();
    void slotAddWatch();
    void slotNewFileLoaded();
    void slotNewStatusMsg();
    void slotAnimationTimeout();
    void slotGlobalOptions();
    void slotDebuggerStarting();
    void slotToggleBreak(const QString&, int);
    void slotEnaDisBreak(const QString&, int);
};

#endif // DBGMAINWND_H
