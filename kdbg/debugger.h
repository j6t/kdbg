// $Id$

// Copyright by Johannes Sixt
// This file is under GPL, the GNU General Public Licence

#ifndef DEBUGGER_H
#define DEBUGGER_H

#include <qqueue.h>
#include <qtimer.h>
#include <qfile.h>
#include <kprocess.h>
#include "brkpt.h"
#include "envvar.h"

class ExprWnd;
class VarTree;
class KTreeViewItem;
class KConfig;
class QListBox;


class GdbProcess : public KProcess
{
public:
    GdbProcess();
protected:
    virtual int commSetupDoneC();
};

class KDebugger : public QObject
{
    Q_OBJECT
public:
    KDebugger(QWidget* parent,		/* will be used as the parent for dialogs */
	      ExprWnd* localVars,
	      ExprWnd* watchVars,
	      QListBox* backtrace
	      );
    ~KDebugger();
    
    /**
     * This function starts to debug the specified executable.
     * @return false if an error occurs, in particular if a program is
     * currently being debugged
     */
    bool debugProgram(const QString& executable);

    /**
     * Queries the user for an executable file and debugs it. If a program
     * is currently being debugged, it is terminated first.
     */
    void fileExecutable();

    /**
     * Queries the user for a core file and uses it to debug the active
     * program
     */
    void fileCoreFile();

    /**
     * Runs the program or continues it if it is stopped at a breakpoint.
     */
    void programRun();

    /**
     * Attaches to a process and debugs it.
     */
    void programAttach();

    /**
     * Restarts the debuggee.
     */
    void programRunAgain();

    /**
     * Performs a single-step, possibly stepping into a function call.
     */
    void programStep();

    /**
     * Performs a single-step, stepping over a function call.
     */
    void programNext();

    /**
     * Runs the program until it returns from the current function.
     */
    void programFinish();

    /**
     * Interrupts the program if it is currently running.
     */
    void programBreak();

    /**
     * Queries the user for program arguments.
     */
    void programArgs();

    /**
     * Shows the breakpoint list if it isn't currently visible or hides it
     * if it is.
     */
    void breakListToggleVisible();

    /**
     * Run the debuggee until the specified line in the specified file is
     * reached.
     * 
     * @return false if the command was not executed, e.g. because the
     * debuggee is running at the moment.
     */
    bool runUntil(const QString& fileName, int lineNo);

    /**
     * Set a breakpoint.
     * 
     * @return false if the command was not executed, e.g. because the
     * debuggee is running at the moment.
     */
    bool setBreakpoint(const QString& fileName, int lineNo, bool temporary);

    /**
     * Enable or disable a breakpoint at the specified location.
     * 
     * @return false if the command was not executed, e.g. because the
     * debuggee is running at the moment.
     */
    bool enableDisableBreakpoint(const QString& fileName, int lineNo);

    /**
     * Tells whether one of the single stepping commands can be invoked
     * (step, next, finish, until, also run).
     */
    bool canSingleStep();

    /**
     * Tells whether a breakpoints can be set, deleted, enabled, or disabled.
     */
    bool canChangeBreakpoints();

    /**
     * Tells whether the debuggee can be changed.
     */
    bool KDebugger::canChangeExecutable() { return isReady() && !m_programActive; }

    /**
     * Add a watch expression.
     */
    void addWatch(const QString& expr);

    /**
     * Retrieves the current status message.
     */
    const QString& statusMessage() const { return m_statusMessage; }
    /**
     * Is the debugger ready to receive another high-priority command?
     */
    bool isReady() const { return m_haveExecutable &&
	    /*(m_state == DSidle || m_state == DSrunningLow)*/
	    m_hipriCmdQueue.isEmpty(); }
    /**
     * Is the debuggee running (not just active)?
     */
    bool isProgramRunning() { return m_haveExecutable && m_programRunning; }

    /**
     * Do we have an executable set?
     */
    bool haveExecutable() { return m_haveExecutable; }

    /**
     * Is the debuggee active, i.e. was it started by the debugger?
     */
    bool isProgramActive() { return m_programActive; }

    /** Is the breakpoint table visible? */
    bool isBreakListVisible() { return m_bpTable.isVisible(); }

    /** The table of breakpoints. */
    BreakpointTable& breakList() { return m_bpTable; }

    const QString& executable() const { return m_executable; }

    void setCoreFile(const QString& corefile) { m_corefile = corefile; }

    // settings
    void saveSettings(KConfig*);
    void restoreSettings(KConfig*);

public:
    // debugger driver
    enum DbgCommand {
	DCinitialize,
	DCnoconfirm,
	DCtty,
	DCexecutable,
	DCcorefile,
	DCattach,
	DCinfolinemain,
	DCinfolocals,
	DCsetargs,
	DCsetenv,
	DCbt,
	DCrun,
	DCcont,
	DCstep,
	DCnext,
	DCfinish,
	DCuntil,
	DCbreak,
	DCdelete,
	DCenable,
	DCdisable,
	DCprint,
	DCprintStruct,
	DCframe,
	DCfindType,
	DCinfobreak,
	DCcondition,
	DCignore
    };
protected:
    pid_t m_outputTermPID;
    QString m_outputTermName;
    QString m_outputTermKeepScript;
    bool createOutputWindow();
    bool startGdb();
    void stopGdb();
    void writeCommand();
    
    enum DebuggerState {
	DSidle,				/* gdb waits for input */
	DSinterrupted,			/* a command was interrupted */
	DSrunningLow,			/* gdb is running a low-priority command */
	DSrunning,			/* gdb waits for program */
	DScommandSent,			/* command has been sent, we wait for wroteStdin signal */
	DScommandSentLow,		/* low-prioritycommand has been sent */
    };
    DebuggerState m_state;
    char* m_gdbOutput;			/* normal gdb output */
    int m_gdbOutputLen;			/* current accumulated output */
    int m_gdbOutputAlloc;		/* space available in m_gdbOutput */
    QQueue<QString> m_delayedOutput;	/* output colleced while we have receivedOutput */
					/* but before signal wroteStdin arrived */
    QList<VarTree> m_watchEvalExpr;	/* exprs to evaluate for watch windows */

public:
    /**
     * Gdb commands are placed in a queue. Only one command at a time is
     * sent down to gdb. All other commands in the queue are retained until
     * the sent command has been processed by gdb. Gdb tells us that it's
     * done with the command by sending the prompt. The output of gdb is
     * parsed at that time. Then, if more commands are in the queue, the
     * next one is sent to gdb.
     * 
     * The active command is kept separately from other pending commands.
     */
    struct CmdQueueItem
    {
	DbgCommand m_cmd;
	QString m_cmdString;
	// remember which expression when printing an expression
	VarTree* m_expr;
	ExprWnd* m_exprWnd;
	// whether command was emitted due to direct user request (only set when relevant)
	bool m_byUser;

	CmdQueueItem(DbgCommand cmd, const QString& str) :
		m_cmd(cmd),
		m_cmdString(str),
		m_expr(0),
		m_exprWnd(0),
		m_byUser(false)
	{ }
    };
    /**
     * Enqueues a high-priority command. High-priority commands are
     * executed before any low-priority commands. No user interaction is
     * possible as long as there is a high-priority command in the queue.
     */
    CmdQueueItem* executeCmd(DbgCommand cmd, QString cmdString, bool clearLow = false);
    enum QueueMode {
	QMnormal,			/* queues the command last */
	QMoverride,			/* removes an already queued command */
	QMoverrideMoreEqual		/* ditto, also puts the command first in the queue */
    };
    /**
     * Enqueues a low-priority command. Low-priority commands are executed
     * after any high-priority commands.
     */
    CmdQueueItem* queueCmd(DbgCommand cmd, QString cmdString, QueueMode mode);

protected:
    QQueue<CmdQueueItem> m_hipriCmdQueue;
    QList<CmdQueueItem> m_lopriCmdQueue;
    CmdQueueItem* m_activeCmd;		/* the cmd we are working on */
    void parse(CmdQueueItem* cmd);
    VarTree* parseExpr(const char* name, bool wantErrorValue = true);
    void handleRunCommands();
    void updateAllExprs();
    void updateBreakptTable();
    void updateProgEnvironment(const char* args, const QDict<EnvVar>& newVars);
    void parseLocals(QList<VarTree>& newVars);
    void handleLocals();
    bool handlePrint(const char* var, ExprWnd* wnd);
    bool handlePrint(CmdQueueItem* cmd);
    void handleBacktrace();
    void handleFrameChange();
    void handleFindType(CmdQueueItem* cmd);
    void handlePrintStruct(CmdQueueItem* cmd);
    void evalExpressions();
    void evalStructExpression(VarTree* var, ExprWnd* wnd, bool immediate);
    void exprExpandingHelper(ExprWnd* wnd, KTreeViewItem* item, bool& allow);
    void dereferencePointer(ExprWnd* wnd, VarTree* var, bool immediate);
    void determineType(ExprWnd* wnd, VarTree* var);
    void removeExpr(ExprWnd* wnd, VarTree* var);
    CmdQueueItem* loadCoreFile();

    bool m_haveExecutable;		/* has an executable been specified */
    bool m_programActive;		/* is the program active (possibly halting in a brkpt)? */
    bool m_programRunning;		/* is the program executing (not stopped)? */
    QString m_executable;
    QString m_corefile;
    QString m_attachedPid;		/* user input of attaching to pid */
    QString m_programArgs;
    QDict<EnvVar> m_envVars;		/* environment variables set by user */
    KSimpleConfig* m_programConfig;	/* program-specific settings (brkpts etc) */
    void saveProgramSettings();
    void restoreProgramSettings();

    // debugger process
    GdbProcess m_gdb;
    int m_gdbMajor, m_gdbMinor;
    bool m_explicitKill;		/* whether we are killing gdb ourselves */

#ifdef GDB_TRANSCRIPT
    // log file
    QFile m_logFile;
#endif

    QString m_statusMessage;

protected slots:
    void receiveOutput(KProcess*, char* buffer, int buflen);
    void commandRead(KProcess*);
    void gdbExited(KProcess*);
    void gotoFrame(int);
    void slotLocalsExpanding(KTreeViewItem*, bool&);
    void slotWatchExpanding(KTreeViewItem*, bool&);
    void slotToggleBreak(const QString&, int);
    void slotEnaDisBreak(const QString&, int);
    void slotUpdateAnimation();
    void slotDeleteWatch();
    
signals:
    /**
     * This signal is emitted whenever a part of the debugger needs to
     * highlight the specfied source code line (e.g. when the program
     * stops).
     * 
     * @param file specifies the file; this is not necessarily a full path
     * name, and if it is relative, you won't know relative to what, you
     * can only guess.
     * @param lineNo specifies the line number (0-based!) (this may be
     * negative, in which case the file should be activated, but the line
     * should NOT be changed).
     */
    void activateFileLine(const QString& file, int lineNo);

    /**
     * This signal is emitted when a line decoration item (those thingies
     * that indicate breakpoints) must be changed.
     */
    void lineItemsChanged();

    /**
     * This signal is a special case of @ref #lineItemsChanged because it
     * indicates that only the program counter has changed.
     *
     * @param filename specifies the filename where the program stopped
     * @param lineNo specifies the line number (zero-based); it can be -1
     * if it is unknown
     * @param frameNo specifies the frame number: 0 is the innermost frame,
     * positive numbers are frames somewhere up the stack (indicates points
     * where a function was called); the latter cases should be indicated
     * differently in the source window.
     */
    void updatePC(const QString& filename, int lineNo, int frameNo);

    /**
     * This signal is emitted when gdb detects that the executable has been
     * updated, e.g. recompiled. (You usually need not handle this signal
     * if you are the editor which changed the executable.)
     */
    void executableUpdated();

    /**
     * This signal is emitted when the animated icon should advance to the
     * next picture.
     */
    void animationTimeout();

    /**
     * Indicates that a new status message is available.
     */
    void updateStatusMessage();

    /**
     * Indicates that the internal state of the debugger has changed, and
     * that this will very likely have an impact on the UI.
     */
    void updateUI();

protected:
    BreakpointTable m_bpTable;
    ExprWnd& m_localVariables;
    ExprWnd& m_watchVariables;
    QListBox& m_btWindow;
    
    // animation
    QTimer m_animationTimer;
    int m_animationInterval;
    
    // implementation helpers
protected:
    void initMenu();
    void initToolbar();
    void initAnimation();
    void startAnimation(bool fast);
    void stopAnimation();

    QWidget* parentWidget() { return static_cast<QWidget*>(parent()); }

    friend class BreakpointTable;
};

#endif // DEBUGGER_H
