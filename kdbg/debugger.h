// $Id$

// Copyright by Johannes Sixt
// This file is under GPL, the GNU General Public Licence

#ifndef DEBUGGER_H
#define DEBUGGER_H

#include <qqueue.h>
#include <qlistbox.h>
#include <qlined.h>
#include <qlayout.h>
#include <qpushbt.h>
#include <ktopwidget.h>
#include <kprocess.h>
#include <ktabctl.h>
#include <knewpanner.h>
#include "winstack.h"
#include "exprwnd.h"
#include "brkpt.h"

// forward declarations
class UpdateUI;
class KStdAccel;

extern KStdAccel* keys;

class GdbProcess : public KProcess
{
public:
    GdbProcess();
protected:
    virtual int commSetupDoneC();
};

class KDebugger : public KTopLevelWidget
{
    Q_OBJECT
public:
    KDebugger(const char* name);
    ~KDebugger();
    
    bool debugProgram(const QString& executable);
    void setCoreFile(const QString& corefile) { m_corefile = corefile; }

protected:
    // session properties
    virtual void saveProperties(KConfig*);
    virtual void readProperties(KConfig*);
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
#ifdef WANT_THIS_PANE
	DCprintthis,
#endif
	DCprint,
	DCprintStruct,
	DCframe,
	DCfindType,
	DCinfobreak
    };
protected:
    pid_t m_outputTermPID;
    QString m_outputTermName;
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
    /**
     * Is the debugger ready to receive another high-priority command?
     */
    bool isReady() const { return m_haveExecutable &&
	    /*(m_state == DSidle || m_state == DSrunningLow)*/
	    m_hipriCmdQueue.isEmpty(); }
protected:
    QQueue<CmdQueueItem> m_hipriCmdQueue;
    QList<CmdQueueItem> m_lopriCmdQueue;
    CmdQueueItem* m_activeCmd;		/* the cmd we are working on */
    bool m_delayedPrintThis;		/* whether we delayed "print *this" */
    void parse(CmdQueueItem* cmd);
    VarTree* parseExpr(const char* name, bool wantErrorValue = true);
    void handleRunCommands();
    void updateAllExprs();
    void updateBreakptTable();
    bool parseLocals(QList<VarTree>& newVars);
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
    KSimpleConfig* m_programConfig;	/* program-specific settings (brkpts etc) */
    void saveProgramSettings();
    void restoreProgramSettings();

    // debugger process
    GdbProcess m_gdb;
    int m_gdbMajor, m_gdbMinor;

#ifdef GDB_TRANSCRIPT
    // log file
    QFile m_logFile;
#endif

    /**
     * Is the window that shows "this" visible?
     */
    bool isThisPaneVisible();
    void updateLineStatus(int lineNo);	/* zero-based line number */

public slots:
    virtual void menuCallback(int item);
    virtual void updateUIItem(UpdateUI* item);
    void receiveOutput(KProcess*, char* buffer, int buflen);
    void commandRead(KProcess*);
    void gdbExited(KProcess*);
    void updateUI();
    void gotoFrame(int);
    void slotAddWatch();
    void slotDeleteWatch();
    void slotWatchHighlighted(int);
    void slotLocalsExpanding(KTreeViewItem*, bool&);
    void slotWatchExpanding(KTreeViewItem*, bool&);
    void slotFileChanged();
    void slotLineChanged();
    void slotAnimationTimeout();
    void slotToggleBreak(const QString&, int);
    void slotEnaDisBreak(const QString&, int);
    /*
     * Unless WANT_THIS_PANE is defined, the following slots are unused.
     * Removing them with #ifdef ... #endif doesn't work because moc
     * doesn't understand the preprocessor defines. Hence, we leave the
     * declarations here and only remove the function body with #ifdef ...
     * #endif.
     */
    void slotThisExpanding(KTreeViewItem*, bool&);
    void slotFrameTabChanged(int);
    
signals:
    void forwardMenuCallback(int item);
    
protected:
    BreakpointTable m_bpTable;
    
    KMenuBar m_menu;
    KToolBar m_toolbar;
    KStatusBar m_statusbar;
    // statusbar texts
    QString m_statusActive;
    // animated buttons
    QList<QPixmap> m_animation;
    QTimer m_animationTimer;
    uint m_animationCounter;
    int m_animationInterval;
    
    // view windows
    KNewPanner m_mainPanner;
    KNewPanner m_leftPanner;
    KNewPanner m_rightPanner;
    WinStack m_filesWindow;
    QListBox m_btWindow;
#ifdef WANT_THIS_PANE
    KTabCtl m_frameVariables;
#endif
    ExprWnd m_localVariables;
#ifdef WANT_THIS_PANE
    ExprWnd m_this;
#endif
    
    QWidget m_watches;
    QLineEdit m_watchEdit;
    QPushButton m_watchAdd;
    QPushButton m_watchDelete;
    ExprWnd m_watchVariables;
    QVBoxLayout m_watchV;
    QHBoxLayout m_watchH;

    // menus
    QPopupMenu m_menuFile;
    QPopupMenu m_menuView;
    QPopupMenu m_menuProgram;
    QPopupMenu m_menuBrkpt;
    QPopupMenu m_menuWindow;
    QPopupMenu m_menuHelp;
    
    // implementation helpers
protected:
    void initMenu();
    void initToolbar();
    void initAnimation();
    void startAnimation(bool fast);
    void stopAnimation();

    friend class BreakpointTable;
};

#endif // DEBUGGER_H
