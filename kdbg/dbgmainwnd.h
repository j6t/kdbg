// $Id$

// Copyright by Johannes Sixt
// This file is under GPL, the GNU General Public Licence

#ifndef DBGMAINWND_H
#define DBGMAINWND_H

#include "dockmainwindow.h"
#include "mainwndbase.h"
#include "regwnd.h"

class WinStack;
class QPopupMenu;
class QListBox;
class ExprWnd;
class DockWidget;
class BreakpointTable;

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

    // menus
    QPopupMenu* m_menuFile;
    QPopupMenu* m_menuView;
    QPopupMenu* m_menuProgram;
    QPopupMenu* m_menuBrkpt;
    QPopupMenu* m_menuWindow;

protected:
    virtual void closeEvent(QCloseEvent* e);
    virtual KToolBar* dbgToolBar();
    virtual KStatusBar* dbgStatusBar();
    virtual QWidget* dbgMainWnd();
    virtual TTYWindow* ttyWindow();
    virtual bool createOutputWindow();

    DockWidget* dockParent(QWidget* w);
    bool isDockVisible(QWidget* w);
    bool canChangeDockVisibility(QWidget* w);
    void showhideWindow(QWidget* w);
    void dockUpdateHelper(UpdateUI* item, QWidget* w);

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
    void slotTermEmuExited();
};

#endif // DBGMAINWND_H
