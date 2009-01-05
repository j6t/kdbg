/*
 * Copyright Johannes Sixt
 * This file is licensed under the GNU General Public License Version 2.
 * See the file COPYING in the toplevel directory of the source directory.
 */

#ifndef DEBUGGER_H
#define DEBUGGER_H

#include <qtimer.h>
#include <qdict.h>
#include <qstringlist.h>
#include "envvar.h"
#include "exprwnd.h"			/* some compilers require this */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

class ExprWnd;
class VarTree;
struct ExprValue;
class ProgramTypeTable;
class KTreeViewItem;
class KConfig;
class KConfigBase;
class ProgramConfig;
class QListBox;
class RegisterInfo;
class ThreadInfo;
class DebuggerDriver;
class CmdQueueItem;
class Breakpoint;
struct DisassembledCode;
struct MemoryDump;
struct DbgAddr;
class KProcess;


class KDebugger : public QObject
{
    Q_OBJECT
public:
    KDebugger(QWidget* parent,		/* will be used as the parent for dialogs */
	      ExprWnd* localVars,
	      ExprWnd* watchVars,
	      QListBox* backtrace);
    ~KDebugger();

    /**
     * This function starts to debug the specified executable using the
     * specified driver. If a program is currently being debugged, it is
     * terminated first. Ownership of driver is taken if and only if
     * true is returned.
     *
     * @return false if an error occurs.
     */
    bool debugProgram(const QString& executable,
		      DebuggerDriver* driver);

    /**
     * Uses the specified core to debug the active program.
     * @param batch tells whether the core file was given on the
     * command line.
     */
    void useCoreFile(QString corefile, bool batch);

    /**
     * Overrides the program argument in the per-program config
     * with a new value.
     */
    void overrideProgramArguments(const QString& args);


    /**
     * Uses the specified pid to attach to the active program.
     */
    void setAttachPid(const QString& pid);

    /**
     * Attaches to the specified process and debugs it.
     */
    void attachProgram(const QString& pid);

    /**
     * Returns the file name of the per-program config file for the
     * specified program.
     */
    static QString getConfigForExe(const QString& exe);

    /**
     * The driver name entry in the per-program config file.
     */
    static const char DriverNameEntry[];

public slots:
    /**
     * Runs the program or continues it if it is stopped at a breakpoint.
     */
    void programRun();

    /**
     * Restarts the debuggee.
     */
    void programRunAgain();

    /**
     * Performs a single-step, possibly stepping into a function call.
     * If byInsn is true, a step by instruction is performed.
     */
    void programStep();

    /**
     * Performs a single-step, stepping over a function call.
     * If byInsn is true, a step by instruction is performed.
     */
    void programNext();

    /**
     * Performs a single-step by instruction, possibly stepping into a
     * function call.
     */
    void programStepi();

    /**
     * Performs a single-step by instruction, stepping over a function
     * call.
     */
    void programNexti();

    /**
     * Runs the program until it returns from the current function.
     */
    void programFinish();

    /**
     * Kills the program (removes it from memory).
     */
    void programKill();

    /**
     * Interrupts the program if it is currently running.
     */
    void programBreak();

    /**
     * Moves the program counter to the specified line.
     * If an address is given, it is moved to the address.
     */
    void setProgramCounter(const QString&, int, const DbgAddr&);

public:
    /**
     * Queries the user for program arguments.
     */
    void programArgs(QWidget* parent);

    /**
     * Queries the user for program settings: Debugger command, terminal
     * emulator.
     */
    void programSettings(QWidget* parent);

    /**
     * Setup remote debugging device
     */
    void setRemoteDevice(const QString& remoteDevice) { m_remoteDevice = remoteDevice; }

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
     * @param fileName The source file in which to set the breakpoint.
     * @param lineNo The zero-based line number.
     * @param address The exact address of the breakpoint.
     * @param temporary Specifies whether this is a temporary breakpoint
     * @return false if the command was not executed, e.g. because the
     * debuggee is running at the moment.
     */
    bool setBreakpoint(QString fileName, int lineNo,
		       const DbgAddr& address, bool temporary);

    /**
     * Set a breakpoint.
     * 
     * @param bp Describes the breakpoint.
     * @param queueOnly If false, the breakpoint is set using a high-priority command.
     */
    void setBreakpoint(Breakpoint* bp, bool queueOnly);

    /**
     * Enable or disable a breakpoint at the specified location.
     * 
     * @param fileName The source file in which the breakpoint is.
     * @param lineNo The zero-based line number.
     * @param address The exact address of the breakpoint.
     * @return false if the command was not executed, e.g. because the
     * debuggee is running at the moment.
     */
    bool enableDisableBreakpoint(QString fileName, int lineNo,
				 const DbgAddr& address);

    /**
     * Enables or disables the specified breakpoint.
     *
     * @return false if the command was not executed, e.g. because the
     * debuggee is running at the moment.
     */
    bool enableDisableBreakpoint(int id)
    { return enableDisableBreakpoint(breakpointById(id)); }

    /**
     * Removes the specified breakpoint. Note that if bp is an orphaned
     * breakpoint, then bp is an invalid pointer if (and only if) this
     * function returns true.
     *
     * @return false if the command was not executed, e.g. because the
     * debuggee is running at the moment.
     */
    bool deleteBreakpoint(int id)
    { return deleteBreakpoint(breakpointById(id)); }

    /**
     * Changes the specified breakpoint's condition and ignore count.
     *
     * @return false if the command was not executed, e.g. because the
     * debuggee is running at the moment.
     */
    bool conditionalBreakpoint(int id,
			       const QString& condition,
			       int ignoreCount)
    { return conditionalBreakpoint(breakpointById(id), condition, ignoreCount); }

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
     * Tells whether a the program is loaded, but not active.
     */
    bool canStart();

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
    bool isReady() const;

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

    /**
     * Is the debugger driver idle?
     */
    bool isIdle() const;

    /* The list of breakpoints. */
    typedef std::list<Breakpoint>::const_iterator BrkptROIterator;
    BrkptROIterator breakpointsBegin() const { return m_brkpts.begin(); }
    BrkptROIterator breakpointsEnd() const { return m_brkpts.end(); }

    const QString& executable() const { return m_executable; }

    /**
     * Terminal emulation level.
     */
    enum TTYLevel {
	ttyNone = 0,			/* ignore output, input triggers EOF */
	ttySimpleOutputOnly = 1,	/* minmal output emulation, input triggers EOF */
	ttyFull = 7			/* program needs full emulation */
    };

    /**
     * Returns the level of terminal emulation requested by the inferior.
     */
    TTYLevel ttyLevel() const { return m_ttyLevel; }

    /** Sets the terminal that is to be used by the debugger. */
    void setTerminal(const QString& term) { m_inferiorTerminal = term; }

    /** Returns the debugger driver. */
    DebuggerDriver* driver() { return m_d; }

    /** Returns the pid that the debugger is currently attached to. */
    const QString& attachedPid() const { return m_attachedPid; }

    /**
     * The memory at that the expression evaluates to is watched. Can be
     * empty. Triggers a redisplay even if the expression did not change.
     */
    void setMemoryExpression(const QString& memexpr);

    /**
     * Sets how the watched memory location is displayed.
     * Call setMemoryExpression() to force a redisplay.
     */
    void setMemoryFormat(unsigned format) { m_memoryFormat = format; }

    // settings
    void saveSettings(KConfig*);
    void restoreSettings(KConfig*);

protected:
    QString m_inferiorTerminal;
    QString m_debuggerCmd;		/* per-program setting */
    TTYLevel m_ttyLevel;		/* level of terminal emulation */
    bool startDriver();
    void stopDriver();
    void writeCommand();
    
    QStringList m_watchEvalExpr;	/* exprs to evaluate for watch window */
    std::list<Breakpoint> m_brkpts;
    QString m_memoryExpression;		/* memory location to watch */
    unsigned m_memoryFormat;		/* how that output should look */

protected slots:
    void parse(CmdQueueItem* cmd, const char* output);
protected:
    void handleRunCommands(const char* output);
    void updateAllExprs();
    void updateProgEnvironment(const QString& args, const QString& wd,
			       const QDict<EnvVar>& newVars,
			       const QStringList& newOptions);
    void parseLocals(const char* output, std::list<ExprValue*>& newVars);
    void handleLocals(const char* output);
    bool handlePrint(CmdQueueItem* cmd, const char* output);
    bool handlePrintDeref(CmdQueueItem* cmd, const char* output);
    void handleBacktrace(const char* output);
    void handleFrameChange(const char* output);
    void handleFindType(CmdQueueItem* cmd, const char* output);
    void handlePrintStruct(CmdQueueItem* cmd, const char* output);
    void handleSharedLibs(const char* output);
    void handleRegisters(const char* output);
    void handleMemoryDump(const char* output);
    void handleInfoLine(CmdQueueItem* cmd, const char* output);
    void handleDisassemble(CmdQueueItem* cmd, const char* output);
    void handleThreadList(const char* output);
    void handleSetPC(const char* output);
    void handleSetVariable(CmdQueueItem* cmd, const char* output);
    void evalExpressions();
    void evalInitialStructExpression(VarTree* var, ExprWnd* wnd, bool immediate);
    void evalStructExpression(VarTree* var, ExprWnd* wnd, bool immediate);
    void dereferencePointer(ExprWnd* wnd, VarTree* var, bool immediate);
    void determineType(ExprWnd* wnd, VarTree* var);
    void queueMemoryDump(bool immediate);
    CmdQueueItem* loadCoreFile();
    void openProgramConfig(const QString& name);

    typedef std::list<Breakpoint>::iterator BrkptIterator;
    BrkptIterator breakpointByFilePos(QString file, int lineNo,
				    const DbgAddr& address);
    BrkptIterator breakpointById(int id);
    CmdQueueItem* executeBreakpoint(const Breakpoint* bp, bool queueOnly);
    void newBreakpoint(CmdQueueItem* cmd, const char* output);
    void updateBreakList(const char* output);
    bool stopMayChangeBreakList() const;
    void saveBreakpoints(ProgramConfig* config);
    void restoreBreakpoints(ProgramConfig* config);
    bool enableDisableBreakpoint(BrkptIterator bp);
    bool deleteBreakpoint(BrkptIterator bp);
    bool conditionalBreakpoint(BrkptIterator bp,
			       const QString& condition,
			       int ignoreCount);

    bool m_haveExecutable;		/* has an executable been specified */
    bool m_programActive;		/* is the program active (possibly halting in a brkpt)? */
    bool m_programRunning;		/* is the program executing (not stopped)? */
    bool m_sharedLibsListed;		/* do we know the shared libraries loaded by the prog? */
    QString m_executable;
    QString m_corefile;
    QString m_attachedPid;		/* user input of attaching to pid */
    QString m_programArgs;
    QString m_remoteDevice;
    QString m_programWD;		/* working directory of gdb */
    QDict<EnvVar> m_envVars;		/* environment variables set by user */
    QStringList m_boolOptions;		/* boolean options */
    QStringList m_sharedLibs;		/* shared libraries used by program */
    ProgramTypeTable* m_typeTable;	/* known types used by the program */
    ProgramConfig* m_programConfig;	/* program-specific settings (brkpts etc) */
    void saveProgramSettings();
    void restoreProgramSettings();
    QString readDebuggerCmd();

    // debugger process
    DebuggerDriver* m_d;
    bool m_explicitKill;		/* whether we are killing gdb ourselves */

    QString m_statusMessage;

protected slots:
    void gdbExited(KProcess*);
    void slotInferiorRunning();
    void backgroundUpdate();
    void gotoFrame(int);
    void slotExpanding(QListViewItem*);
    void slotDeleteWatch();
    void slotValuePopup(const QString&);
    void slotDisassemble(const QString&, int);
    void slotValueEdited(VarTree*, const QString&);
public slots:
    void setThread(int);
    void shutdown();

signals:
    /**
     * This signal is emitted before the debugger is started. The slot is
     * supposed to set up m_inferiorTerminal.
     */
    void debuggerStarting();

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
     * @param address specifies the exact address of the PC or is empty.
     */
    void activateFileLine(const QString& file, int lineNo, const DbgAddr& address);

    /**
     * This signal indicates that the program counter has changed.
     *
     * @param filename specifies the filename where the program stopped
     * @param lineNo specifies the line number (zero-based); it can be -1
     * if it is unknown
     * @param address specifies the address that the instruction pointer
     * points to.
     * @param frameNo specifies the frame number: 0 is the innermost frame,
     * positive numbers are frames somewhere up the stack (indicates points
     * where a function was called); the latter cases should be indicated
     * differently in the source window.
     */
    void updatePC(const QString& filename, int lineNo,
		  const DbgAddr& address, int frameNo);

    /**
     * This signal is emitted when gdb detects that the executable has been
     * updated, e.g. recompiled. (You usually need not handle this signal
     * if you are the editor which changed the executable.)
     */
    void executableUpdated();

    /**
     * Indicates that a new status message is available.
     */
    void updateStatusMessage();

    /**
     * Indicates that the internal state of the debugger has changed, and
     * that this will very likely have an impact on the UI.
     */
    void updateUI();

    /**
     * Indicates that the list of breakpoints has possibly changed.
     */
    void breakpointsChanged();

    /**
     * Indicates that the register values have possibly changed.
     */
    void registersChanged(const std::list<RegisterInfo>&);

    /**
     * Indicates that the list of threads has possibly changed.
     */
    void threadsChanged(const std::list<ThreadInfo>&);

    /**
     * Indicates that the value for a value popup is ready.
     */
    void valuePopup(const QString&);

    /**
     * Provides the disassembled code of the location given by file and
     * line number (zero-based).
     */
    void disassembled(const QString& file, int line, const std::list<DisassembledCode>& code);

    /**
     * Indicates that the program has stopped for any reason: by a
     * breakpoint, by a signal that the debugger driver caught, by a single
     * step instruction.
     */
    void programStopped();

    /**
     * Indicates that a new memory dump output is ready.
     * @param msg is an error message or empty
     * @param memdump is the memory dump
     */
    void memoryDumpChanged(const QString&, const std::list<MemoryDump>&);

    /**
     * Gives other objects a chance to save program specific settings.
     */
    void saveProgramSpecific(KConfigBase* config);

    /**
     * Gives other objects a chance to restore program specific settings.
     */
    void restoreProgramSpecific(KConfigBase* config);

protected:
    ExprWnd& m_localVariables;
    ExprWnd& m_watchVariables;
    QListBox& m_btWindow;

    // implementation helpers
protected:
    QWidget* parentWidget() { return static_cast<QWidget*>(parent()); }
};

#endif // DEBUGGER_H
