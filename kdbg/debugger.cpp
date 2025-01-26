/*
 * Copyright Johannes Sixt
 * This file is licensed under the GNU General Public License Version 2.
 * See the file COPYING in the toplevel directory of the source directory.
 */

#include "debugger.h"
#include "dbgdriver.h"
#include "pgmargs.h"
#include "typetable.h"
#include "exprwnd.h"
#include "pgmsettings.h"
#include <QFileInfo>
#include <QListWidget>
#include <QApplication>
#include <QCryptographicHash>
#include <QStandardPaths>
#include <kconfig.h>
#include <kconfiggroup.h>
#include <klocalizedstring.h>		/* i18n */
#include <kmessagebox.h>
#include <ctype.h>
#include <stdlib.h>			/* strtol, atoi */
#include <unistd.h>			/* sleep(3) */
#include <algorithm>
#include "mydebug.h"

/**
 * Returns expression value for a tooltip.
 */
template <class T>
QString formatPopupValue(const T* v)
{
    QString tip;

    if (!v->value().isEmpty())
    {
        tip += v->value();
    }
    else
    {
	// no value: we use some hint
        switch (v->m_varKind) {
        case VarTree::VKstruct:
            tip += "{...}";
            break;
        case VarTree::VKarray:
            tip += "[...]";
            break;
        default:
            tip += "?""?""?";	// 2 question marks in a row would be a trigraph
            break;
        }
    }

    return tip;
}

KDebugger::KDebugger(QWidget* parent,
		     ExprWnd* localVars,
		     ExprWnd* watchVars,
		     QListWidget* backtrace) :
	QObject(parent),
	m_ttyLevel(ttyFull),
	m_memoryFormat(MDTword | MDThex),
	m_memoryLength(16),
	m_haveExecutable(false),
	m_programActive(false),
	m_programRunning(false),
	m_sharedLibsListed(false),
	m_typeTable(0),
	m_programConfig(0),
	m_d(0),
	m_localVariables(*localVars),
	m_watchVariables(*watchVars),
	m_btWindow(*backtrace)
{
    connect(&m_localVariables, SIGNAL(itemExpanded(QTreeWidgetItem*)),
	    SLOT(slotExpanding(QTreeWidgetItem*)));
    connect(&m_watchVariables, SIGNAL(itemExpanded(QTreeWidgetItem*)),
	    SLOT(slotExpanding(QTreeWidgetItem*)));
    connect(&m_localVariables, SIGNAL(editValueCommitted(VarTree*, const QString&)),
	    SLOT(slotValueEdited(VarTree*, const QString&)));
    connect(&m_watchVariables, SIGNAL(editValueCommitted(VarTree*, const QString&)),
	    SLOT(slotValueEdited(VarTree*, const QString&)));

    connect(&m_btWindow, SIGNAL(currentRowChanged(int)), this, SLOT(gotoFrame(int)));

    emit updateUI();
}

KDebugger::~KDebugger()
{
    if (m_programConfig != 0) {
	saveProgramSettings();
	m_programConfig->sync();
	delete m_programConfig;
    }

    delete m_typeTable;
}


void KDebugger::saveSettings(KConfig* /*config*/)
{
}

void KDebugger::restoreSettings(KConfig* /*config*/)
{
}


//////////////////////////////////////////////////////////////////////
// external interface

const char GeneralGroup[] = "General";
const char DebuggerCmdStr[] = "DebuggerCmdStr";
const char TTYLevelEntry[] = "TTYLevel";
const char KDebugger::DriverNameEntry[] = "DriverName";
const char DisassemblyFlavor[] = "DisassemblyFlavor";

bool KDebugger::debugProgram(const QString& name,
			     DebuggerDriver* driver)
{
    if (m_d != 0 && m_d->isRunning())
    {
	QApplication::setOverrideCursor(Qt::WaitCursor);

	stopDriver();

	QApplication::restoreOverrideCursor();

	if (m_d->isRunning() || m_haveExecutable) {
	    /* timed out! We can't really do anything useful now */
	    TRACE("timed out while waiting for gdb to die!");
	    return false;
	}
	delete m_d;
	m_d = 0;
    }

    // wire up the driver
    connect(driver, SIGNAL(activateFileLine(const QString&,int,const DbgAddr&)),
	    this, SIGNAL(activateFileLine(const QString&,int,const DbgAddr&)));
    connect(driver, SIGNAL(finished(int, QProcess::ExitStatus)),
	    SLOT(gdbExited()));
    connect(driver, SIGNAL(commandReceived(CmdQueueItem*,const char*)),
	    SLOT(parse(CmdQueueItem*,const char*)));
    connect(driver, SIGNAL(bytesWritten(qint64)), SIGNAL(updateUI()));
    connect(driver, SIGNAL(inferiorRunning()), SLOT(slotInferiorRunning()));
    connect(driver, SIGNAL(enterIdleState()), SLOT(backgroundUpdate()));
    connect(driver, SIGNAL(enterIdleState()), SIGNAL(updateUI()));
    connect(&m_localVariables, SIGNAL(removingItem(VarTree*)),
	    driver, SLOT(dequeueCmdByVar(VarTree*)));
    connect(&m_watchVariables, SIGNAL(removingItem(VarTree*)),
	    driver, SLOT(dequeueCmdByVar(VarTree*)));

    // create the program settings object
    openProgramConfig(name);

    // get debugger command from per-program settings
    if (m_programConfig != 0) {
	KConfigGroup g = m_programConfig->group(GeneralGroup);
	m_debuggerCmd = g.readEntry(DebuggerCmdStr);
	// get terminal emulation level
	m_ttyLevel = TTYLevel(g.readEntry(TTYLevelEntry, int(ttyFull)));
    }
    // the rest is read in later in the handler of DCexecutable

    m_d = driver;

    if (!startDriver()) {
	TRACE("startDriver failed");
	m_d = 0;
	return false;
    }

    TRACE("before file cmd");
    m_d->executeCmd(DCexecutable, name);
    m_executable = name;

    m_d->executeCmd(DCinfotarget);

    // set remote target
    if (!m_remoteDevice.isEmpty()) {
	m_d->executeCmd(DCtargetremote, m_remoteDevice);
	m_d->queueCmd(DCbt);
	m_d->queueCmd(DCinfothreads);
	m_d->queueCmdAgain(DCframe, 0);
	m_programActive = true;
	m_haveExecutable = true;
    }

    // create a type table
    m_typeTable = new ProgramTypeTable;
    m_sharedLibsListed = false;

    emit updateUI();

    return true;
}

void KDebugger::shutdown()
{
    // shut down debugger driver
    if (m_d != 0 && m_d->isRunning())
    {
	stopDriver();
    }
}

void KDebugger::useCoreFile(QString corefile, bool batch)
{
    m_corefile = corefile;
    if (!batch) {
	CmdQueueItem* cmd = loadCoreFile();
	cmd->m_byUser = true;
    }
}

void KDebugger::setAttachPid(const QString& pid)
{
    m_attachedPid = pid;
}

void KDebugger::programRun()
{
    if (!isReady())
	return;

    // when program is active, but not a core file, continue
    // otherwise run the program
    if (m_programActive && m_corefile.isEmpty()) {
	// gdb command: continue
	m_d->executeCmdOnce(DCcont);
    } else {
	// gdb command: run
	m_d->executeCmdOnce(DCrun);
	m_corefile = QString();
	m_programActive = true;
    }
    m_programRunning = true;
}

void KDebugger::attachProgram(const QString& pid)
{
    if (!isReady())
	return;

    m_attachedPid = pid;
    TRACE("Attaching to " + m_attachedPid);
    m_d->executeCmd(DCattach, m_attachedPid);
    m_programActive = true;
    m_programRunning = true;
}

void KDebugger::programRunAgain()
{
    if (canSingleStep()) {
	m_d->executeCmdOnce(DCrun);
	m_corefile = QString();
	m_programRunning = true;
    }
}

void KDebugger::programStep()
{
    if (canSingleStep()) {
	m_d->executeCmdOnce(DCstep);
	m_programRunning = true;
    }
}

void KDebugger::programNext()
{
    if (canSingleStep()) {
	m_d->executeCmdOnce(DCnext);
	m_programRunning = true;
    }
}

void KDebugger::programStepi()
{
    if (canSingleStep()) {
	m_d->executeCmdOnce(DCstepi);
	m_programRunning = true;
    }
}

void KDebugger::programNexti()
{
    if (canSingleStep()) {
	m_d->executeCmdOnce(DCnexti);
	m_programRunning = true;
    }
}

void KDebugger::programFinish()
{
    if (canSingleStep()) {
	m_d->executeCmdOnce(DCfinish);
	m_programRunning = true;
    }
}

void KDebugger::programKill()
{
    if (haveExecutable() && isProgramActive()) {
	if (m_programRunning) {
	    m_d->interruptInferior();
	}
	// this is an emergency command; flush queues
	m_d->flushCommands(true);
	m_d->executeCmdOnce(DCkill);
    }
}

void KDebugger::programDetach()
{
    if (haveExecutable() && isProgramActive()) {
	if (m_programRunning) {
	    m_d->interruptInferior();
	}
	// this is an emergency command; flush queues
	m_d->flushCommands(true);
	m_d->executeCmdOnce(DCdetach);
    }
}

bool KDebugger::runUntil(const QString& fileName, int lineNo)
{
    if (isReady() && m_programActive && !m_programRunning) {
	// strip off directory part of file name
	QFileInfo fi(fileName);
	m_d->executeCmdOnce(DCuntil, fi.fileName(), lineNo);
	m_programRunning = true;
	return true;
    } else {
	return false;
    }
}

void KDebugger::programBreak()
{
    if (m_haveExecutable && m_programRunning) {
	m_d->interruptInferior();
    }
}

bool KDebugger::isCpuTargetX86() const
{
    return m_cpuTarget.indexOf(QLatin1String("86")) >= 0;
}

void KDebugger::programArgs(QWidget* parent)
{
    if (m_haveExecutable) {
	PgmArgs dlg(parent, m_executable, m_envVars);
	dlg.setArgs(m_programArgs);
	dlg.setWd(m_programWD);
	if (dlg.exec()) {
	    updateProgEnvironment(dlg.args(), dlg.wd(), dlg.envVars());
	}
    }
}

void KDebugger::programSettings(QWidget* parent)
{
    if (!m_haveExecutable)
	return;

    ProgramSettings dlg(parent, m_executable);

    dlg.m_chooseDriver.setDebuggerCmd(m_debuggerCmd);
    dlg.m_chooseDriver.setDisassemblyFlavor(m_flavor);
    dlg.m_chooseDriver.setIsX86(isCpuTargetX86());
    dlg.m_output.setTTYLevel(m_ttyLevel);

    if (dlg.exec() == QDialog::Accepted)
    {
	m_debuggerCmd = dlg.m_chooseDriver.debuggerCmd();
	m_flavor = dlg.m_chooseDriver.disassemblyFlavor();
	m_ttyLevel = TTYLevel(dlg.m_output.ttyLevel());

	submitDisassemblyFlavor();
    }
}

bool KDebugger::setBreakpoint(QString file, int lineNo,
			      const DbgAddr& address, bool temporary)
{
    if (!isReady()) {
	return false;
    }

    BrkptIterator bp = breakpointByFilePos(file, lineNo, address);
    if (bp == m_brkpts.end())
    {
	/*
	 * No such breakpoint, so set a new one. If we have an address, we
	 * set the breakpoint exactly there. Otherwise we use the file name
	 * plus line no.
	 */
	Breakpoint* bp = new Breakpoint;
	bp->temporary = temporary;

	if (address.isEmpty())
	{
	    bp->fileName = file;
	    bp->lineNo = lineNo;
	}
	else
	{
	    bp->address = address;
	}
	setBreakpoint(bp, false);
    }
    else
    {
	/*
	 * If the breakpoint is disabled, enable it; if it's enabled,
	 * delete that breakpoint.
	 */
	if (bp->enabled) {
	    deleteBreakpoint(bp);
	} else {
	    enableDisableBreakpoint(bp);
	}
    }
    return true;
}

void KDebugger::setBreakpoint(Breakpoint* bp, bool queueOnly)
{
    CmdQueueItem* cmd = executeBreakpoint(bp, queueOnly);
    cmd->m_brkpt = bp;	// used in newBreakpoint()
}

CmdQueueItem* KDebugger::executeBreakpoint(const Breakpoint* bp, bool queueOnly)
{
    CmdQueueItem* cmd;
    if (!bp->text.isEmpty())
    {
	/*
	 * The breakpoint was set using the text box in the breakpoint
	 * list. This is the only way in which watchpoints are set.
	 */
	if (bp->type == Breakpoint::watchpoint) {
	    cmd = m_d->executeCmd(DCwatchpoint, bp->text);
	} else {
	    cmd = m_d->executeCmd(DCbreaktext, bp->text);
	}
    }
    else if (bp->address.isEmpty())
    {
	// strip off directory part of file name
	QString file = QFileInfo(bp->fileName).fileName();
	if (queueOnly) {
	    cmd = m_d->queueCmd(bp->temporary ? DCtbreakline : DCbreakline,
				file, bp->lineNo);
	} else {
	    cmd = m_d->executeCmd(bp->temporary ? DCtbreakline : DCbreakline,
				  file, bp->lineNo);
	}
    }
    else
    {
	if (queueOnly) {
	    cmd = m_d->queueCmd(bp->temporary ? DCtbreakaddr : DCbreakaddr,
				bp->address.asString());
	} else {
	    cmd = m_d->executeCmd(bp->temporary ? DCtbreakaddr : DCbreakaddr,
				  bp->address.asString());
	}
    }
    return cmd;
}

bool KDebugger::infoLine(QString file, int lineNo, const DbgAddr& addr)
{
    if (isReady() && !m_programRunning) {
	CmdQueueItem* cmd = m_d->executeCmd(DCinfoline, file, lineNo);
	cmd->m_addr = addr;
	return true;
    } else {
	return false;
    }
}

bool KDebugger::enableDisableBreakpoint(QString file, int lineNo,
					const DbgAddr& address)
{
    BrkptIterator bp = breakpointByFilePos(file, lineNo, address);
    return enableDisableBreakpoint(bp);
}

bool KDebugger::enableDisableBreakpoint(BrkptIterator bp)
{
    if (bp == m_brkpts.end())
	return false;

    /*
     * Toggle enabled/disabled state.
     * 
     * The driver is not bothered if we are modifying an orphaned
     * breakpoint.
     */
    if (!bp->isOrphaned()) {
	if (!canChangeBreakpoints()) {
	    return false;
	}
	m_d->executeCmd(bp->enabled ? DCdisable : DCenable, bp->id);
    } else {
	bp->enabled = !bp->enabled;
	emit breakpointsChanged();
    }
    return true;
}

bool KDebugger::conditionalBreakpoint(BrkptIterator bp,
				      const QString& condition,
				      int ignoreCount)
{
    if (bp == m_brkpts.end())
	return false;

    /*
     * Change the condition and ignore count.
     *
     * The driver is not bothered if we are removing an orphaned
     * breakpoint.
     */

    if (!bp->isOrphaned()) {
	if (!canChangeBreakpoints()) {
	    return false;
	}

	bool changed = false;

	if (bp->condition != condition) {
	    // change condition
	    m_d->executeCmd(DCcondition, condition, bp->id);
	    changed = true;
	}
	if (bp->ignoreCount != ignoreCount) {
	    // change ignore count
	    m_d->executeCmd(DCignore, bp->id, ignoreCount);
	    changed = true;
	}
	if (changed) {
	    // get the changes
	    m_d->queueCmd(DCinfobreak);
	}
    } else {
	bp->condition = condition;
	bp->ignoreCount = ignoreCount;
	emit breakpointsChanged();
    }
    return true;
}

bool KDebugger::deleteBreakpoint(BrkptIterator bp)
{
    if (bp == m_brkpts.end())
	return false;

    /*
     * Remove the breakpoint.
     *
     * The driver is not bothered if we are removing an orphaned
     * breakpoint.
     */
    if (!bp->isOrphaned()) {
	if (!canChangeBreakpoints()) {
	    return false;
	}
	m_d->executeCmd(DCdelete, bp->id);
    } else {
	m_brkpts.erase(bp);
	emit breakpointsChanged();
    }
    return false;
}

bool KDebugger::canSingleStep()
{
    return isReady() && m_programActive && !m_programRunning;
}

bool KDebugger::canChangeBreakpoints()
{
    return isReady() && !m_programRunning;
}

bool KDebugger::canStart()
{
    return isReady() && !m_programActive;
}

bool KDebugger::isReady() const 
{
    return m_haveExecutable &&
	m_d != 0 && m_d->canExecuteImmediately();
}

bool KDebugger::isIdle() const
{
    return m_d == 0 || m_d->isIdle();
}


//////////////////////////////////////////////////////////
// debugger driver

bool KDebugger::startDriver()
{
    emit debuggerStarting();		/* must set m_inferiorTerminal */

    /*
     * If the per-program command string is empty, use the global setting
     * (which might also be empty, in which case the driver uses its
     * default).
     */
    m_explicitKill = false;
    if (!m_d->startup(m_debuggerCmd)) {
	return false;
    }

    /*
     * If we have an output terminal, we use it. Otherwise we will run the
     * program with input and output redirected to /dev/null. Other
     * redirections are also necessary depending on the tty emulation
     * level.
     */
    int redirect = RDNstdin|RDNstdout|RDNstderr;	/* redirect everything */
    if (!m_inferiorTerminal.isEmpty()) {
	switch (m_ttyLevel) {
	default:
	case ttyNone:
	    // redirect everything
	    break;
	case ttySimpleOutputOnly:
	    redirect = RDNstdin;
	    break;
	case ttyFull:
	    redirect = 0;
	    break;
	}
    }
    m_d->executeCmd(DCtty, m_inferiorTerminal, redirect);

    return true;
}

void KDebugger::stopDriver()
{
    m_explicitKill = true;

    if (m_attachedPid.isEmpty()) {
	m_d->terminate();
    } else {
	m_d->detachAndTerminate();
    }

    /*
     * We MUST wait until the slot gdbExited() has been called. But to
     * avoid a deadlock, we wait only for some certain maximum time. Should
     * this timeout be reached, the only reasonable thing one could do then
     * is exiting kdbg.
     */
    QApplication::processEvents(QEventLoop::AllEvents, 1000);
    int maxTime = 20;			/* about 20 seconds */
    while (m_haveExecutable && maxTime > 0) {
	// give gdb time to die (and send a SIGCLD)
	::sleep(1);
	--maxTime;
	QApplication::processEvents(QEventLoop::AllEvents, 1000);
    }
}

void KDebugger::gdbExited()
{
    /*
     * Save settings, but only if gdb has already processed "info line
     * main", otherwise we would save an empty config file, because it
     * isn't read in until then!
     */
    if (m_programConfig != 0) {
	if (m_haveExecutable) {
	    saveProgramSettings();
	    m_programConfig->sync();
	}
	delete m_programConfig;
	m_programConfig = 0;
    }

    // erase types
    delete m_typeTable;
    m_typeTable = 0;

    if (m_explicitKill) {
	TRACE(m_d->driverName() + " exited normally");
    } else {
	QString msg = i18n("%1 exited unexpectedly.\n"
			   "Restart the session (e.g. with File|Load Executable).");
	KMessageBox::error(parentWidget(), msg.arg(m_d->driverName()));
    }

    // reset state
    m_haveExecutable = false;
    m_executable = "";
    m_programActive = false;
    m_programRunning = false;
    m_explicitKill = false;
    m_debuggerCmd = QString();		/* use global setting at next start! */
    m_attachedPid = QString();		/* we are no longer attached to a process */
    m_ttyLevel = ttyFull;
    m_brkpts.clear();

    // erase PC
    emit updatePC(QString(), -1, DbgAddr(), 0);
}

QString KDebugger::getConfigForExe(const QString& name)
{
    QFileInfo fi(name);
    QString dir = fi.absolutePath();

    // The session file for the given executable consists of
    // a hash of the directory, followed by the program name.
    // Assume that the first 15 positions of the hash are unique;
    // this keeps the file names short.
    QCryptographicHash dirDigest(QCryptographicHash::Md5);
    dirDigest.addData(dir.toUtf8());
    QString hash = dirDigest.result().toBase64();
    hash.replace('/', QString());	// avoid directory separators
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
		+ "/sessions/"
		+ hash.left(15) + "-" + fi.fileName();
}

void KDebugger::openProgramConfig(const QString& name)
{
    ASSERT(m_programConfig == 0);

    QString pgmConfigFile = getConfigForExe(name);

    m_programConfig = new KConfig(pgmConfigFile);

    // this leaves a clue behind in the config file which
    // executable it applies to; it is mostly intended for
    // users peeking into the file
    KConfigGroup g = m_programConfig->group(GeneralGroup);
    g.writeEntry("ExecutableFile", name);
}

const char EnvironmentGroup[] = "Environment";
const char WatchGroup[] = "Watches";
const char FileVersion[] = "FileVersion";
const char ProgramArgs[] = "ProgramArgs";
const char WorkingDirectory[] = "WorkingDirectory";
const char Variable[] = "Var%d";
const char Value[] = "Value%d";
const char ExprFmt[] = "Expr%d";

void KDebugger::saveProgramSettings()
{
    ASSERT(m_programConfig != 0);
    KConfigGroup gg = m_programConfig->group(GeneralGroup);
    gg.writeEntry(FileVersion, 1);
    gg.writeEntry(ProgramArgs, m_programArgs);
    gg.writeEntry(WorkingDirectory, m_programWD);
    gg.writeEntry(DebuggerCmdStr, m_debuggerCmd);
    gg.writeEntry(TTYLevelEntry, int(m_ttyLevel));
    gg.writeEntry(DisassemblyFlavor, m_flavor);

    QString driverName;
    if (m_d != 0)
	driverName = m_d->driverName();
    gg.writeEntry(DriverNameEntry, driverName);

    // write environment variables
    m_programConfig->deleteGroup(EnvironmentGroup);
    KConfigGroup eg = m_programConfig->group(EnvironmentGroup);
    QString varName;
    QString varValue;
    int i = 0;
    for (std::map<QString,QString>::iterator it = m_envVars.begin(); it != m_envVars.end(); ++it, ++i)
    {
	varName = QString::asprintf(Variable, i);
	varValue = QString::asprintf(Value, i);
	eg.writeEntry(varName, it->first);
	eg.writeEntry(varValue, it->second);
    }

    saveBreakpoints(m_programConfig);

    // watch expressions
    // first get rid of whatever was in this group
    m_programConfig->deleteGroup(WatchGroup);
    // then start a new group
    KConfigGroup wg = m_programConfig->group(WatchGroup);
    int watchNum = 0;
    foreach (QString expr, m_watchVariables.exprList()) {
	varName = QString::asprintf(ExprFmt, watchNum++);
	wg.writeEntry(varName, expr);
    }

    // give others a chance
    emit saveProgramSpecific(m_programConfig);
}

void KDebugger::overrideProgramArguments(const QString& args)
{
    ASSERT(m_programConfig != 0);
    KConfigGroup g = m_programConfig->group(GeneralGroup);
    g.writeEntry(ProgramArgs, args);
}

void KDebugger::restoreProgramSettings()
{
    ASSERT(m_programConfig != 0);
    KConfigGroup gg = m_programConfig->group(GeneralGroup);
    /*
     * We ignore file version for now we will use it in the future to
     * distinguish different versions of this configuration file.
     */
    // m_debuggerCmd has been read in already
    // m_ttyLevel has been read in already
    QString pgmArgs = gg.readEntry(ProgramArgs);
    QString pgmWd = gg.readEntry(WorkingDirectory);
    m_flavor = gg.readEntry(DisassemblyFlavor, QString{});

    // read environment variables
    KConfigGroup eg = m_programConfig->group(EnvironmentGroup);
    m_envVars.clear();
    std::map<QString,EnvVar> pgmVars;
    QString varName;
    QString varValue;
    for (int i = 0;; ++i) {
	varName = QString::asprintf(Variable, i);
	varValue = QString::asprintf(Value, i);
	if (!eg.hasKey(varName)) {
	    /* entry not present, assume that we've hit them all */
	    break;
	}
	QString name = eg.readEntry(varName, QString());
	if (name.isEmpty()) {
	    // skip empty names
	    continue;
	}
	EnvVar* var = &pgmVars[name];
	var->value = eg.readEntry(varValue, QString());
	var->status = EnvVar::EVnew;
    }

    submitDisassemblyFlavor();

    updateProgEnvironment(pgmArgs, pgmWd, pgmVars);

    restoreBreakpoints(m_programConfig);

    // watch expressions
    KConfigGroup wg = m_programConfig->group(WatchGroup);
    m_watchVariables.clear();
    for (int i = 0;; ++i) {
	varName = QString::asprintf(ExprFmt, i);
	if (!wg.hasKey(varName)) {
	    /* entry not present, assume that we've hit them all */
	    break;
	}
	QString expr = wg.readEntry(varName, QString());
	if (expr.isEmpty()) {
	    // skip empty expressions
	    continue;
	}
	addWatch(expr);
    }

    // give others a chance
    emit restoreProgramSpecific(m_programConfig);
}

/*
 * Breakpoints are saved one per group.
 */
const char BPGroup[] = "Breakpoint %d";
const char File[] = "File";
const char Line[] = "Line";
const char Text[] = "Text";
const char Address[] = "Address";
const char Temporary[] = "Temporary";
const char Enabled[] = "Enabled";
const char Condition[] = "Condition";

void KDebugger::saveBreakpoints(KConfig* config)
{
    QString groupName;
    int i = 0;
    for (BrkptIterator bp = m_brkpts.begin(); bp != m_brkpts.end(); ++bp)
    {
	if (bp->type == Breakpoint::watchpoint)
	    continue;			/* don't save watchpoints */
	groupName = QString::asprintf(BPGroup, i++);

	/* remove remmants */
	config->deleteGroup(groupName);

	KConfigGroup g = config->group(groupName);
	if (!bp->text.isEmpty()) {
	    /*
	     * The breakpoint was set using the text box in the breakpoint
	     * list. We do not save the location by filename+line number,
	     * but instead honor what the user typed (a function name, for
	     * example, which could move between sessions).
	     */
	    g.writeEntry(Text, bp->text);
	} else if (!bp->fileName.isEmpty()) {
	    g.writeEntry(File, bp->fileName);
	    g.writeEntry(Line, bp->lineNo);
	    /*
	     * Addresses are hardly correct across sessions, so we don't
	     * save it.
	     */
	} else {
	    g.writeEntry(Address, bp->address.asString());
	}
	g.writeEntry(Temporary, bp->temporary);
	g.writeEntry(Enabled, bp->enabled);
	if (!bp->condition.isEmpty())
	    g.writeEntry(Condition, bp->condition);
	// we do not save the ignore count
    }
    // delete remaining groups
    // we recognize that a group is present if there is an Enabled entry
    for (;; i++) {
	groupName = QString::asprintf(BPGroup, i);
	if (!config->group(groupName).hasKey(Enabled)) {
	    /* group not present, assume that we've hit them all */
	    break;
	}
	config->deleteGroup(groupName);
    }
}

void KDebugger::restoreBreakpoints(KConfig* config)
{
    QString groupName;
    /*
     * We recognize the end of the list if there is no Enabled entry
     * present.
     */
    for (int i = 0;; i++) {
	groupName = QString::asprintf(BPGroup, i);
	KConfigGroup g = config->group(groupName);
	if (!g.hasKey(Enabled)) {
	    /* group not present, assume that we've hit them all */
	    break;
	}
	Breakpoint* bp = new Breakpoint;
	bp->fileName = g.readEntry(File);
	bp->lineNo = g.readEntry(Line, -1);
	bp->text = g.readEntry(Text);
	bp->address = g.readEntry(Address);
	// check consistency
	if ((bp->fileName.isEmpty() || bp->lineNo < 0) &&
	    bp->text.isEmpty() &&
	    bp->address.isEmpty())
	{
	    delete bp;
	    continue;
	}
	bp->enabled = g.readEntry(Enabled, true);
	bp->temporary = g.readEntry(Temporary, false);
	bp->condition = g.readEntry(Condition);

	/*
	 * Add the breakpoint.
	 */
	setBreakpoint(bp, false);
	// the new breakpoint is disabled or conditionalized later
	// in newBreakpoint()
    }
    m_d->queueCmd(DCinfobreak);
}


// parse output of command cmd
void KDebugger::parse(CmdQueueItem* cmd, const char* output)
{
    ASSERT(cmd != 0);			/* queue mustn't be empty */

    TRACE(QString(__PRETTY_FUNCTION__) + " parsing " + output);

    switch (cmd->m_cmd) {
    case DCtargetremote:
	// the output (if any) is uninteresting
    case DCsetargs:
    case DCtty:
	// there is no output
    case DCsetenv:
    case DCunsetenv:
	/* if value is empty, we see output, but we don't care */
	break;
    case DCcd:
	/* display gdb's message in the status bar */
	m_d->parseChangeWD(output, m_statusMessage);
	emit updateStatusMessage();
	break;
    case DCinitialize:
	break;
    case DCexecutable:
	if (m_d->parseChangeExecutable(output, m_statusMessage))
	{
	    // success; restore breakpoints etc.
	    if (m_programConfig != 0) {
		restoreProgramSettings();
	    }
	    // load file containing main() or core file
	    if (!m_corefile.isEmpty())
	    {
		// load core file
		loadCoreFile();
	    }
	    else if (!m_attachedPid.isEmpty())
	    {
		m_d->queueCmd(DCattach, m_attachedPid);
		m_programActive = true;
		m_programRunning = true;
	    }
	    else if (!m_remoteDevice.isEmpty())
	    {
		// handled elsewhere
	    }
	    else
	    {
		m_d->queueCmdAgain(DCinfolinemain);
	    }
	    if (!m_statusMessage.isEmpty())
		emit updateStatusMessage();
	} else {
	    QString msg = m_d->driverName() + ": " + m_statusMessage;
	    KMessageBox::sorry(parentWidget(), msg);
	    m_executable = "";
	    m_corefile = "";		/* don't process core file */
	    m_haveExecutable = false;
	}
	break;
    case DCcorefile:
	// in any event we have an executable at this point
	m_haveExecutable = true;
	if (m_d->parseCoreFile(output)) {
	    // loading a core is like stopping at a breakpoint
	    m_programActive = true;
	    handleRunCommands(output);
	    // do not reset m_corefile
	} else {
	    // report error
	    QString msg = m_d->driverName() + ": " + QString(output);
	    KMessageBox::sorry(parentWidget(), msg);

	    // if core file was loaded from command line, revert to info line main
	    if (!cmd->m_byUser) {
		m_d->queueCmdAgain(DCinfolinemain);
	    }
	    m_corefile = QString();	/* core file not available any more */
	}
	break;
    case DCinfolinemain:
	// ignore the output, marked file info follows
	m_haveExecutable = true;
	break;
    case DCinfolocals:
	// parse local variables
	if (output[0] != '\0') {
	    handleLocals(output);
	}
	break;
    case DCinforegisters:
	handleRegisters(output);
	break;
    case DCexamine:
	handleMemoryDump(output);
	break;
    case DCinfoline:
	handleInfoLine(cmd, output);
	break;
    case DCinfotarget:
	handleInfoTarget(output);
	break;
    case DCdisassemble:
	handleDisassemble(cmd, output);
	break;
    case DCsetdisassflavor:
	handleSetDisassFlavor(output);
	break;
    case DCframe:
	handleFrameChange(output);
	updateAllExprs();
	break;
    case DCbt:
	handleBacktrace(output);
	updateAllExprs();
	break;
    case DCprint:
	handlePrint(cmd, output);
	break;
    case DCprintPopup:
	handlePrintPopup(cmd, output);
	break;
    case DCprintDeref:
	handlePrintDeref(cmd, output);
	break;
    case DCattach:
	m_haveExecutable = true;
	// fall through
    case DCrun:
    case DCcont:
    case DCstep:
    case DCstepi:
    case DCnext:
    case DCnexti:
    case DCfinish:
    case DCuntil:
    case DCthread:
	handleRunCommands(output);
	break;
    case DCkill:
    case DCdetach:
	m_programRunning = m_programActive = false;
	// erase PC
	emit updatePC(QString(), -1, DbgAddr(), 0);
	break;
    case DCbreaktext:
    case DCbreakline:
    case DCtbreakline:
    case DCbreakaddr:
    case DCtbreakaddr:
    case DCwatchpoint:
	newBreakpoint(cmd, output);
	// fall through
    case DCdelete:
    case DCenable:
    case DCdisable:
	// these commands need immediate response
	m_d->queueCmdPrio(DCinfobreak);
	break;
    case DCinfobreak:
	// note: this handler must not enqueue a command, since
	// DCinfobreak is used at various different places.
	updateBreakList(output);
	break;
    case DCfindType:
	handleFindType(cmd, output);
	break;
    case DCprintStruct:
    case DCprintQStringStruct:
    case DCprintWChar:
	handlePrintStruct(cmd, output);
	break;
    case DCinfosharedlib:
	handleSharedLibs(output);
	break;
    case DCcondition:
    case DCignore:
	// we are not interested in the output
	break;
    case DCinfothreads:
	handleThreadList(output);
	break;
    case DCsetpc:
	handleSetPC(output);
	break;
    case DCsetvariable:
	handleSetVariable(cmd, output);
	break;
    }
}

void KDebugger::backgroundUpdate()
{
    /*
     * If there are still expressions that need to be updated, then do so.
     */
    if (m_programActive)
	evalExpressions();
}

void KDebugger::handleRunCommands(const char* output)
{
    uint flags = m_d->parseProgramStopped(output, !m_corefile.isEmpty(),
					  m_statusMessage);
    emit updateStatusMessage();

    m_programActive = flags & DebuggerDriver::SFprogramActive;

    // refresh files if necessary
    if (flags & DebuggerDriver::SFrefreshSource) {
	TRACE("re-reading files");
	emit executableUpdated();
    }

    /* 
     * Try to set any orphaned breakpoints now.
     */
    for (BrkptIterator bp = m_brkpts.begin(); bp != m_brkpts.end(); ++bp)
    {
	if (bp->isOrphaned()) {
	    TRACE(QString("re-trying brkpt loc: %2 file: %3 line: %1")
		    .arg(bp->lineNo).arg(bp->location, bp->fileName));
	    CmdQueueItem* cmd = executeBreakpoint(&*bp, true);
	    cmd->m_existingBrkpt = bp->id;	// used in newBreakpoint()
	    flags |= DebuggerDriver::SFrefreshBreak;
	}
    }

    /*
     * If we stopped at a breakpoint, we must update the breakpoint list
     * because the hit count changes. Also, if the breakpoint was temporary
     * it would go away now.
     */
    if ((flags & (DebuggerDriver::SFrefreshBreak|DebuggerDriver::SFrefreshSource)) ||
	stopMayChangeBreakList())
    {
	m_d->queueCmd(DCinfobreak);
    }

    /*
     * If we haven't listed the shared libraries yet, do so. We must do
     * this before we emit any commands that list variables, since the type
     * libraries depend on the shared libraries.
     */
    if (!m_sharedLibsListed) {
	// must be a high-priority command!
	m_d->executeCmd(DCinfosharedlib);
    }

    // get the backtrace if the program is running
    if (m_programActive) {
	m_d->queueCmd(DCbt);
    } else {
	// program finished: erase PC
	emit updatePC(QString(), -1, DbgAddr(), 0);
	// dequeue any commands in the queues
	m_d->flushCommands();
    }

    /* Update threads list */
    if (m_programActive && (flags & DebuggerDriver::SFrefreshThreads)) {
	m_d->queueCmd(DCinfothreads);
    }

    m_programRunning = false;
    emit programStopped();
}

void KDebugger::slotInferiorRunning()
{
    m_programRunning = true;
}

void KDebugger::updateAllExprs()
{
    if (!m_programActive)
	return;

    // retrieve local variables
    m_d->queueCmd(DCinfolocals);

    // retrieve registers
    m_d->queueCmd(DCinforegisters);

    // get new memory dump
    if (!m_memoryStartExpression.isEmpty()) {
	queueMemoryDump(false, true);
    }

    // update watch expressions
    foreach (QString expr, m_watchVariables.exprList()) {
	m_watchEvalExpr.push_back(expr);
    }
}

void KDebugger::updateProgEnvironment(const QString& args, const QString& wd,
				      const std::map<QString,EnvVar>& newVars)
{
    m_programArgs = args;
    m_d->executeCmd(DCsetargs, m_programArgs);
    TRACE("new pgm args: " + m_programArgs + "\n");

    m_programWD = wd.trimmed();
    if (!m_programWD.isEmpty()) {
	m_d->executeCmd(DCcd, m_programWD);
	TRACE("new wd: " + m_programWD + "\n");
    }

    // update environment variables
    for (std::map<QString,EnvVar>::const_iterator i = newVars.begin(); i != newVars.end(); ++i)
    {
	QString var = i->first;
	const EnvVar* val = &i->second;
	switch (val->status) {
	case EnvVar::EVnew:
	case EnvVar::EVdirty:
	    m_envVars[var] = val->value;
	    // update value
	    m_d->executeCmd(DCsetenv, var, val->value);
	    break;
	case EnvVar::EVdeleted:
	    // delete value
	    m_d->executeCmd(DCunsetenv, var);
	    m_envVars.erase(var);
	    break;
	default:
	    ASSERT(false);
	case EnvVar::EVclean:
	    // variable not changed
	    break;
	}
    }
}

void KDebugger::handleLocals(const char* output)
{
    // retrieve old list of local variables
    QStringList oldVars = m_localVariables.exprList();

    /*
     *  Get local variables.
     */
    std::list<ExprValue*> newVars;
    parseLocals(output, newVars);

    /*
     * Clear any old VarTree item pointers, so that later we don't access
     * dangling pointers.
     */
    m_localVariables.clearPendingUpdates();

    /*
     * Match old variables against new ones.
     */
    for (QStringList::ConstIterator n = oldVars.begin(); n != oldVars.end(); ++n) {
	// lookup this variable in the list of new variables
	std::list<ExprValue*>::iterator v = newVars.begin();
	while (v != newVars.end() && (*v)->m_name != *n)
	    ++v;
	if (v == newVars.end()) {
	    // old variable not in the new variables
	    TRACE("old var deleted: " + *n);
	    VarTree* v = m_localVariables.topLevelExprByName(*n);
	    if (v != 0) {
		m_localVariables.removeExpr(v);
	    }
	} else {
	    // variable in both old and new lists: update
	    TRACE("update var: " + *n);
	    m_localVariables.updateExpr(*v, *m_typeTable);
	    // remove the new variable from the list
	    delete *v;
	    newVars.erase(v);
	}
    }
    // insert all remaining new variables
    while (!newVars.empty())
    {
	ExprValue* v = newVars.front();
	TRACE("new var: " + v->m_name);
	m_localVariables.insertExpr(v, *m_typeTable);
	delete v;
	newVars.pop_front();
    }
}

void KDebugger::parseLocals(const char* output, std::list<ExprValue*>& newVars)
{
    std::list<ExprValue*> vars;
    m_d->parseLocals(output, vars);

    QString origName;			/* used in renaming variables */
    while (!vars.empty())
    {
	ExprValue* variable = vars.front();
	vars.pop_front();
	/*
	 * When gdb prints local variables, those from the innermost block
	 * come first. We run through the list of already parsed variables
	 * to find duplicates (ie. variables that hide local variables from
	 * a surrounding block). We keep the name of the inner variable, but
	 * rename those from the outer block so that, when the value is
	 * updated in the window, the value of the variable that is
	 * _visible_ changes the color!
	 */
	int block = 0;
	origName = variable->m_name;
	for (std::list<ExprValue*>::iterator v = newVars.begin(); v != newVars.end(); ++v) {
	    if (variable->m_name == (*v)->m_name) {
		// we found a duplicate, change name
		block++;
		QString newName = origName + " (" + QString().setNum(block) + ")";
		variable->m_name = newName;
	    }
	}
	newVars.push_back(variable);
    }
}

bool KDebugger::handlePrint(CmdQueueItem* cmd, const char* output)
{
    ASSERT(cmd->m_expr != 0);

    ExprValue* variable = m_d->parsePrintExpr(output, true);
    if (variable == 0)
	return false;

    // set expression "name"
    variable->m_name = cmd->m_expr->getText();

    {
	TRACE("update expr: " + cmd->m_expr->getText());
	cmd->m_exprWnd->updateExpr(cmd->m_expr, variable, *m_typeTable);
	delete variable;
    }

    evalExpressions();			/* enqueue dereferenced pointers */

    return true;
}

bool KDebugger::handlePrintPopup(CmdQueueItem* cmd, const char* output)
{
    ExprValue* value = m_d->parsePrintExpr(output, false);
    if (value == 0)
	return false;

    TRACE("<" + cmd->m_popupExpr + "> = " + value->m_value);

    // construct the tip, m_popupExpr contains the variable name
    QString tip = cmd->m_popupExpr + " = " + formatPopupValue(value);
    emit valuePopup(tip);

    return true;
}

bool KDebugger::handlePrintDeref(CmdQueueItem* cmd, const char* output)
{
    ASSERT(cmd->m_expr != 0);

    ExprValue* variable = m_d->parsePrintExpr(output, true);
    if (variable == 0)
	return false;

    // set expression "name"
    variable->m_name = cmd->m_expr->getText();

    {
	/*
	 * We must insert a dummy parent, because otherwise variable's value
	 * would overwrite cmd->m_expr's value.
	 */
	ExprValue* dummyParent = new ExprValue(variable->m_name, VarTree::NKplain);
	dummyParent->m_varKind = VarTree::VKdummy;
	// the name of the parsed variable is the address of the pointer
	QString addr = "*" + cmd->m_expr->value();
	variable->m_name = addr;
	variable->m_nameKind = VarTree::NKaddress;

	dummyParent->m_child = variable;
	// expand the first level for convenience
	variable->m_initiallyExpanded = true;
	TRACE("update ptr: " + cmd->m_expr->getText());
	cmd->m_exprWnd->updateExpr(cmd->m_expr, dummyParent, *m_typeTable);
	delete dummyParent;
    }

    evalExpressions();			/* enqueue dereferenced pointers */

    return true;
}

// parse the output of bt
void KDebugger::handleBacktrace(const char* output)
{
    m_btWindow.clear();

    std::list<StackFrame> stack;
    m_d->parseBackTrace(output, stack);

    if (!stack.empty()) {
	std::list<StackFrame>::iterator frm = stack.begin();
	// first frame must set PC
	// note: frm->lineNo is zero-based
	emit updatePC(frm->fileName, frm->lineNo, frm->address, frm->frameNo);

	for (; frm != stack.end(); ++frm) {
	    QString func;
	    if (frm->var != 0)
		func = frm->var->m_name;
	    else
		func = frm->fileName + ":" + QString().setNum(frm->lineNo+1);
        
 	    m_btWindow.addItem(func);
	    TRACE("frame " + func + " (" + frm->fileName + ":" +
		  QString().setNum(frm->lineNo+1) + ")");
	}
    }

}

void KDebugger::gotoFrame(int frame)
{
    // frame is negative when the backtrace list is cleared
    // ignore this event
    if (frame < 0)
	return;
    m_d->executeCmd(DCframe, frame);
}

void KDebugger::handleFrameChange(const char* output)
{
    QString fileName;
    int frameNo;
    int lineNo;
    DbgAddr address;
    if (m_d->parseFrameChange(output, frameNo, fileName, lineNo, address)) {
	/* lineNo can be negative here if we can't find a file name */
	emit updatePC(fileName, lineNo, address, frameNo);
    } else {
	emit updatePC(fileName, -1, address, frameNo);
    }
}

void KDebugger::evalExpressions()
{
    // evaluate expressions in the following order:
    //   watch expressions
    //   pointers in local variables
    //   pointers in watch expressions
    //   types in local variables
    //   types in watch expressions
    //   struct members in local variables
    //   struct members in watch expressions
    VarTree* exprItem = 0;
    if (!m_watchEvalExpr.empty())
    {
	QString expr = m_watchEvalExpr.front();
	m_watchEvalExpr.pop_front();
	exprItem = m_watchVariables.topLevelExprByName(expr);
    }
    if (exprItem != 0) {
	CmdQueueItem* cmd = m_d->queueCmd(DCprint, exprItem->getText());
	// remember which expr this was
	cmd->m_expr = exprItem;
	cmd->m_exprWnd = &m_watchVariables;
    } else {
	ExprWnd* wnd;
#define POINTER(widget) \
		wnd = &widget; \
		exprItem = widget.nextUpdatePtr(); \
		if (exprItem != 0) goto pointer
#define STRUCT(widget) \
		wnd = &widget; \
		exprItem = widget.nextUpdateStruct(); \
		if (exprItem != 0) goto ustruct
#define TYPE(widget) \
		wnd = &widget; \
		exprItem = widget.nextUpdateType(); \
		if (exprItem != 0) goto type
    repeat:
	POINTER(m_localVariables);
	POINTER(m_watchVariables);
	STRUCT(m_localVariables);
	STRUCT(m_watchVariables);
	TYPE(m_localVariables);
	TYPE(m_watchVariables);
#undef POINTER
#undef STRUCT
#undef TYPE
	return;

	pointer:
	// we have an expression to send
	dereferencePointer(wnd, exprItem, false);
	return;

	ustruct:
	// paranoia
	if (exprItem->m_type == 0 || exprItem->m_type == TypeInfo::unknownType())
	    goto repeat;
	evalInitialStructExpression(exprItem, wnd, false);
	return;

	type:
	/*
	 * Sometimes a VarTree gets registered twice for a type update. So
	 * it may happen that it has already been updated. Hence, we ignore
	 * it here and go on to the next task.
	 */
	if (exprItem->m_type != 0)
	    goto repeat;
	determineType(wnd, exprItem);
    }
}

void KDebugger::dereferencePointer(ExprWnd* wnd, VarTree* exprItem,
				   bool immediate)
{
    ASSERT(exprItem->m_varKind == VarTree::VKpointer);

    QString expr = exprItem->computeExpr();
    TRACE("dereferencing pointer: " + expr);
    CmdQueueItem* cmd;
    if (immediate) {
	cmd = m_d->queueCmdPrio(DCprintDeref, expr);
    } else {
	cmd = m_d->queueCmd(DCprintDeref, expr);
    }
    // remember which expr this was
    cmd->m_expr = exprItem;
    cmd->m_exprWnd = wnd;
}

void KDebugger::determineType(ExprWnd* wnd, VarTree* exprItem)
{
    ASSERT(exprItem->m_varKind == VarTree::VKstruct);

    QString expr = exprItem->computeExpr();
    TRACE("get type of: " + expr);
    CmdQueueItem* cmd;
    cmd = m_d->queueCmd(DCfindType, expr);

    // remember which expr this was
    cmd->m_expr = exprItem;
    cmd->m_exprWnd = wnd;
}

void KDebugger::handleFindType(CmdQueueItem* cmd, const char* output)
{
    QString type;
    if (m_d->parseFindType(output, type))
    {
	ASSERT(cmd != 0 && cmd->m_expr != 0);

	const TypeInfo* info = m_typeTable->lookup(type);

	if (info == 0) {
	    /*
	     * We've asked gdb for the type of the expression in
	     * cmd->m_expr, but it returned a name we don't know. The base
	     * class (and member) types have been checked already (at the
	     * time when we parsed that particular expression). Now it's
	     * time to derive the type from the base classes as a last
	     * resort.
	     */
	    info = cmd->m_expr->inferTypeFromBaseClass();
	    // if we found a type through this method, register an alias
	    if (info != 0) {
		TRACE("infered alias: " + type);
		m_typeTable->registerAlias(type, info);
	    }
	}
	if (info == 0) {
            TRACE("unknown type "+type);
	    cmd->m_expr->m_type = TypeInfo::unknownType();
	} else {
	    cmd->m_expr->m_type = info;
	    /* since this node has a new type, we get its value immediately */
	    evalInitialStructExpression(cmd->m_expr, cmd->m_exprWnd, false);
	    return;
	}
    }

    evalExpressions();			/* queue more of them */
}

void KDebugger::handlePrintStruct(CmdQueueItem* cmd, const char* output)
{
    VarTree* var = cmd->m_expr;
    ASSERT(var != 0);
    ASSERT(var->m_varKind == VarTree::VKstruct);

    ExprValue* partExpr;
    if (cmd->m_cmd == DCprintQStringStruct) {
	partExpr = m_d->parseQCharArray(output, false, m_typeTable->qCharIsShort());
    } else if (cmd->m_cmd == DCprintWChar) {
	partExpr = m_d->parseQCharArray(output, false, true);
    } else {
	partExpr = m_d->parsePrintExpr(output, false);
    }
    bool errorValue =
	partExpr == 0 ||
	/* we only allow simple values at the moment */
	partExpr->m_child != 0;

    QString partValue;
    if (errorValue)
    {
	partValue = "?""?""?";	// 2 question marks in a row would be a trigraph
    } else {
	partValue = partExpr->m_value;
    }
    delete partExpr;
    partExpr = 0;

    /*
     * Updating a struct value works like this: var->m_partialValue holds
     * the value that we have gathered so far (it's been initialized with
     * var->m_type->m_displayString[0] earlier). Each time we arrive here,
     * we append the printed result followed by the next
     * var->m_type->m_displayString to var->m_partialValue.
     * 
     * If the expression we just evaluated was a guard expression, and it
     * resulted in an error, we must not evaluate the real expression, but
     * go on to the next index. (We must still add the question marks to
     * the value).
     * 
     * Next, if this was the length expression, we still have not seen the
     * real expression, but the length of a QString.
     */
    ASSERT(var->m_exprIndex >= 0 && var->m_exprIndex <= typeInfoMaxExpr);

    if (errorValue || !var->m_exprIndexUseGuard)
    {
	// add current partValue (which might be the question marks)
	var->m_partialValue += partValue;
	var->m_exprIndex++;		/* next part */
	var->m_exprIndexUseGuard = true;
	var->m_partialValue += var->m_type->m_displayString[var->m_exprIndex];
    }
    else
    {
	// this was a guard expression that succeeded
	// go for the real expression
	var->m_exprIndexUseGuard = false;
    }

    /* go for more sub-expressions if needed */
    if (var->m_exprIndex < var->m_type->m_numExprs) {
	/* queue a new print command with quite high priority */
	evalStructExpression(var, cmd->m_exprWnd, true);
	return;
    }

    cmd->m_exprWnd->updateStructValue(var);

    evalExpressions();			/* enqueue dereferenced pointers */
}

/* queues the first printStruct command for a struct */
void KDebugger::evalInitialStructExpression(VarTree* var, ExprWnd* wnd, bool immediate)
{
    var->m_exprIndex = 0;
    if (var->m_type != TypeInfo::wchartType())
    {
	var->m_exprIndexUseGuard = true;
	var->m_partialValue = var->m_type->m_displayString[0];
	evalStructExpression(var, wnd, immediate);
    }
    else
    {
	var->m_exprIndexUseGuard = false;
	QString expr = var->computeExpr();
	CmdQueueItem* cmd = immediate  ?
				m_d->queueCmdPrio(DCprintWChar, expr)  :
				m_d->queueCmd(DCprintWChar, expr)  ;
	// remember which expression this was
	cmd->m_expr = var;
	cmd->m_exprWnd = wnd;
    }
}

/** queues a printStruct command; var must have been initialized correctly */
void KDebugger::evalStructExpression(VarTree* var, ExprWnd* wnd, bool immediate)
{
    QString base = var->computeExpr();
    QString expr;
    if (var->m_exprIndexUseGuard) {
	expr = var->m_type->m_guardStrings[var->m_exprIndex];
	if (expr.isEmpty()) {
	    // no guard, omit it and go to expression
	    var->m_exprIndexUseGuard = false;
	}
    }
    if (!var->m_exprIndexUseGuard) {
	expr = var->m_type->m_exprStrings[var->m_exprIndex];
    }

    expr.replace("%s", base);

    DbgCommand dbgCmd = DCprintStruct;
    // check if this is a QString::Data
    if (expr.left(15) == "/QString::Data ")
    {
	if (m_typeTable->parseQt2QStrings())
	{
	    expr = expr.mid(15, expr.length());	/* strip off /QString::Data */
	    dbgCmd = DCprintQStringStruct;
	} else {
	    /*
	     * This should not happen: the type libraries should be set up
	     * in a way that this can't happen. If this happens
	     * nevertheless it means that, eg., kdecore was loaded but qt2
	     * was not (only qt2 enables the QString feature).
	     */
	    // TODO: remove this "print"; queue the next printStruct instead
	    expr = "*0";
	}
    }
    TRACE("evalStruct: " + expr + (var->m_exprIndexUseGuard ? " // guard" : " // real"));
    CmdQueueItem* cmd = immediate  ?
			m_d->queueCmdPrio(dbgCmd, expr)  :
			m_d->queueCmd(dbgCmd, expr);

    // remember which expression this was
    cmd->m_expr = var;
    cmd->m_exprWnd = wnd;
}

void KDebugger::handleSharedLibs(const char* output)
{
    // parse the table of shared libraries
    m_sharedLibs = m_d->parseSharedLibs(output);
    m_sharedLibsListed = true;

    // get type libraries
    m_typeTable->loadLibTypes(m_sharedLibs);

    // hand over the QString data cmd
    m_d->setPrintQStringDataCmd(m_typeTable->printQStringDataCmd());
}

CmdQueueItem* KDebugger::loadCoreFile()
{
    return m_d->queueCmd(DCcorefile, m_corefile);
}

void KDebugger::slotExpanding(QTreeWidgetItem* item)
{
    VarTree* exprItem = static_cast<VarTree*>(item);
    if (exprItem->m_varKind != VarTree::VKpointer) {
	return;
    }
    ExprWnd* wnd = static_cast<ExprWnd*>(item->treeWidget());
    dereferencePointer(wnd, exprItem, true);
}

// add the expression in the edit field to the watch expressions
void KDebugger::addWatch(const QString& t)
{
    QString expr = t.trimmed();
    // don't add a watched expression again
    if (expr.isEmpty() || m_watchVariables.topLevelExprByName(expr) != 0)
	return;
    ExprValue e(expr, VarTree::NKplain);
    m_watchVariables.insertExpr(&e, *m_typeTable);

    // if we are boring ourselves, send down the command
    if (m_programActive) {
	m_watchEvalExpr.push_back(expr);
	if (m_d->isIdle()) {
	    evalExpressions();
	}
    }
}

// delete a toplevel watch expression
void KDebugger::slotDeleteWatch()
{
    // delete only allowed while debugger is idle; or else we might delete
    // the very expression the debugger is currently working on...
    if (m_d == 0 || !m_d->isIdle())
	return;

    VarTree* item = m_watchVariables.selectedItem();
    if (item == 0 || !item->isToplevelExpr())
	return;

    // remove the variable from the list to evaluate
    std::list<QString>::iterator i =
	    std::find(m_watchEvalExpr.begin(), m_watchEvalExpr.end(), item->getText());
    if (i != m_watchEvalExpr.end()) {
	m_watchEvalExpr.erase(i);
    }
    m_watchVariables.removeExpr(item);
    // item is invalid at this point!
}

void KDebugger::handleRegisters(const char* output)
{
    emit registersChanged(m_d->parseRegisters(output));
}

/*
 * The output of the DCbreak* commands has more accurate information about
 * the file and the line number.
 *
 * All newly set breakpoints are inserted in the m_brkpts, even those that
 * were not set sucessfully. The unsuccessful breakpoints ("orphaned
 * breakpoints") are assigned negative ids, and they are tried to set later
 * when the program stops again at a breakpoint.
 */
void KDebugger::newBreakpoint(CmdQueueItem* cmd, const char* output)
{
    BrkptIterator bp;
    if (cmd->m_brkpt != 0) {
	// a new breakpoint, put it in the list
	assert(cmd->m_brkpt->id == 0);
	m_brkpts.push_back(*cmd->m_brkpt);
	delete cmd->m_brkpt;
	bp = m_brkpts.end();
	--bp;
    } else {
	// an existing breakpoint was retried
	assert(cmd->m_existingBrkpt != 0);
	bp = breakpointById(cmd->m_existingBrkpt);
	if (bp == m_brkpts.end())
	    return;
    }

    // parse the output to determine success or failure
    int id;
    QString file;
    int lineNo;
    QString address;
    if (!m_d->parseBreakpoint(output, id, file, lineNo, address))
    {
	/*
	 * Failure, the breakpoint could not be set. If this is a new
	 * breakpoint, assign it a negative id. We look for the minimal id
	 * of all breakpoints (that are already in the list) to get the new
	 * id.
	 */
	if (bp->id == 0)
	{
	    int minId = 0;
	    for (BrkptIterator i = m_brkpts.begin(); i != m_brkpts.end(); ++i) {
		if (i->id < minId)
		    minId = i->id;
	    }
	    bp->id = minId-1;
	}
	return;
    }

    // The breakpoint was successfully set.
    if (bp->id <= 0)
    {
	// this is a new or orphaned breakpoint:
	// set the remaining properties
	if (!bp->enabled) {
	    m_d->executeCmd(DCdisable, id);
	}
	if (!bp->condition.isEmpty()) {
	    m_d->executeCmd(DCcondition, bp->condition, id);
	}
    }

    bp->id = id;
    bp->fileName = file;
    bp->lineNo = lineNo;
    if (!address.isEmpty())
	bp->address = address;
}

void KDebugger::updateBreakList(const char* output)
{
    // get the new list
    std::list<Breakpoint> brks;
    m_d->parseBreakList(output, brks);

    // merge existing information into the new list
    // then swap the old and new lists

    for (BrkptIterator bp = brks.begin(); bp != brks.end(); ++bp)
    {
	BrkptIterator i = breakpointById(bp->id);
	if (i != m_brkpts.end())
	{
	    // preserve accurate location information
	    // note that xsldbg doesn't have a location in
	    // the listed breakpoint if it has just been set
	    // therefore, we copy it as well if necessary
	    bp->text = i->text;
	    if (!i->fileName.isEmpty()) {
		bp->fileName = i->fileName;
		bp->lineNo = i->lineNo;
	    }
	}
    }

    // orphaned breakpoints must be copied
    for (BrkptIterator bp = m_brkpts.begin(); bp != m_brkpts.end(); ++bp)
    {
	if (bp->isOrphaned())
	    brks.push_back(*bp);
    }

    m_brkpts.swap(brks);
    emit breakpointsChanged();
}

// look if there is at least one temporary breakpoint
// or a watchpoint
bool KDebugger::stopMayChangeBreakList() const
{
    for (BrkptROIterator bp = m_brkpts.begin(); bp != m_brkpts.end(); ++bp)
    {
	if (bp->temporary || bp->type == Breakpoint::watchpoint)
	    return true;
    }
    return false;
}

KDebugger::BrkptIterator KDebugger::breakpointByFilePos(QString file, int lineNo,
					   const DbgAddr& address)
{
    // look for exact file name match
    for (BrkptIterator bp = m_brkpts.begin(); bp != m_brkpts.end(); ++bp)
    {
	if (bp->lineNo == lineNo &&
	    bp->fileName == file &&
	    (address.isEmpty() || bp->address == address))
	{
	    return bp;
	}
    }
    // not found, so try basename
    file = QFileInfo(file).fileName();

    for (BrkptIterator bp = m_brkpts.begin(); bp != m_brkpts.end(); ++bp)
    {
	// get base name of breakpoint's file
	QString basename = QFileInfo(bp->fileName).fileName();

	if (bp->lineNo == lineNo &&
	    basename == file &&
	    (address.isEmpty() || bp->address == address))
	{
	    return bp;
	}
    }

    // not found
    return m_brkpts.end();
}

KDebugger::BrkptIterator KDebugger::breakpointById(int id)
{
    for (BrkptIterator bp = m_brkpts.begin(); bp != m_brkpts.end(); ++bp)
    {
	if (bp->id == id) {
	    return bp;
	}
    }
    // not found
    return m_brkpts.end();
}

void KDebugger::slotValuePopup(const QString& expr)
{
    // search the local variables for a match
    VarTree* v = m_localVariables.topLevelExprByName(expr);
    if (v == 0) {
	// not found, check watch expressions
	v = m_watchVariables.topLevelExprByName(expr);
	if (v == 0) {
	    // try a member of 'this'
	    v = m_localVariables.topLevelExprByName("this");
	    if (v != 0)
		v = ExprWnd::ptrMemberByName(v, expr);
	    if (v == 0) {
		// nothing found, try printing variable in gdb
		if (m_d) {
		    CmdQueueItem *cmd = m_d->executeCmd(DCprintPopup, expr);
		    cmd->m_popupExpr = expr;
		}
		return;
	    }
	}
    }

    // construct the tip
    QString tip = v->getText() + " = " + formatPopupValue(v);
    emit valuePopup(tip);
}

void KDebugger::slotDisassemble(const QString& fileName, int lineNo)
{
    if (m_haveExecutable) {
	CmdQueueItem* cmd = m_d->queueCmdPrio(DCinfoline, fileName, lineNo);
	cmd->m_fileName = fileName;
	cmd->m_lineNo = lineNo;
    }
}

void KDebugger::submitDisassemblyFlavor()
{
    QString flavor = m_flavor;

    if (m_flavor.isEmpty()) {
	flavor = m_globalFlavor;
    }

    m_effectiveFlavor = flavor;

    if (!flavor.isEmpty())
	m_d->executeCmd(DCsetdisassflavor, flavor);
}

void KDebugger::setDefaultFlavor(const QString& defFlavor)
{
    m_globalFlavor = defFlavor;
}

void KDebugger::handleInfoLine(CmdQueueItem* cmd, const char* output)
{
    QString addrFrom, addrTo;
    if (cmd->m_lineNo >= 0) {
	// disassemble
	if (m_d->parseInfoLine(output, addrFrom, addrTo)) {
	    // got the address range, now get the real code
	    CmdQueueItem* c = m_d->queueCmdPrio(DCdisassemble, addrFrom, addrTo);
	    c->m_fileName = cmd->m_fileName;
	    c->m_lineNo = cmd->m_lineNo;
	} else {
	    // no code
	    emit disassembled(cmd->m_fileName, cmd->m_lineNo, std::list<DisassembledCode>());
	}
    } else {
	// set program counter
	if (m_d->parseInfoLine(output, addrFrom, addrTo)) {
	    // move the program counter to the start address
	    m_d->executeCmd(DCsetpc, addrFrom);
	}
    }
}

void KDebugger::handleInfoTarget(const char* output)
{
    m_cpuTarget = m_d->parseInfoTarget(output);
}

void KDebugger::handleDisassemble(CmdQueueItem* cmd, const char* output)
{
    emit disassembled(cmd->m_fileName, cmd->m_lineNo,
		      m_d->parseDisassemble(output));
}

void KDebugger::handleThreadList(const char* output)
{
    emit threadsChanged(m_d->parseThreadList(output));
}

void KDebugger::setThread(int id)
{
    m_d->queueCmdPrio(DCthread, id);
}

void KDebugger::setMemoryExpression(const QString& start_memexpr, unsigned total_length,
        const QString& current_memexpr, unsigned current_length)
{
    m_memoryExpression = current_memexpr;
    m_memoryLength = current_length;
    m_memoryStartExpression = start_memexpr;
    m_memoryTotalLength = total_length;

    // queue the new expression
    if (!m_memoryExpression.isEmpty() &&
	isProgramActive() &&
	!isProgramRunning())
    {
	queueMemoryDump(true, false);
    }
}

void KDebugger::queueMemoryDump(bool immediate, bool update)
{
    if (update) {
	if (immediate)
	    m_d->queueCmdPrio(DCexamine, m_memoryStartExpression, m_memoryFormat, m_memoryTotalLength);
	else
	    m_d->queueCmd(DCexamine, m_memoryStartExpression, m_memoryFormat, m_memoryTotalLength);
    } else {
	if (immediate)
	    m_d->queueCmdPrio(DCexamine, m_memoryExpression, m_memoryFormat, m_memoryLength);
	else
	    m_d->queueCmd(DCexamine, m_memoryExpression, m_memoryFormat, m_memoryLength);
    }
}

void KDebugger::handleMemoryDump(const char* output)
{
    std::list<MemoryDump> memdump;
    QString msg = m_d->parseMemoryDump(output, memdump);
    emit memoryDumpChanged(msg, memdump);
}

void KDebugger::setProgramCounter(const QString& file, int line, const DbgAddr& addr)
{
    if (addr.isEmpty()) {
	// find address of the specified line
	CmdQueueItem* cmd = m_d->executeCmd(DCinfoline, file, line);
	cmd->m_lineNo = -1;		/* indicates "Set PC" UI command */
    } else {
	// move the program counter to that address
	m_d->executeCmd(DCsetpc, addr.asString());
    }
}

void KDebugger::handleSetDisassFlavor(const char* output)
{
    QString res = m_d->parseSetDisassFlavor(output);

    if (!res.isEmpty()) {
	m_statusMessage = i18n("Setting the disassembly flavor failed.");
	emit updateStatusMessage();
	return;
    }

    emit disassFlavorChanged(m_effectiveFlavor, m_cpuTarget);
}

void KDebugger::handleSetPC(const char* /*output*/)
{
    // TODO: handle errors

    // now go to the top-most frame
    // this also modifies the program counter indicator in the UI
    gotoFrame(0);
}

void KDebugger::slotValueEdited(VarTree* expr, const QString& text)
{
    if (text.simplified().isEmpty())
	return;			       /* no text entered: ignore request */

    ExprWnd* wnd = static_cast<ExprWnd*>(expr->treeWidget());
    TRACE(QString::asprintf("Changing %s to ",
			    wnd->exprList().join(" ")) + text);

    // determine the lvalue to edit
    QString lvalue = expr->computeExpr();
    CmdQueueItem* cmd = m_d->executeCmd(DCsetvariable, lvalue, text);
    cmd->m_expr = expr;
    cmd->m_exprWnd = wnd;
}

void KDebugger::handleSetVariable(CmdQueueItem* cmd, const char* output)
{
    QString msg = m_d->parseSetVariable(output);
    if (!msg.isEmpty())
    {
	// there was an error; display it in the status bar
	m_statusMessage = msg;
	emit updateStatusMessage();
	return;
    }

    // get the new value
    QString expr = cmd->m_expr->computeExpr();
    CmdQueueItem* printCmd = m_d->queueCmdPrio(DCprint, expr);
    printCmd->m_expr = cmd->m_expr;
    printCmd->m_exprWnd = cmd->m_exprWnd;
}
