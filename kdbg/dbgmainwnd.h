/*
 * Copyright Johannes Sixt
 * This file is licensed under the GNU General Public License Version 2.
 * See the file COPYING in the toplevel directory of the source directory.
 */

#ifndef DBGMAINWND_H
#define DBGMAINWND_H

#include <qtimer.h>
#include <q3dockwindow.h>
#include <Q3CString>
#include <kmainwindow.h>
#include "regwnd.h"

typedef Q3DockWindow QDockWidget;
class K3Process;
class KRecentFilesAction;
class KToggleAction;
class WinStack;
class Q3ListBox;
class Q3CString;
class ExprWnd;
class BreakpointTable;
class ThreadList;
class MemoryWindow;
class TTYWindow;
class WatchWindow;
class KDebugger;
class DebuggerDriver;
struct DbgAddr;

class DebuggerMainWnd : public KMainWindow
{
    Q_OBJECT
public:
    DebuggerMainWnd(const char* name);
    ~DebuggerMainWnd();

    bool debugProgram(const QString& exe, const QString& lang);

    /**
     * Specifies the file where to write the transcript.
     */
    void setTranscript(const QString& name);
    /**
     * Specifies the process to attach to after the program is loaded.
     */
    void setAttachPid(const QString& pid);

    // the following are needed to handle program arguments
    void setCoreFile(const QString& corefile);
    void setRemoteDevice(const QString &remoteDevice);
    void overrideProgramArguments(const QString& args);

protected:
    // session properties
    virtual void saveProperties(KConfig*);
    virtual void readProperties(KConfig*);
    // settings
    void saveSettings(KConfig*);
    void restoreSettings(KConfig*);

    void initToolbar();
    void initKAction();

    // view windows
    WinStack* m_filesWindow;
    Q3ListBox* m_btWindow;
    ExprWnd* m_localVariables;
    WatchWindow* m_watches;
    RegisterView* m_registers;
    BreakpointTable* m_bpTable;
    TTYWindow* m_ttyWindow;
    ThreadList* m_threads;
    MemoryWindow* m_memoryWindow;

    QTimer m_backTimer;

    // recent execs in File menu
    KAction* m_closeAction;
    KAction* m_reloadAction;
    KAction* m_fileExecAction;
    KRecentFilesAction* m_recentExecAction;
    KAction* m_coreDumpAction;
    KAction* m_settingsAction;
    KToggleAction* m_findAction;
    KToggleAction* m_btWindowAction;
    KToggleAction* m_localVariablesAction;
    KToggleAction* m_watchesAction;
    KToggleAction* m_registersAction;
    KToggleAction* m_bpTableAction;
    KToggleAction* m_ttyWindowAction;
    KToggleAction* m_threadsAction;
    KToggleAction* m_memoryWindowAction;
    KAction* m_runAction;
    KAction* m_stepIntoAction;
    KAction* m_stepOverAction;
    KAction* m_stepOutAction;
    KAction* m_toCursorAction;
    KAction* m_stepIntoIAction;
    KAction* m_stepOverIAction;
    KAction* m_execMovePCAction;
    KAction* m_breakAction;
    KAction* m_killAction;
    KAction* m_restartAction;
    KAction* m_attachAction;
    KAction* m_argumentsAction;
    KAction* m_bpSetAction;
    KAction* m_bpSetTempAction;
    KAction* m_bpEnableAction;
    KAction* m_editValueAction;
    QString m_lastDirectory;		/* the dir of the most recently opened file */

protected:
    virtual bool queryClose();
    KAction* createAction(const QString& text, const char* icon,
			int shortcut, const QObject* receiver,
			const char* slot, const char* name);
    KAction* createAction(const QString& text,
			int shortcut, const QObject* receiver,
			const char* slot, const char* name);

    // the debugger proper
    QString m_debuggerCmdStr;
    KDebugger* m_debugger;
    QString m_transcriptFile;		/* where gdb dialog is logged */

    /**
     * Starts to debug the specified program using the specified language
     * driver.
     */
    bool startDriver(const QString& executable, QString lang);
    DebuggerDriver* driverFromLang(QString lang);
    /**
     * Derives a driver name from the contents of the named file.
     */
    QString driverNameFromFile(const QString& exe);

    // output window
    QString m_outputTermCmdStr;
    QString m_outputTermKeepScript;
    K3Process* m_outputTermProc;
    int m_ttyLevel;

    QString createOutputWindow();
    void shutdownTermWindow();

    bool m_popForeground;		/* whether main wnd raises when prog stops */
    int m_backTimeout;			/* when wnd goes back */
    int m_tabWidth;			/* tab width in characters (can be 0) */
    QString m_sourceFilter;
    QString m_headerFilter;
    void setTerminalCmd(const QString& cmd);
    void setDebuggerCmdStr(const QString& cmd);

    QDockWidget* createDockWidget(const char* name, const QString& title);
    QDockWidget* dockParent(QWidget* w);
    bool isDockVisible(QWidget* w);

    QString makeSourceFilter();

    // to avoid flicker when the status bar is updated,
    // we store the last string that we put there
    QString m_lastActiveStatusText;
    bool m_animRunning;

    // statusbar texts
    QString m_statusActive;

signals:
    void setTabWidth(int tabWidth);

public slots:
    virtual void updateUI();
    virtual void updateLineItems();
    void slotAddWatch();
    void slotAddWatch(const QString& text);
    void slotNewFileLoaded();
    void slotNewStatusMsg();
    void slotDebuggerStarting();
    void slotToggleBreak(const QString&, int, const DbgAddr&, bool);
    void slotEnaDisBreak(const QString&, int, const DbgAddr&);
    void slotTermEmuExited();
    void slotProgramStopped();
    void slotBackTimer();
    void slotRecentExec(const KURL& url);
    void slotLocalsPopup(Q3ListViewItem*, const QPoint& pt);
    void slotLocalsToWatch();
    void slotEditValue();

    void slotFileOpen();
    void slotFileExe();
    void slotFileCore();
    void slotFileGlobalSettings();
    void slotFileProgSettings();
    void slotViewStatusbar();
    void slotExecUntil();
    void slotExecAttach();
    void slotExecArgs();
    void intoBackground();
    void slotConfigureKeys();
};

#endif // DBGMAINWND_H
