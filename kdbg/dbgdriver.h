// $Id$

// Copyright by Johannes Sixt
// This file is under GPL, the GNU General Public Licence

#ifndef DBGDRIVER_H
#define DBGDRIVER_H

#include <qqueue.h>
#include <qlist.h>
#include <qfile.h>
#include <kprocess.h>


class VarTree;
class ExprWnd;
class KDebugger;


enum DbgCommand {
	DCinitialize,
	DCtty,
	DCexecutable,
	DCtargetremote,
	DCcorefile,
	DCattach,
	DCinfolinemain,
	DCinfolocals,
	DCinforegisters,
	DCinfoline,
	DCdisassemble,
	DCsetargs,
	DCsetenv,
	DCunsetenv,
	DCcd,
	DCbt,
	DCrun,
	DCcont,
	DCstep,
	DCnext,
	DCfinish,
	DCuntil,			/* line number is zero-based! */
	DCkill,
	DCbreaktext,
	DCbreakline,			/* line number is zero-based! */
	DCtbreakline,			/* line number is zero-based! */
	DCdelete,
	DCenable,
	DCdisable,
	DCprint,
	DCprintStruct,
	DCprintQStringStruct,
	DCframe,
	DCfindType,
	DCinfosharedlib,
	DCinfobreak,
	DCcondition,
	DCignore
};

enum RunDevNull {
    RDNstdin = 0x1,			/* redirect stdin to /dev/null */
    RDNstdout = 0x2,			/* redirect stdout to /dev/null */
    RDNstderr = 0x4			/* redirect stderr to /dev/null */
};

/**
 * Debugger commands are placed in a queue. Only one command at a time is
 * sent down to the debugger. All other commands in the queue are retained
 * until the sent command has been processed by gdb. The debugger tells us
 * that it's done with the command by sending the prompt. The output of the
 * debugger is parsed at that time. Then, if more commands are in the
 * queue, the next one is sent to the debugger.
 */
struct CmdQueueItem
{
    DbgCommand m_cmd;
    QString m_cmdString;
    bool m_committed;			/* just a debugging aid */
    // remember which expression when printing an expression
    VarTree* m_expr;
    ExprWnd* m_exprWnd;
    // remember file position
    QString m_fileName;
    int m_lineNo;
    // whether command was emitted due to direct user request (only set when relevant)
    bool m_byUser;

    CmdQueueItem(DbgCommand cmd, const QString& str) :
    	m_cmd(cmd),
	m_cmdString(str),
	m_committed(false),
	m_expr(0),
	m_exprWnd(0),
	m_lineNo(0),
	m_byUser(false)
    { }
};

/**
 * The information about a breakpoint that is parsed from the list of
 * breakpoints.
 */
struct Breakpoint
{
    int id;				/* gdb's number */
    bool temporary;
    bool enabled;
    QString location;
    QString condition;			/* condition as printed by gdb */
    int ignoreCount;			/* ignore next that may hits */
    int hitCount;			/* as reported by gdb */
    // the following items repeat the location, but in a better usable way
    QString fileName;
    int lineNo;				/* zero-based line number */
};

/**
 * The information about a stack frame.
 */
struct StackFrame
{
    int frameNo;
    QString fileName;
    int lineNo;				/* zero-based line number */
    VarTree* var;			/* more information if non-zero */
    StackFrame() : var(0) { }
    ~StackFrame();
};

/**
 * Register information
 */
struct RegisterInfo
{
    QString regName;
    QString rawValue;
    QString cookedValue;		/* may be empty */
};

/**
 * This is an abstract base class for debugger process.
 *
 * This class represents the debugger program. It provides the low-level
 * interface to the commandline debugger. As such it implements the
 * commands and parses the output.
 */
class DebuggerDriver : public KProcess
{
    Q_OBJECT
public:
    DebuggerDriver();
    virtual ~DebuggerDriver() = 0;

    virtual QString driverName() const = 0;
    /**
     * Returns the default command string to invoke the debugger driver.
     */
    virtual QString defaultInvocation() const = 0;

    virtual bool startup(QString cmdStr);
    void dequeueCmdByVar(VarTree* var);
    void setLogFileName(const QString& fname) { m_logFileName = fname; }

protected:
    QString m_runCmd;
    
    enum DebuggerState {
	DSidle,				/* gdb waits for input */
	DSinterrupted,			/* a command was interrupted */
	DSrunningLow,			/* gdb is running a low-priority command */
	DSrunning,			/* gdb waits for program */
	DScommandSent,			/* command has been sent, we wait for wroteStdin signal */
	DScommandSentLow		/* low-prioritycommand has been sent */
    };
    DebuggerState m_state;

public:
    bool isIdle() const { return m_state == DSidle; }
    /**
     * Tells whether a high prority command would be executed immediately.
     */
    bool canExecuteImmediately() const { return m_hipriCmdQueue.isEmpty(); }

protected:
    char* m_output;			/* normal gdb output */
    int m_outputLen;			/* current accumulated output */
    int m_outputAlloc;			/* space available in m_gdbOutput */
#if QT_VERSION < 200
    typedef QString DelayedStr;
#else
    typedef QCString DelayedStr;
#endif
    QQueue<DelayedStr> m_delayedOutput;	/* output colleced while we have receivedOutput */
					/* but before signal wroteStdin arrived */

public:
    /**
     * Enqueues a high-priority command. High-priority commands are
     * executed before any low-priority commands. No user interaction is
     * possible as long as there is a high-priority command in the queue.
     */
    virtual CmdQueueItem* executeCmd(DbgCommand,
				     bool clearLow = false) = 0;
    virtual CmdQueueItem* executeCmd(DbgCommand, QString strArg,
				     bool clearLow = false) = 0;
    virtual CmdQueueItem* executeCmd(DbgCommand, int intArg,
				     bool clearLow = false) = 0;
    virtual CmdQueueItem* executeCmd(DbgCommand, QString strArg, int intArg,
				     bool clearLow = false) = 0;
    virtual CmdQueueItem* executeCmd(DbgCommand, QString strArg1, QString strArg2,
				     bool clearLow = false) = 0;
    virtual CmdQueueItem* executeCmd(DbgCommand, int intArg1, int intArg2,
				     bool clearLow = false) = 0;

    enum QueueMode {
	QMnormal,			/* queues the command last */
	QMoverride,			/* removes an already queued command */
	QMoverrideMoreEqual		/* ditto, also puts the command first in the queue */
    };

    /**
     * Enqueues a low-priority command. Low-priority commands are executed
     * after any high-priority commands.
     */
    virtual CmdQueueItem* queueCmd(DbgCommand,
				   QueueMode mode) = 0;
    virtual CmdQueueItem* queueCmd(DbgCommand, QString strArg,
				   QueueMode mode) = 0;
    virtual CmdQueueItem* queueCmd(DbgCommand, int intArg,
				   QueueMode mode) = 0;
    virtual CmdQueueItem* queueCmd(DbgCommand, QString strArg, int intArg,
				   QueueMode mode) = 0;
    virtual CmdQueueItem* queueCmd(DbgCommand, QString strArg1, QString strArg2,
				   QueueMode mode) = 0;

    /**
     * Flushes the command queues.
     * @param hipriOnly if true, only the high priority queue is flushed.
     */
    virtual void flushCommands(bool hipriOnly = false);

    /**
     * Terminates the debugger process.
     */
    virtual void terminate() = 0;

    /**
     * Interrupts the debuggee.
     */
    virtual void interruptInferior() = 0;

    /**
     * Parses the output as an array of QChars.
     */
    virtual VarTree* parseQCharArray(const char* output, bool wantErrorValue) = 0;

    /**
     * Parses a back-trace (the output of the DCbt command).
     */
    virtual void parseBackTrace(const char* output, QList<StackFrame>& stack) = 0;

    /**
     * Parses the output of the DCframe command;
     * @param frameNo Returns the frame number.
     * @param file Returns the source file name.
     * @param lineNo The zero-based line number.
     * @return false if the frame could not be parsed successfully. The
     * output values are undefined in this case.
     */
    virtual bool parseFrameChange(const char* output,
				  int& frameNo, QString& file, int& lineNo) = 0;

    /**
     * Parses a list of breakpoints.
     * @param output The output of the debugger.
     * @param brks The list of new #Breakpoint objects. The list
     * must initially be empty.
     * @return False if there was an error before the first breakpoint
     * was found. Even if true is returned, #brks may be empty.
     */
    virtual bool parseBreakList(const char* output, QList<Breakpoint>& brks) = 0;

    /**
     * Parses the output when the program stops to see whether this it
     * stopped due to a breakpoint.
     * @param output The output of the debugger.
     * @param id Returns the breakpoint id.
     * @param file Returns the file name in which the breakpoint is.
     * @param lineNo Returns he zero-based line number of the breakpoint.
     * @return False if there was no breakpoint.
     */
    virtual bool parseBreakpoint(const char* output, int& id,
				 QString& file, int& lineNo) = 0;

    /**
     * Parses the output of the DCinfolocals command.
     * @param output The output of the debugger.
     * @param newVars Receives the parsed variable values. The values are
     * simply append()ed to the supplied list.
     */
    virtual void parseLocals(const char* output, QList<VarTree>& newVars) = 0;

    /**
     * Parses the output of a DCprint or DCprintStruct command.
     * @param output The output of the debugger.
     * @param wantErrorValue Specifies whether the error message should be
     * provided as the value of a NKplain variable. If this is false,
     * #var will be 0 if the printed value is an error message.
     * @param var Returns the variable value. It is set to 0 if there was
     * a parse error or if the output is an error message and wantErrorValue
     * is false. If it is not 0, #var->text() will return junk and must be
     * set using #var->setText().
     * @return false if the output is an error message. Even if true is
     * returned, #var might still be 0 (due to a parse error).
     */
    virtual bool parsePrintExpr(const char* output, bool wantErrorValue,
				VarTree*& var) = 0;

    /**
     * Parses the output of the DCcd command.
     * @return false if the message is an error message.
     */
    virtual bool parseChangeWD(const char* output, QString& message) = 0;

    /**
     * Parses the output of the DCexecutable command.
     * @return false if an error occured.
     */
    virtual bool parseChangeExecutable(const char* output, QString& message) = 0;

    /**
     * Parses the output of the DCcorefile command.
     * @return false if the core file was not loaded successfully.
     */
    virtual bool parseCoreFile(const char* output) = 0;

    enum StopFlags {
	SFrefreshSource = 1,		/* refresh of source code is needed */
	SFrefreshBreak = 2,		/* refresh breakpoints */
	SFprogramActive = 4		/* program remains active */
    };
    /**
     * Parses the output of commands that execute (a piece of) the program.
     * @return The inclusive OR of zero or more of the StopFlags.
     */
    virtual uint parseProgramStopped(const char* output, QString& message) = 0;

    /**
     * Parses the output of the DCsharedlibs command.
     */
    virtual void parseSharedLibs(const char* output, QStrList& shlibs) = 0;

    /**
     * Parses the output of the DCfindType command.
     * @return true if a type was found.
     */
    virtual bool parseFindType(const char* output, QString& type) = 0;

    /**
     * Parses the output of the DCinforegisters command.
     */
    virtual void parseRegisters(const char* output, QList<RegisterInfo>& regs) = 0;

    /**
     * Parses the output of the DCinfoline command. Returns false if the
     * two addresses could not be found.
     */
    virtual bool parseInfoLine(const char* output,
			       QString& addrFrom, QString& addrTo) = 0;

    /**
     * Parses the ouput of the DCdisassemble command.
     */
    virtual QString parseDisassemble(const char* output) = 0;

protected:
    /** Removes all commands from the low-priority queue. */
    void flushLoPriQueue();
    /** Removes all commands from  the high-priority queue. */
    void flushHiPriQueue();

    QQueue<CmdQueueItem> m_hipriCmdQueue;
    QList<CmdQueueItem> m_lopriCmdQueue;
    /**
     * The active command is kept separately from other pending commands.
     */
    CmdQueueItem* m_activeCmd;
    /**
     * Helper function that queues the given command string in the
     * low-priority queue.
     */
    CmdQueueItem* queueCmdString(DbgCommand cmd, QString cmdString,
				 QueueMode mode);
    /**
     * Helper function that queues the given command string in the
     * high-priority queue.
     */
    CmdQueueItem* executeCmdString(DbgCommand cmd, QString cmdString,
				   bool clearLow);
    void writeCommand();
    virtual void commandFinished(CmdQueueItem* cmd) = 0;

protected:
    /** @internal */
    virtual int commSetupDoneC();

    char m_prompt[10];
    int m_promptLen;
    char m_promptLastChar;

    // log file
    QString m_logFileName;
    QFile m_logFile;

protected slots:
    void slotReceiveOutput(KProcess*, char* buffer, int buflen);
    void slotCommandRead(KProcess*);
    void slotExited(KProcess*);
    
signals:
    /**
     * This signal is emitted when the output of a command has been fully
     * collected and is ready to be interpreted.
     */
    void commandReceived(CmdQueueItem* cmd, const char* output);

    /**
     * This signal is emitted when the debugger recognizes that a specific
     * location in a file ought to be displayed.
     * 
     * Gdb's --fullname option supports this for the step, next, frame, and
     * run commands (and possibly others).
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
     * This signal is emitted when a command that starts the inferior has
     * been submitted to the debugger.
     */
    void inferiorRunning();

    /**
     * This signal is emitted when all output from the debugger has been
     * consumed and no more commands are in the queues.
     */
    void enterIdleState();
};

#endif // DBGDRIVER_H
