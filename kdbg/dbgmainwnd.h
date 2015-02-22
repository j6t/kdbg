/*
 * Copyright Johannes Sixt
 * This file is licensed under the GNU General Public License Version 2.
 * See the file COPYING in the toplevel directory of the source directory.
 */

#ifndef DBGMAINWND_H
#define DBGMAINWND_H

#include <QPointer>
#include <QTimer>
#include <kxmlguiwindow.h>
#include "regwnd.h"

class QDockWidget;
class QProcess;
class KAnimatedButton;
class KRecentFilesAction;
class KUrl;
class WinStack;
class QListWidget;
class ExprWnd;
class BreakpointTable;
class ThreadList;
class MemoryWindow;
class TTYWindow;
class WatchWindow;
class KDebugger;
class DebuggerDriver;
struct DbgAddr;

class DebuggerMainWnd : public KXmlGuiWindow
{
    Q_OBJECT
public:
    DebuggerMainWnd();
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
    virtual void saveProperties(KConfigGroup& cg);
    virtual void readProperties(const KConfigGroup& cg);
    // settings
    void saveSettings(KSharedConfigPtr);
    void restoreSettings(KSharedConfigPtr);

    void initAnimation();
    void initStatusBar();
    void initKAction();

    // view windows
    WinStack* m_filesWindow;
    QListWidget* m_btWindow;
    ExprWnd* m_localVariables;
    WatchWindow* m_watches;
    RegisterView* m_registers;
    BreakpointTable* m_bpTable;
    TTYWindow* m_ttyWindow;
    ThreadList* m_threads;
    MemoryWindow* m_memoryWindow;

    QTimer m_backTimer;

    // recent execs in File menu
    QAction* m_closeAction;
    QAction* m_reloadAction;
    QAction* m_fileExecAction;
    KRecentFilesAction* m_recentExecAction;
    QAction* m_coreDumpAction;
    QAction* m_settingsAction;
    QAction* m_findAction;
    QAction* m_btWindowAction;
    QAction* m_localVariablesAction;
    QAction* m_watchesAction;
    QAction* m_registersAction;
    QAction* m_bpTableAction;
    QAction* m_ttyWindowAction;
    QAction* m_threadsAction;
    QAction* m_memoryWindowAction;
    QAction* m_runAction;
    QAction* m_stepIntoAction;
    QAction* m_stepOverAction;
    QAction* m_stepOutAction;
    QAction* m_toCursorAction;
    QAction* m_stepIntoIAction;
    QAction* m_stepOverIAction;
    QAction* m_execMovePCAction;
    QAction* m_breakAction;
    QAction* m_killAction;
    QAction* m_restartAction;
    QAction* m_attachAction;
    QAction* m_argumentsAction;
    QAction* m_bpSetAction;
    QAction* m_bpSetTempAction;
    QAction* m_bpEnableAction;
    QAction* m_editValueAction;
    QString m_lastDirectory;		/* the dir of the most recently opened file */

protected:
    virtual bool queryClose();
    QAction* createAction(const QString& text, const char* icon,
			int shortcut, const QObject* receiver,
			const char* slot, const char* name);
    QAction* createAction(const QString& text,
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
    QProcess* m_outputTermProc;
    int m_ttyLevel;

    QString createOutputWindow();

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
    void makeDefaultLayout();

    QString makeSourceFilter();

    // to avoid flicker when the status bar is updated,
    // we store the last string that we put there
    QPointer<KAnimatedButton> m_animation;
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
    void slotProgramStopped();
    void slotBackTimer();
    void slotRecentExec(const KUrl& url);
    void slotLocalsPopup(const QPoint& pt);
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
