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

    bool debugProgram(const QString& exe);

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
    void initFileWndMenus();
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

    QPopupMenu* m_popupLocals;

    // popup menus of the files window
    QPopupMenu* m_popupFilesEmpty;
    QPopupMenu* m_popupFiles;

    QTimer m_backTimer;

protected:
    virtual void closeEvent(QCloseEvent* e);
    virtual KToolBar* dbgToolBar();
    virtual KStatusBar* dbgStatusBar();
    virtual TTYWindow* ttyWindow();
    virtual QString createOutputWindow();

    DockWidget* dockParent(QWidget* w);
    bool isDockVisible(QWidget* w);
    bool canChangeDockVisibility(QWidget* w);
    void dockUpdateHelper(UpdateUI* item, QWidget* w);
    void intoBackground();

    QString makeSourceFilter();
    void fillRecentExecMenu();

    // to avoid flicker when the status bar is updated,
    // we store the last string that we put there
    QString m_lastActiveStatusText;

signals:
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
    void slotLocalsPopup(int item, const QPoint& pt);
    void slotLocalsToWatch();

    void slotFileOpen();
    void slotFileQuit();
    void slotFileExe();
    void slotFileCore();
    void slotFileGlobalSettings();
    void slotFileProgSettings();
    void slotViewToolbar();
    void slotViewStatusbar();
    void slotExecUntil();
    void slotExecAttach();
    void slotExecArgs();

    void slotFileWndMenu(const QPoint& pos);
    void slotFileWndEmptyMenu(const QPoint& pos);
};

#endif // DBGMAINWND_H
