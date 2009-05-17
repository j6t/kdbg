/*
 * Copyright Johannes Sixt
 * This file is licensed under the GNU General Public License Version 2.
 * See the file COPYING in the toplevel directory of the source directory.
 */

#ifndef DBGDRIVER_H
#define DBGDRIVER_H

#include <qfile.h>
#include <qregexp.h>
#include <qcstring.h>
#include <kprocess.h>
#include <queue>
#include <list>


class VarTree;
class ExprValue;
class ExprWnd;
class KDebugger;
class QStringList;


/**
 * A type representing an address.
 */
struct DbgAddr
{
    QString a;
    QString fnoffs;
    DbgAddr() { }
    DbgAddr(const QString& aa);
    DbgAddr(const DbgAddr& src) : a(src.a), fnoffs(src.fnoffs) { }
    void operator=(const QString& aa);
    void operator=(const DbgAddr& src) { a = src.a; fnoffs = src.fnoffs; }
    QString asString() const;
    bool isEmpty() const { return a.isEmpty(); }
protected:
    void cleanAddr();
};
bool operator==(const DbgAddr& a1, const DbgAddr& a2);
bool operator>(const DbgAddr& a1, const DbgAddr& a2);


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
	DCexamine,
	DCinfoline,
	DCdisassemble,
	DCsetargs,
	DCsetenv,
	DCunsetenv,
	DCsetoption,                    /* debugger options */
	DCcd,
	DCbt,
	DCrun,
	DCcont,
	DCstep,
	DCstepi,
	DCnext,
	DCnexti,
	DCfinish,
	DCuntil,			/* line number is zero-based! */
	DCkill,
	DCbreaktext,
	DCbreakline,			/* line number is zero-based! */
	DCtbreakline,			/* line number is zero-based! */
	DCbreakaddr,
	DCtbreakaddr,
	DCwatchpoint,
	DCdelete,
	DCenable,
	DCdisable,
	DCprint,
	DCprintDeref,
	DCprintStruct,
	DCprintQStringStruct,
	DCframe,
	DCfindType,
	DCinfosharedlib,
	DCthread,
	DCinfothreads,
	DCinfobreak,
	DCcondition,
	DCsetpc,
	DCignore,
	DCprintWChar,
	DCsetvariable
};

enum RunDevNull {
    RDNstdin = 0x1,			/* redirect stdin to /dev/null */
    RDNstdout = 0x2,			/* redirect stdout to /dev/null */
    RDNstderr = 0x4			/* redirect stderr to /dev/null */
};

/**
 * How the memory dump is formated. The lowest 4 bits define the size of
 * the entities. The higher bits define how these are formatted. Note that
 * not all combinations make sense.
 */
enum MemoryDumpType {
    // sizes
    MDTbyte = 0x1,
    MDThalfword = 0x2,
    MDTword = 0x3,
    MDTgiantword = 0x4,
    MDTsizemask = 0xf,
    // formats
    MDThex = 0x10,
    MDTsigned = 0x20,
    MDTunsigned = 0x30,
    MDToctal = 0x40,
    MDTbinary = 0x50,
    MDTaddress = 0x60,
    MDTchar = 0x70,
    MDTfloat = 0x80,
    MDTstring = 0x90,
    MDTinsn = 0xa0,
    MDTformatmask = 0xf0
};

struct Breakpoint;

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
    // the breakpoint info
    Breakpoint* m_brkpt;
    int m_existingBrkpt;
    // whether command was emitted due to direct user request (only set when relevant)
    bool m_byUser;

    CmdQueueItem(DbgCommand cmd, const QString& str) :
    	m_cmd(cmd),
	m_cmdString(str),
	m_committed(false),
	m_expr(0),
	m_exprWnd(0),
	m_lineNo(0),
	m_brkpt(0),
	m_existingBrkpt(0),
	m_byUser(false)
    { }

    struct IsEqualCmd
    {
	IsEqualCmd(DbgCommand cmd, const QString& str) : m_cmd(cmd), m_str(str) { }
	bool operator()(CmdQueueItem*) const;
	DbgCommand m_cmd;
	const QString& m_str;
    };
};

/**
 * The information about a breakpoint that is parsed from the list of
 * breakpoints.
 */
struct Breakpoint
{
    int id;				/* gdb's number */
    enum Type {
	breakpoint, watchpoint 
    } type;
    bool temporary;
    bool enabled;
    QString location;
    QString text;			/* text if set using DCbreaktext */
    DbgAddr address;			/* exact address of breakpoint */
    QString condition;			/* condition as printed by gdb */
    int ignoreCount;			/* ignore next that may hits */
    int hitCount;			/* as reported by gdb */
    // the following items repeat the location, but in a better usable way
    QString fileName;
    int lineNo;				/* zero-based line number */
    Breakpoint();
    bool isOrphaned() const { return id < 0; }
};

/**
 * Information about a stack frame.
 */
struct FrameInfo
{
    QString fileName;
    int lineNo;				/* zero-based line number */
    DbgAddr address;			/* exact address of PC */
};

/**
 * The information about a stack frame as parsed from the backtrace.
 */
struct StackFrame : FrameInfo
{
    int frameNo;
    ExprValue* var;			/* more information if non-zero */
    StackFrame() : var(0) { }
    ~StackFrame();
};

/**
 * The information about a thread as parsed from the threads list.
 */
struct ThreadInfo : FrameInfo
{
    int id;				/* gdb's number */
    QString threadName;			/* the SYSTAG */
    QString function;			/* where thread is halted */
    bool hasFocus;			/* the thread whose stack we are watching */
};

/**
 * Register information
 */
struct RegisterInfo
{
    QString regName;
    QString rawValue;
    QString cookedValue;		/* may be empty */
    QString type;			/* of vector register if not empty */
};

/**
 * Disassembled code
 */
struct DisassembledCode
{
    DbgAddr address;
    QString code;
};

/**
 * Memory contents
 */
struct MemoryDump
{
    DbgAddr address;
    QString dump;
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

    /**
     * Returns a list of options that can be  turned on and off.
     */
    virtual QStringList boolOptionList() const = 0;

    virtual bool startup(QString cmdStr);
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
    bool canExecuteImmediately() const { return m_hipriCmdQueue.empty(); }

protected:
    char* m_output;			/* normal gdb output */
    size_t m_outputLen;			/* amount of data so far accumulated in m_output */
    size_t m_outputAlloc;		/* space available in m_output */
    std::queue<QByteArray> m_delayedOutput;	/* output colleced while we have receivedOutput */
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
     * Terminates the debugger process, but also detaches any program that
     * it has been attached to.
     */
    virtual void detachAndTerminate() = 0;

    /**
     * Interrupts the debuggee.
     */
    virtual void interruptInferior() = 0;

    /**
     * Specifies the command that prints the QString data.
     */
    virtual void setPrintQStringDataCmd(const char* cmd) = 0;

    /**
     * Parses the output as an array of QChars.
     */
    virtual ExprValue* parseQCharArray(const char* output, bool wantErrorValue, bool qt3like) = 0;

    /**
     * Parses a back-trace (the output of the DCbt command).
     */
    virtual void parseBackTrace(const char* output, std::list<StackFrame>& stack) = 0;

    /**
     * Parses the output of the DCframe command;
     * @param frameNo Returns the frame number.
     * @param file Returns the source file name.
     * @param lineNo The zero-based line number.
     * @param address Returns the exact address.
     * @return false if the frame could not be parsed successfully. The
     * output values are undefined in this case.
     */
    virtual bool parseFrameChange(const char* output, int& frameNo,
				  QString& file, int& lineNo, DbgAddr& address) = 0;

    /**
     * Parses a list of breakpoints.
     * @param output The output of the debugger.
     * @param brks The list of new #Breakpoint objects. The list
     * must initially be empty.
     * @return False if there was an error before the first breakpoint
     * was found. Even if true is returned, #brks may be empty.
     */
    virtual bool parseBreakList(const char* output, std::list<Breakpoint>& brks) = 0;

    /**
     * Parses a list of threads.
     * @param output The output of the debugger.
     * @return The new thread list. There is no indication if there was
     * a parse error.
     */
    virtual std::list<ThreadInfo> parseThreadList(const char* output) = 0;

    /**
     * Parses the output when the program stops to see whether this it
     * stopped due to a breakpoint.
     * @param output The output of the debugger.
     * @param id Returns the breakpoint id.
     * @param file Returns the file name in which the breakpoint is.
     * @param lineNo Returns the zero-based line number of the breakpoint.
     * @param address Returns the address of the breakpoint.
     * @return False if there was no breakpoint.
     */
    virtual bool parseBreakpoint(const char* output, int& id,
				 QString& file, int& lineNo, QString& address) = 0;

    /**
     * Parses the output of the DCinfolocals command.
     * @param output The output of the debugger.
     * @param newVars Receives the parsed variable values. The values are
     * simply append()ed to the supplied list.
     */
    virtual void parseLocals(const char* output, std::list<ExprValue*>& newVars) = 0;

    /**
     * Parses the output of a DCprint or DCprintStruct command.
     * @param output The output of the debugger.
     * @param wantErrorValue Specifies whether the error message should be
     * provided as the value of a NKplain variable. If this is false,
     * 0 is returned if the printed value is an error message.
     * @return the parsed value. It is 0 if there was a parse error
     * or if the output is an error message and #wantErrorValue
     * is \c false. The returned object's text() is undefined.
     */
    virtual ExprValue* parsePrintExpr(const char* output, bool wantErrorValue) = 0;

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
	SFrefreshThreads = 4,		/* refresh thread list */
	SFprogramActive = 128		/* program remains active */
    };
    /**
     * Parses the output of commands that execute (a piece of) the program.
     * @return The inclusive OR of zero or more of the StopFlags.
     */
    virtual uint parseProgramStopped(const char* output, QString& message) = 0;

    /**
     * Parses the output of the DCsharedlibs command.
     */
    virtual QStringList parseSharedLibs(const char* output) = 0;

    /**
     * Parses the output of the DCfindType command.
     * @return true if a type was found.
     */
    virtual bool parseFindType(const char* output, QString& type) = 0;

    /**
     * Parses the output of the DCinforegisters command.
     */
    virtual std::list<RegisterInfo> parseRegisters(const char* output) = 0;

    /**
     * Parses the output of the DCinfoline command. Returns false if the
     * two addresses could not be found.
     */
    virtual bool parseInfoLine(const char* output,
			       QString& addrFrom, QString& addrTo) = 0;

    /**
     * Parses the ouput of the DCdisassemble command.
     */
    virtual std::list<DisassembledCode> parseDisassemble(const char* output) = 0;

    /**
     * Parses a memory dump. Returns an empty string if no error was found;
     * otherwise it contains an error message.
     */
    virtual QString parseMemoryDump(const char* output, std::list<MemoryDump>& memdump) = 0;

    /**
     * Parses the output of the DCsetvariable command. Returns an empty
     * string if no error was found; otherwise it contains an error
     * message.
     */
    virtual QString parseSetVariable(const char* output) = 0;

    /**
     * Returns a value that the user can edit.
     */
    virtual QString editableValue(VarTree* value);

protected:
    /** Removes all commands from the low-priority queue. */
    void flushLoPriQueue();
    /** Removes all commands from  the high-priority queue. */
    void flushHiPriQueue();

    std::queue<CmdQueueItem*> m_hipriCmdQueue;
    std::list<CmdQueueItem*> m_lopriCmdQueue;
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
    size_t m_promptMinLen;
    char m_promptLastChar;
    QRegExp m_promptRE;

    // log file
    QString m_logFileName;
    QFile m_logFile;

public slots:
    void dequeueCmdByVar(VarTree* var);

protected slots:
    virtual void slotReceiveOutput(KProcess*, char* buffer, int buflen);
    virtual void slotCommandRead(KProcess*);
    virtual void slotExited(KProcess*);
    
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
     * @param address specifies the exact address of the PC or is empty.
     */
    void activateFileLine(const QString& file, int lineNo, const DbgAddr& address);

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
