// $Id$

// Copyright by Johannes Sixt
// This file is under GPL, the GNU General Public Licence

#ifndef DBGMAINWND_H
#define DBGMAINWND_H

#include <qtimer.h>
#include "dockmainwindow.h"
#include "mainwndbase.h"
#include "regwnd.h"

class WinStack;
class QPopupMenu;
class QListBox;
class ExprWnd;
class DockWidget;
class BreakpointTable;
class ThreadList;
class MemoryWindow;
struct DbgAddr;

class DebuggerMainWnd : public DockMainWindow, public DebuggerMainWndBase
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
    WinStack* m_filesWindow;
    QListBox* m_btWindow;
    ExprWnd* m_localVariables;
    WatchWindow* m_watches;
    RegisterView* m_registers;
    BreakpointTable* m_bpTable;
    TTYWindow* m_ttyWindow;
    ThreadList* m_threads;
    MemoryWindow* m_memoryWindow;

    // menus
    QPopupMenu* m_menuFile;
    QPopupMenu* m_menuView;
    QPopupMenu* m_menuProgram;
    QPopupMenu* m_menuBrkpt;
    QPopupMenu* m_menuWindow;

    QPopupMenu* m_menuRecentExecutables;

    QTimer m_backTimer;

protected:
    virtual void closeEvent(QCloseEvent* e);
    virtual KToolBar* dbgToolBar();
    virtual KStatusBar* dbgStatusBar();
    virtual QWidget* dbgMainWnd();
    virtual TTYWindow* ttyWindow();
    virtual QString createOutputWindow();
    virtual void doGlobalOptions();

    DockWidget* dockParent(QWidget* w);
    bool isDockVisible(QWidget* w);
    bool canChangeDockVisibility(QWidget* w);
    void showhideWindow(QWidget* w);
    void dockUpdateHelper(UpdateUI* item, QWidget* w);
    void intoBackground();

    QString makeSourceFilter();
    void fillRecentExecMenu();

    // to avoid flicker when the status bar is updated,
    // we store the last string that we put there
    QString m_lastActiveStatusText;

signals:
    void forwardMenuCallback(int item);
    void setTabWidth(int tabWidth);

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
    void slotDebuggerStarting();
    void slotToggleBreak(const QString&, int, const DbgAddr&, bool);
    void slotEnaDisBreak(const QString&, int, const DbgAddr&);
    void slotTermEmuExited();
    void slotProgramStopped();
    void slotBackTimer();
    void slotRecentExec(int item);
};

#endif // DBGMAINWND_H
