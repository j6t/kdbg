// $Id$

// Copyright by Johannes Sixt
// This file is under GPL, the GNU General Public Licence

#include "debugger.h"
#include "parsevar.h"
#include "pgmargs.h"
#include "procattach.h"
#include "typetable.h"
#include "exprwnd.h"
#include "valarray.h"
#include <qregexp.h>
#include <qfileinf.h>
#include <qlistbox.h>
#include <kapp.h>
#include <kfiledialog.h>
#include <kmsgbox.h>
#include <ksimpleconfig.h>
#include <kconfig.h>
#include <kwm.h>
#if QT_VERSION >= 200
#include <klocale.h>			/* i18n */
#endif
#include <ctype.h>

#ifndef VERSION				/* #ifndef HAVE_CONFIG_H */
#define VERSION ""
#endif
#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>			/* mknod(2) */
#endif
#ifdef HAVE_FCNTL_H
#include <fcntl.h>			/* open(2) */
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>			/* sleep(3) */
#endif
#include "mydebug.h"


GdbProcess::GdbProcess()
{
}

int GdbProcess::commSetupDoneC()
{
    if (!KProcess::commSetupDoneC())
	return 0;

    close(STDERR_FILENO);
    return dup2(STDOUT_FILENO, STDERR_FILENO) != -1;
}

const char defaultTermCmdStr[] = "xterm -name kdbgio -title %T -e sh -c %C";
const char defaultDebuggerCmdStr[] =
	"gdb"
	" -fullname"			/* to get standard file names each time the prog stops */
	" -nx";				/* do not execute initialization files */

KDebugger::KDebugger(QWidget* parent,
		     ExprWnd* localVars,
		     ExprWnd* watchVars,
		     QListBox* backtrace
		     ) :
	QObject(parent, "debugger"),
	m_outputTermCmdStr(defaultTermCmdStr),
	m_outputTermPID(0),
	m_debuggerCmdStr(defaultDebuggerCmdStr),
	m_state(DSidle),
	m_gdbOutput(0),
	m_gdbOutputLen(0),
	m_gdbOutputAlloc(0),
	m_activeCmd(0),
	m_haveExecutable(false),
	m_programActive(false),
	m_programRunning(false),
	m_sharedLibsListed(false),
	m_typeTable(0),
	m_programConfig(0),
	m_gdb(),
	m_gdbMajor(4), m_gdbMinor(16),
#ifdef GDB_TRANSCRIPT
	m_logFile(GDB_TRANSCRIPT),
#endif
	m_bpTable(*this),
	m_localVariables(*localVars),
	m_watchVariables(*watchVars),
	m_btWindow(*backtrace),
	m_animationTimer(this),
	m_animationInterval(0)
{
    m_gdbOutputAlloc = 4000;
    m_gdbOutput = new char[m_gdbOutputAlloc];

    m_hipriCmdQueue.setAutoDelete(true);
    m_envVars.setAutoDelete(true);

    connect(&m_localVariables, SIGNAL(expanding(KTreeViewItem*,bool&)),
	    SLOT(slotLocalsExpanding(KTreeViewItem*,bool&)));
    connect(&m_watchVariables, SIGNAL(expanding(KTreeViewItem*,bool&)),
	    SLOT(slotWatchExpanding(KTreeViewItem*,bool&)));

    connect(&m_btWindow, SIGNAL(highlighted(int)), SLOT(gotoFrame(int)));

    m_bpTable.setCaption(i18n("Breakpoints"));
    connect(&m_bpTable, SIGNAL(closed()), SIGNAL(updateUI()));
    connect(&m_bpTable, SIGNAL(activateFileLine(const QString&,int)),
	    this, SIGNAL(activateFileLine(const QString&,int)));
    connect(this, SIGNAL(updateUI()), &m_bpTable, SLOT(updateUI()));

    // debugger process
    connect(&m_gdb, SIGNAL(receivedStdout(KProcess*,char*,int)),
	    SLOT(receiveOutput(KProcess*,char*,int)));
    connect(&m_gdb, SIGNAL(wroteStdin(KProcess*)), SLOT(commandRead(KProcess*)));
    connect(&m_gdb, SIGNAL(processExited(KProcess*)), SLOT(gdbExited(KProcess*)));

    // animation
    connect(&m_animationTimer, SIGNAL(timeout()), SIGNAL(animationTimeout()));
    // special update of animation
    connect(this, SIGNAL(updateUI()), SLOT(slotUpdateAnimation()));

    emit updateUI();
}

KDebugger::~KDebugger()
{
    TRACE(__PRETTY_FUNCTION__);

    // if the output window is open, close it
    if (m_outputTermPID != 0) {
	kill(m_outputTermPID, SIGTERM);
    }

    if (m_programConfig != 0) {
	saveProgramSettings();
	m_programConfig->sync();
	delete m_programConfig;
    }
    delete m_typeTable;
    delete[] m_gdbOutput;
}


const char DebuggerGroup[] = "Debugger";
const char WindowGroup[] = "Windows";
const char OutputWindowGroup[] = "OutputWindow";
const char DebuggerCmdStr[] = "DebuggerCmdStr";
const char BreaklistVisible[] = "BreaklistVisible";
const char Breaklist[] = "Breaklist";
const char TermCmdStr[] = "TermCmdStr";
const char KeepScript[] = "KeepScript";

void KDebugger::saveSettings(KConfig* config)
{
    KConfigGroupSaver g(config, WindowGroup);
    // breakpoint window
    bool visible = m_bpTable.isVisible();
    // ask window manager for position
    const QRect& r = KWM::geometry(m_bpTable.winId());
    config->writeEntry(BreaklistVisible, visible);
    config->writeEntry(Breaklist, r);

    config->setGroup(DebuggerGroup);
    config->writeEntry(DebuggerCmdStr, m_debuggerCmdStr);
    
    config->setGroup(OutputWindowGroup);
    config->writeEntry(TermCmdStr, m_outputTermCmdStr);
}

void KDebugger::restoreSettings(KConfig* config)
{
    KConfigGroupSaver g(config, WindowGroup);
    // breakpoint window
    bool visible = config->readBoolEntry(BreaklistVisible, false);
    if (config->hasKey(Breaklist)) {
	QRect r = config->readRectEntry(Breaklist);
	m_bpTable.setGeometry(r);
    }
    visible ? m_bpTable.show() : m_bpTable.hide();

    config->setGroup(DebuggerGroup);
    setDebuggerCmd(config->readEntry(DebuggerCmdStr));

    /*
     * For debugging and emergency purposes, let the config file override
     * the shell script that is used to keep the output window open. This
     * string must have EXACTLY 1 %s sequence in it.
     */
    config->setGroup(OutputWindowGroup);
    setTerminalCmd(config->readEntry(TermCmdStr, defaultTermCmdStr));
    m_outputTermKeepScript = config->readEntry(KeepScript);
}


// helper that gets a file name (it only differs in the caption of the dialog)
static QString getFileName(const char* caption,
			   const char* dir = 0, const char* filter = 0,
			   QWidget* parent = 0, const char* name = 0)
{
    QString filename;
    KFileDialog dlg(dir, filter, parent, name, true, false);

    dlg.setCaption(caption);

    if (dlg.exec() == QDialog::Accepted)
	filename = dlg.selectedFile();

    return filename;
}


//////////////////////////////////////////////////////////////////////
// external interface

bool KDebugger::debugProgram(const QString& name)
{
    TRACE(__PRETTY_FUNCTION__);
    // check for running program
    if (!m_executable.isEmpty()) {
	return false;
    }

    // check the file name
    QFileInfo fi(name);
    m_lastDirectory = fi.dirPath(true);

    if (!fi.isFile()) {
	QString msgFmt = i18n("`%s' is not a file or does not exist");
#if QT_VERSION < 200
	QString msg(msgFmt.length() + name.length() + 20);
#else
	QString msg;
#endif
	msg.sprintf(msgFmt, name.data());
	KMsgBox::message(parentWidget(), kapp->appName(),
			 msg,
			 KMsgBox::STOP,
			 i18n("OK"));
	return false;
    }

    if (!m_gdb.isRunning()) {
	if (!startGdb()) {
	    TRACE("startGdb failed");
	    return false;
	}
    }

    TRACE("before file cmd");
    executeCmd(DCexecutable, "file " + name);
    m_executable = name;

    // create the program settings object
    QString pgmConfigFile = fi.dirPath(false);
    if (!pgmConfigFile.isEmpty()) {
	pgmConfigFile += '/';
    }
    pgmConfigFile += ".kdbgrc." + fi.fileName();
    TRACE("program config file = " + pgmConfigFile);
    // check whether we can write to the file
    QFile file(pgmConfigFile);
    bool readonly = true;
    bool openit = true;
    if (file.open(IO_ReadWrite)) {	/* don't truncate! */
	readonly = false;
	// the file exists now
    } else if (!file.open(IO_ReadOnly)) {
	/* file does not exist and cannot be created: don't use it */
	openit = false;
    }
    if (openit) {
	m_programConfig = new KSimpleConfig(pgmConfigFile, readonly);
	// it is read in later in the handler of DCexecutable

	// create a type table
	m_typeTable = new ProgramTypeTable;
	m_sharedLibsListed = false;
    }

    emit updateUI();

    return true;
}


void KDebugger::fileExecutable()
{
    if (m_state != DSidle)
	return;

    // open a new executable
    QString executable = getFileName(i18n("Select the executable to debug"),
				     m_lastDirectory, 0, parentWidget());
    if (executable.isEmpty())
	return;

    if (m_haveExecutable)
    {
	QApplication::setOverrideCursor(waitCursor);

	stopGdb();
	/*
	 * We MUST wait until the slot gdbExited() has been called. But to
	 * avoid a deadlock, we wait only for some certain maximum time.
	 * Should this timeout be reached, the only reasonable thing one
	 * could do then is exiting kdbg.
	 */
	int maxTime = 20;		/* about 20 seconds */
	while (m_haveExecutable && maxTime > 0) {
	    kapp->processEvents(1000);
	    // give gdb time to die (and send a SIGCLD)
	    ::sleep(1);
	    --maxTime;
	}

	QApplication::restoreOverrideCursor();

	if (m_haveExecutable) {
	    /* timed out! We can't really do anything useful now */
	    TRACE("timed out while waiting for gdb to die!");
	    return;
	}
    }

    debugProgram(executable);
}

void KDebugger::fileCoreFile()
{
    if (!isReady() || m_programRunning)
	return;

    // use a core dump
    QString corefile = getFileName(i18n("Select core dump"),
				   m_lastDirectory, 0, parentWidget());
    if (!corefile.isEmpty()) {
	m_corefile = corefile;
	CmdQueueItem* cmd = loadCoreFile();
	cmd->m_byUser = true;
    }
}

void KDebugger::programRun()
{
    if (!isReady())
	return;

    if (m_programActive) {
	// gdb command: continue
	executeCmd(DCcont, "cont", true);
    } else {
	// gdb command: run
	executeCmd(DCrun, "run", true);
	m_programActive = true;
    }
    m_programRunning = true;
}

void KDebugger::programAttach()
{
    if (!isReady())
	return;

    ProcAttach dlg(parentWidget());
    dlg.setText(m_attachedPid);
    if (dlg.exec()) {
	m_attachedPid = dlg.text();
	TRACE("Attaching to " + m_attachedPid);
	executeCmd(DCattach, "attach " + m_attachedPid);
	m_programActive = true;
	m_programRunning = true;
    }
}

void KDebugger::programRunAgain()
{
    if (canSingleStep()) {
	executeCmd(DCrun, "run", true);
	m_programRunning = true;
    }
}

void KDebugger::programStep()
{
    if (canSingleStep()) {
	executeCmd(DCstep, "step", true);
	m_programRunning = true;
    }
}

void KDebugger::programNext()
{
    if (canSingleStep()) {
	executeCmd(DCnext, "next", true);
	m_programRunning = true;
    }
}

void KDebugger::programFinish()
{
    if (canSingleStep()) {
	executeCmd(DCfinish, "finish", true);
	m_programRunning = true;
    }
}

bool KDebugger::runUntil(const QString& fileName, int lineNo)
{
    if (isReady() && m_programActive && !m_programRunning) {
	// strip off directory part of file name
	QString file = fileName;
#if QT_VERSION < 200
	file.detach();
#endif
	int offset = file.findRev("/");
	if (offset >= 0) {
	    file.remove(0, offset+1);
	}
#if QT_VERSION < 200
	QString cmdString(file.length() + 30);
#else
	QString cmdString;
#endif
	cmdString.sprintf("until %s:%d", file.data(), lineNo+1);
	executeCmd(DCuntil, cmdString, true);
	m_programRunning = true;
	return true;
    } else {
	return false;
    }
}

void KDebugger::programBreak()
{
    if (m_haveExecutable && m_programRunning) {
	m_gdb.kill(SIGINT);
    }
}

void KDebugger::programArgs()
{
    if (m_haveExecutable) {
	PgmArgs dlg(parentWidget(), m_executable, m_envVars);
	dlg.setText(m_programArgs);
	if (dlg.exec()) {
	    updateProgEnvironment(dlg.text(), dlg.envVars());
	}
    }
}

void KDebugger::breakListToggleVisible()
{
    if (m_bpTable.isVisible()) {
	m_bpTable.hide();
    } else {
	m_bpTable.show();
    }
}

bool KDebugger::setBreakpoint(const QString& fileName, int lineNo, bool temporary)
{
    if (isReady()) {
	m_bpTable.doBreakpoint(fileName, lineNo, temporary);
	return true;
    } else {
	return false;
    }
}

bool KDebugger::enableDisableBreakpoint(const QString& fileName, int lineNo)
{
    if (isReady()) {
	m_bpTable.doEnableDisableBreakpoint(fileName, lineNo);
	return true;
    } else {
	return false;
    }
}

bool KDebugger::canSingleStep()
{
    return isReady() && m_programActive && !m_programRunning;
}

bool KDebugger::canChangeBreakpoints()
{
    return isReady() && !m_programRunning;
}

void KDebugger::setTerminalCmd(const QString& cmd)
{
    m_outputTermCmdStr = cmd;
    // revert to default if empty
    if (m_outputTermCmdStr.isEmpty()) {
	m_outputTermCmdStr = defaultTermCmdStr;
    }
}

void KDebugger::setDebuggerCmd(const QString& cmd)
{
    m_debuggerCmdStr = cmd;
    // revert to default if empty
    if (m_debuggerCmdStr.isEmpty()) {
	m_debuggerCmdStr = defaultDebuggerCmdStr;
    }
}


//////////////////////////////////////////////////////////
// debugger driver

static void splitCmdStr(const QString& cmd, ValArray<QString>& parts)
{
    QString str = cmd.simplifyWhiteSpace();
    int start = 0;
    int end;
    while ((end = str.find(' ', start)) >= 0) {
	parts.append(str.mid(start, end-start));
	start = end+1;
    }
    parts.append(str.mid(start, str.length()-start));
}

#define PROMPT "(kdbg)"
#define PROMPT_LEN 6
#define PROMPT_LAST_CHAR ')'		/* needed when searching for prompt string */

bool KDebugger::startGdb()
{
    if (m_outputTermName.isEmpty()) {
	// create an output window
	if (!createOutputWindow()) {
	    TRACE("createOuputWindow failed");
	    return false;
	}
	TRACE("successfully created output window");
    }

    // clear command queues
    delete m_activeCmd;
    m_activeCmd = 0;
    m_hipriCmdQueue.clear();
    bool autodel = m_lopriCmdQueue.autoDelete();
    m_lopriCmdQueue.setAutoDelete(true);
    m_lopriCmdQueue.clear();
    m_lopriCmdQueue.setAutoDelete(autodel);

    // debugger executable
    ValArray<QString> cmdParts;
    splitCmdStr(m_debuggerCmdStr, cmdParts);
    
    m_gdb.clearArguments();
    for (int i = 0; i < cmdParts.size(); i++) {
	m_gdb << cmdParts[i];
    }

    m_explicitKill = false;
    if (!m_gdb.start(KProcess::NotifyOnExit,
		     KProcess::Communication(KProcess::Stdin|KProcess::Stdout))) {
	return false;
    }

#ifdef GDB_TRANSCRIPT
    // open log file
    if (!m_logFile.isOpen()) {
	m_logFile.open(IO_WriteOnly);
    }
#endif

    // change prompt string and synchronize with gdb
    m_state = DSidle;
    executeCmd(DCinitialize, "set prompt " PROMPT);
    executeCmd(DCinitialSet, "set confirm off");
    executeCmd(DCinitialSet, "set print static-members off");
    executeCmd(DCinitialSet, "tty " + m_outputTermName);

    return true;
}

void KDebugger::stopGdb()
{
    m_explicitKill = true;
    m_gdb.kill(SIGTERM);
    m_state = DSidle;
}

void KDebugger::gdbExited(KProcess*)
{
#ifdef GDB_TRANSCRIPT
    static const char txt[] = "\ngdb exited\n";
    m_logFile.writeBlock(txt,sizeof(txt)-1);
#endif

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
	TRACE("gdb exited normally");
    } else {
	const char* msg = i18n("gdb exited unexpectedly.\n"
			       "Restart the session (e.g. with File|Executable).");
	KMsgBox::message(parentWidget(), kapp->appName(), msg, KMsgBox::EXCLAMATION);
    }

    // reset state
    m_state = DSidle;
    m_haveExecutable = false;
    m_executable = "";
    m_programActive = false;
    m_programRunning = false;
    m_explicitKill = false;
    // empty buffer
    m_gdbOutputLen = 0;
    *m_gdbOutput = '\0';

    // stop gear wheel and erase PC
    stopAnimation();
    emit updatePC(QString(), -1, 0);
}

const char fifoNameBase[] = "/tmp/kdbgttywin%05d";

/*
 * We use the scope operator :: in this function, so that we don't
 * accidentally use the wrong close() function (I've been bitten ;-),
 * outch!) (We use it for all the libc functions, to be consistent...)
 */
bool KDebugger::createOutputWindow()
{
    // create a name for a fifo
    QString fifoName;
    fifoName.sprintf(fifoNameBase, ::getpid());

    // create a fifo that will pass in the tty name
    ::unlink(fifoName);			/* remove remnants */
#ifdef HAVE_MKFIFO
    if (::mkfifo(fifoName, S_IRUSR|S_IWUSR) < 0) {
	// failed
	TRACE("mkfifo " + fifoName + " failed");
	return false;
    }
#else
    if (::mknod(fifoName, S_IFIFO | S_IRUSR|S_IWUSR, 0) < 0) {
	// failed
	TRACE("mknod " + fifoName + " failed");
	return false;
    }
#endif

    int pid = ::fork();
    if (pid < 0) {
	// error
	TRACE("fork failed for fifo " + fifoName);
	::unlink(fifoName);
	return false;
    }
    if (pid == 0) {
	// child process
	/*
	 * Spawn an xterm that in turn runs a shell script that passes us
	 * back the terminal name and then only sits and waits.
	 */
	static const char shellScriptFmt[] =
	    "tty>%s;"
	    "trap \"\" INT QUIT TSTP;"	/* ignore various signals */
	    "exec<&-;exec>&-;"		/* close stdin and stdout */
	    "while :;do sleep 3600;done";
	// let config file override this script
	const char* fmt = shellScriptFmt;
	if (m_outputTermKeepScript.length() != 0) {
	    fmt = m_outputTermKeepScript.data();
	}
#if QT_VERSION < 200
	QString shellScript(strlen(fmt) + fifoName.length());
#else
	QString shellScript;
#endif
	shellScript.sprintf(fmt, fifoName.data());
	TRACE("output window script is " + shellScript);

	QString title = kapp->getCaption();
	title += i18n(": Program output");

	// parse the command line specified in the preferences
	ValArray<QString> cmdParts;
	splitCmdStr(m_outputTermCmdStr, cmdParts);

	/*
	 * Build the argv array. Thereby substitute special sequences:
	 */
	struct {
	    char seq[4];
	    QString replace;
	} substitute[] = {
	    { "%T", title },
	    { "%C", shellScript }
	};
	const char** argv = new const char*[cmdParts.size()+1];
	argv[cmdParts.size()] = 0;

	for (int i = cmdParts.size()-1; i >= 0; i--) {
	    QString& str = cmdParts[i];
	    for (int j = sizeof(substitute)/sizeof(substitute[0])-1; j >= 0; j--) {
		int pos = str.find(substitute[j].seq);
		if (pos >= 0) {
		    str.replace(pos, 2, substitute[j].replace);
		    break;		/* substitute only one sequence */
		}
	    }
	    argv[i] = str;
	}

	::execvp(argv[0], const_cast<char* const*>(argv));

	// failed; what can we do?
	::exit(0);
    } else {
	// parent process
	// read the ttyname from the fifo
	int f = ::open(fifoName, O_RDONLY);
	if (f < 0) {
	    // error
	    ::unlink(fifoName);
	    return false;
	}

	char ttyname[50];
	int n = ::read(f, ttyname, sizeof(ttyname)-sizeof(char));   /* leave space for '\0' */

	::close(f);
	::unlink(fifoName);

	if (n < 0) {
	    // error
	    return false;
	}

	// remove whitespace
	QString tty(ttyname, n+1);
	m_outputTermName = tty.stripWhiteSpace();
	m_outputTermPID = pid;
	TRACE(QString().sprintf("tty=%s", m_outputTermName.data()));
    }
    return true;
}

const char GeneralGroup[] = "General";
const char EnvironmentGroup[] = "Environment";
const char FileVersion[] = "FileVersion";
const char ProgramArgs[] = "ProgramArgs";
const char Variable[] = "Var%d";
const char Value[] = "Value%d";

void KDebugger::saveProgramSettings()
{
    ASSERT(m_programConfig != 0);
    m_programConfig->setGroup(GeneralGroup);
    m_programConfig->writeEntry(FileVersion, 1);
    m_programConfig->writeEntry(ProgramArgs, m_programArgs);

    // write environment variables
    m_programConfig->deleteGroup(EnvironmentGroup);
    m_programConfig->setGroup(EnvironmentGroup);
    QDictIterator<EnvVar> it = m_envVars;
    EnvVar* var;
    QString varName;
    QString varValue;
    for (int i = 0; (var = it) != 0; ++it, ++i) {
	varName.sprintf(Variable, i);
	varValue.sprintf(Value, i);
	m_programConfig->writeEntry(varName, it.currentKey());
	m_programConfig->writeEntry(varValue, var->value);
    }

    m_bpTable.saveBreakpoints(m_programConfig);
}

void KDebugger::restoreProgramSettings()
{
    ASSERT(m_programConfig != 0);
    m_programConfig->setGroup(GeneralGroup);
    /*
     * We ignore file version for now we will use it in the future to
     * distinguish different versions of this configuration file.
     */
    QString pgmArgs = m_programConfig->readEntry(ProgramArgs);

    // read environment variables
    m_programConfig->setGroup(EnvironmentGroup);
    m_envVars.clear();
    QDict<EnvVar> pgmVars;
    EnvVar* var;
    QString varName;
    QString varValue;
    for (int i = 0;; ++i) {
	varName.sprintf(Variable, i);
	varValue.sprintf(Value, i);
	if (!m_programConfig->hasKey(varName)) {
	    /* entry not present, assume that we've hit them all */
	    break;
	}
	QString name = m_programConfig->readEntry(varName);
	if (name.isEmpty()) {
	    // skip empty names
	    continue;
	}
	var = new EnvVar;
	var->value = m_programConfig->readEntry(varValue);
	var->status = EnvVar::EVnew;
	pgmVars.insert(name, var);
    }

    updateProgEnvironment(pgmArgs, pgmVars);

    m_bpTable.restoreBreakpoints(m_programConfig);
}


KDebugger::CmdQueueItem* KDebugger::executeCmd(KDebugger::DbgCommand cmd,
					       QString cmdString, bool clearLow)
{
    // place a new command into the high-priority queue
    CmdQueueItem* cmdItem = new CmdQueueItem(cmd, cmdString + "\n");
    m_hipriCmdQueue.enqueue(cmdItem);

    if (clearLow) {
	if (m_state == DSrunningLow) {
	    // take the liberty to interrupt the running command
	    m_state = DSinterrupted;
	    m_gdb.kill(SIGINT);
	    ASSERT(m_activeCmd != 0);
	    TRACE(QString().sprintf("interrupted the command %d",
		  (m_activeCmd ? m_activeCmd->m_cmd : -1)));
	    delete m_activeCmd;
	    m_activeCmd = 0;
	}
	while (!m_lopriCmdQueue.isEmpty()) {
	    delete m_lopriCmdQueue.take(0);
	}
    }
    // if gdb is idle, send it the command
    if (m_state == DSidle) {
	ASSERT(m_activeCmd == 0);
	writeCommand();
    }

    return cmdItem;
}

KDebugger::CmdQueueItem* KDebugger::queueCmd(KDebugger::DbgCommand cmd,
					     QString cmdString, QueueMode mode)
{
    // place a new command into the low-priority queue
#if QT_VERSION < 200
    cmdString.detach();
#endif
    cmdString += "\n";

    CmdQueueItem* cmdItem = 0;
    switch (mode) {
    case QMoverrideMoreEqual:
    case QMoverride:
	// check whether gdb is currently processing this command
	if (m_activeCmd != 0 &&
	    m_activeCmd->m_cmd == cmd && m_activeCmd->m_cmdString == cmdString)
	{
	    return m_activeCmd;
	}
	// check whether there is already the same command in the queue
	for (cmdItem = m_lopriCmdQueue.first(); cmdItem != 0; cmdItem = m_lopriCmdQueue.next()) {
	    if (cmdItem->m_cmd == cmd && cmdItem->m_cmdString == cmdString)
		break;
	}
	if (cmdItem != 0) {
	    // found one
	    if (mode == QMoverrideMoreEqual) {
		// All commands are equal, but some are more equal than others...
		// put this command in front of all others
		m_lopriCmdQueue.take();
		m_lopriCmdQueue.insert(0, cmdItem);
	    }
	    break;
	} // else none found, so add it
	// drop through
    case QMnormal:
	cmdItem = new CmdQueueItem(cmd, cmdString);
	m_lopriCmdQueue.append(cmdItem);
    }

    // if gdb is idle, send it the command
    if (m_state == DSidle) {
	ASSERT(m_activeCmd == 0);
	writeCommand();
    }

    return cmdItem;
}

// dequeue a pending command, make it the active one and send it to gdb
void KDebugger::writeCommand()
{
//    ASSERT(m_activeCmd == 0);
    assert(m_activeCmd == 0);

    // first check the high-priority queue - only if it is empty
    // use a low-priority command.
    CmdQueueItem* cmd = m_hipriCmdQueue.dequeue();
    DebuggerState newState = DScommandSent;
    if (cmd == 0) {
	cmd = m_lopriCmdQueue.first();
	m_lopriCmdQueue.removeFirst();
	newState = DScommandSentLow;
    }
    if (cmd == 0) {
	// nothing to do
	m_state = DSidle;		/* is necessary if command was interrupted earlier */
	return;
    }

    m_activeCmd = cmd;
    TRACE("in writeCommand: " + cmd->m_cmdString);

    const char* str = cmd->m_cmdString;
    m_gdb.writeStdin(const_cast<char*>(str), cmd->m_cmdString.length());

#ifdef GDB_TRANSCRIPT
    // write also to log file
    m_logFile.writeBlock(str, cmd->m_cmdString.length());
    m_logFile.flush();
#endif

    m_state = newState;
}

void KDebugger::commandRead(KProcess*)
{
    TRACE(__PRETTY_FUNCTION__);

    // there must be an active command which is not yet commited
    ASSERT(m_state == DScommandSent || m_state == DScommandSentLow);
    ASSERT(m_activeCmd != 0);
    ASSERT(!m_activeCmd->m_committed);

    // commit the command
    m_activeCmd->m_committed = true;

    // now the debugger is officially working on the command
    m_state = m_state == DScommandSent ? DSrunning : DSrunningLow;

    // set the flag that reflects whether the program is really running
    switch (m_activeCmd->m_cmd) {
    case DCrun:	case DCcont: case DCnext: case DCstep: case DCfinish: case DCuntil:
	m_programRunning = true;
	break;
    default:
	break;
    }

    // re-receive delayed output
    if (m_delayedOutput.current() != 0) {
	QString* delayed;
	while ((delayed = m_delayedOutput.dequeue()) != 0) {
	    const char* str = *delayed;
	    receiveOutput(0, const_cast<char*>(str), delayed->length());
	    delete delayed;
	}
	// updateUI() has been emitted
    } else {
	emit updateUI();
    }
}

void KDebugger::receiveOutput(KProcess*, char* buffer, int buflen)
{
    /*
     * The debugger should be running (processing a command) at this point.
     * If it is not, it is still idle because we haven't received the
     * wroteStdin signal yet, in which case there must be an active command
     * which is not commited.
     */
    if (m_state == DScommandSent || m_state == DScommandSentLow) {
	ASSERT(m_activeCmd != 0);
	ASSERT(!m_activeCmd->m_committed);
	/*
	 * We received output before we got signal wroteStdin. Collect this
	 * output, it will be re-sent by commandRead when it gets the
	 * acknowledgment for the uncommitted command.
	 */
	m_delayedOutput.enqueue(new QString(buffer, buflen+1));
	return;
    }
#ifdef GDB_TRANSCRIPT
    // write to log file (do not log delayed output - it would appear twice)
    m_logFile.writeBlock(buffer, buflen);
    m_logFile.flush();
#endif
    
    /*
     * gdb sometimes produces stray output while it's idle. This happens if
     * it receives a signal, most prominently a SIGCONT after a SIGTSTP:
     * The user haltet kdbg with Ctrl-Z, then continues it with "fg", which
     * also continues gdb, which repeats the prompt!
     */
    if (m_activeCmd == 0 && m_state != DSinterrupted) {
	// ignore the output
	TRACE("ignoring stray output: " + QString(buffer, buflen+1));
	return;
    }
    ASSERT(m_state == DSrunning || m_state == DSrunningLow || m_state == DSinterrupted);
    ASSERT(m_activeCmd != 0 || m_state == DSinterrupted);

    // collect output until next prompt string is found
    
    // accumulate it
    if (m_gdbOutputLen + buflen >= m_gdbOutputAlloc) {
	/*
	 * Must enlarge m_gdbOutput: double it. Note: That particular
	 * sequence of commandes here ensures exception safety.
	 */
	int newSize = m_gdbOutputAlloc * 2;
	char* newBuf = new char[newSize];
	memcpy(newBuf, m_gdbOutput, m_gdbOutputLen);
	delete[] m_gdbOutput;
	m_gdbOutput = newBuf;
	m_gdbOutputAlloc = newSize;
    }
    memcpy(m_gdbOutput+m_gdbOutputLen, buffer, buflen);
    m_gdbOutputLen += buflen;
    m_gdbOutput[m_gdbOutputLen] = '\0';

    /*
     * If there's a prompt string in the collected output, it must be at
     * the very end.
     * 
     * Note: It could nevertheless happen that a character sequence that is
     * equal to the prompt string appears at the end of the output,
     * although it is very, very unlikely (namely as part of a string that
     * lingered in gdb's output buffer due to some timing/heavy load
     * conditions for a very long time such that that buffer overflowed
     * exactly at the end of the prompt string look-a-like).
     */
    if (m_gdbOutput[m_gdbOutputLen-1] == PROMPT_LAST_CHAR &&
	m_gdbOutputLen >= PROMPT_LEN &&
	strncmp(m_gdbOutput+m_gdbOutputLen-PROMPT_LEN, PROMPT, PROMPT_LEN) == 0)
    {
	// found prompt!

	// terminate output before the prompt
	m_gdbOutput[m_gdbOutputLen-PROMPT_LEN] = '\0';

	/*
	 * We've got output for the active command. But if it was
	 * interrupted, ignore it.
	 */
	if (m_state != DSinterrupted) {
	    /*
	     * m_state shouldn't be DSidle while we are parsing the output
	     * so that all commands produced by parse() go into the queue
	     * instead of being written to gdb immediately.
	     */
	    ASSERT(m_state != DSidle);
	    CmdQueueItem* cmd = m_activeCmd;
	    m_activeCmd = 0;
	    parse(cmd);
	    delete cmd;
	}

	// empty buffer
	m_gdbOutputLen = 0;
	*m_gdbOutput = '\0';
	// also clear delayed output if interrupted
	if (m_state == DSinterrupted) {
	    QString* delayed;
	    while ((delayed = m_delayedOutput.dequeue()) != 0) {
		delete delayed;
	    }
	}

	/*
	 * We parsed some output successfully. Unless there's more delayed
	 * output, the debugger must be idle now, so send down the next
	 * command.
	 */
	if (m_delayedOutput.current() == 0) {
	    if (m_hipriCmdQueue.isEmpty() && m_lopriCmdQueue.isEmpty()) {
		// no pending commands
		m_state = DSidle;
		/*
		 * If there are still expressions that need to be updated,
		 * then do so.
		 */
		evalExpressions();
	    } else {
		writeCommand();
	    }
	}
	emit updateUI();
    }
}

static QRegExp MarkerRE(":[0-9]+:[0-9]+:beg");

// parse len characters from m_gdbOutput
void KDebugger::parse(CmdQueueItem* cmd)
{
    ASSERT(cmd != 0);			/* queue mustn't be empty */

    TRACE(QString(__PRETTY_FUNCTION__) + " parsing " + m_gdbOutput);

    // command string must be committed
    if (!cmd->m_committed) {
	// not commited!
	TRACE("calling KDebugger::parse with uncommited command:\n\t" +
	      cmd->m_cmdString);
	return;
    }

    bool parseMarker = false;
    
    switch (cmd->m_cmd) {
    case DCinitialSet:
	// the output (if any) is uninteresting
    case DCsetargs:
	// there is no output
    case DCsetenv:
	/* if value is empty, we see output, but we don't care */
	break;
    case DCinitialize:
	// get version number from preamble
	{
	    int len;
	    QRegExp GDBVersion("\\nGDB [0-9]+\\.[0-9]+");
	    int offset = GDBVersion.match(m_gdbOutput, 0, &len);
	    if (offset >= 0) {
		char* start = m_gdbOutput + offset + 5;	// skip "\nGDB "
		char* end;
		m_gdbMajor = strtol(start, &end, 10);
		m_gdbMinor = strtol(end + 1, 0, 10);	// skip "."
		if (start == end) {
		    // nothing was parsed
		    m_gdbMajor = 4;
		    m_gdbMinor = 16;
		}
	    } else {
		// assume some default version (what would make sense?)
		m_gdbMajor = 4;
		m_gdbMinor = 16;
	    }
	}
	break;
    case DCexecutable:
	/*
	 * The command is successful if there is no output or the single
	 * message (no debugging symbols found)...
	 */
	if (m_gdbOutput[0] == '\0' ||
	    strcmp(m_gdbOutput, "(no debugging symbols found)...") == 0)
	{
	    // success; restore breakpoints etc.
	    if (m_programConfig != 0) {
		restoreProgramSettings();
	    }
	    // load file containing main() or core file
	    if (m_corefile.isEmpty()) {
		queueCmd(DCinfolinemain, "info line main", QMnormal);
	    } else {
		// load core file
		loadCoreFile();
	    }
	} else {
	    QString msg = "gdb: " + QString(m_gdbOutput);
	    KMsgBox::message(parentWidget(), kapp->appName(), msg,
			     KMsgBox::STOP, i18n("OK"));
	    m_executable = "";
	    m_corefile = "";		/* don't process core file */
	    m_haveExecutable = false;
	}
	break;
    case DCcorefile:
	// in any event we have an executable at this point
	m_haveExecutable = true;
	parseMarker = true;
	// if command succeeded, gdb emits a line starting with "#0 "
	if (strstr(m_gdbOutput, "\n#0 ")) {
	    // loading a core is like stopping at a breakpoint
	    handleRunCommands();
	} else {
	    // report error
	    QString msg = "gdb: " + QString(m_gdbOutput);
	    KMsgBox::message(parentWidget(), kapp->appName(), msg,
			     KMsgBox::EXCLAMATION, i18n("OK"));
	    // if core file was loaded from command line, revert to info line main
	    if (!cmd->m_byUser) {
		queueCmd(DCinfolinemain, "info line main", QMnormal);
	    }
	}
	m_corefile = "";		/* core file not available any more */
	break;
    case DCinfolinemain:
	// ignore the output, marked file info follows
	parseMarker = true;
	m_haveExecutable = true;
	break;
    case DCinfolocals:
	// parse local variables
	if (m_gdbOutput[0] != '\0') {
	    handleLocals();
	}
	break;
    case DCframe:
	handleFrameChange();
	parseMarker = true;
	updateAllExprs();
	break;
    case DCbt:
	handleBacktrace();
	updateAllExprs();
	break;
    case DCprint:
	handlePrint(cmd);
	break;
    case DCattach:
    case DCrun:
    case DCcont:
    case DCstep:
    case DCnext:
    case DCfinish:
    case DCuntil:
	handleRunCommands();
	parseMarker = true;
	break;
    case DCbreak:
	updateBreakptTable();
	break;
    case DCdelete:
    case DCenable:
    case DCdisable:
	queueCmd(DCinfobreak, "info breakpoints", QMoverride);
	break;
    case DCinfobreak:
	// note: this handler must not enqueue a command, since
	// DCinfobreak is used at various different places.
	m_bpTable.updateBreakList(m_gdbOutput);
	emit lineItemsChanged();
	break;
    case DCfindType:
	handleFindType(cmd);
	break;
    case DCprintStruct:
    case DCprintQStringStruct:
	handlePrintStruct(cmd);
	break;
    case DCinfosharedlib:
	handleSharedLibs();
	break;
    case DCcondition:
    case DCignore:
	// we are not interested in the output
	break;
    }
    /*
     * The -fullname option makes gdb send a special normalized sequence
     * print each time the program stops and at some other points. The
     * sequence has the form "\032\032filename:lineno:???:beg:address".
     */
    if (parseMarker) {
	char* startMarker = strstr(m_gdbOutput, "\032\032");
	if (startMarker != 0) {
	    // extract the marker
	    startMarker += 2;
	    TRACE(QString("found marker: ") + startMarker);
	    char* endMarker = strchr(startMarker, '\n');
	    if (endMarker != 0) {
		*endMarker = '\0';

		// extract filename and line number
		int lineNoStart = MarkerRE.match(startMarker);
		if (lineNoStart >= 0) {
		    int lineNo = atoi(startMarker + lineNoStart+1);

		    // now show the window
		    startMarker[lineNoStart] = '\0';   /* split off file name */
		    emit activateFileLine(startMarker, lineNo-1);
		}
	    }
	}
    }
}

void KDebugger::handleRunCommands()
{
    // optionally: "program changed, rereading symbols",
    // followed by:
    // "Program exited normally"
    // "Program terminated with wignal SIGSEGV"
    // "Program received signal SIGINT" or other signal
    // "Breakpoint..."
    
    // go through the output, line by line, checking what we have
    char* start = m_gdbOutput - 1;
    bool refreshNeeded = false;
    bool refreshBreaklist = false;
    QString msg;
    do {
	start++;			/* skip '\n' */

	if (strncmp(start, "Program ", 8) == 0 ||
	    strncmp(start, "ptrace: ", 8) == 0) {
	    if (strncmp(start, "Program exited", 14) == 0 ||
		strncmp(start, "Program terminated", 18) == 0 ||
		strncmp(start, "ptrace: ", 8) == 0)
	    {
		m_programActive = false;
	    }

	    // set message
	    char* endOfMessage = strchr(start, '\n');
	    if (endOfMessage == 0) {
		msg = start;
	    } else {
		msg = QString(start, endOfMessage-start);
	    }
	} else if (strncmp(start, "Breakpoint ", 11) == 0) {
	    /*
	     * We stopped at a (permanent) breakpoint (gdb doesn't tell us
	     * that it stopped at a temporary breakpoint).
	     */
	    refreshBreaklist = true;
	} else if (strstr(start, "re-reading symbols.") != 0) {
	    refreshNeeded = true;
	}

	// next line, please
	start = strchr(start, '\n');
    } while (start != 0);

    m_statusMessage = msg;
    emit updateStatusMessage();

    // refresh files if necessary
    if (refreshNeeded) {
	TRACE("re-reading files");
	emit executableUpdated();
    }

    /*
     * If we stopped at a breakpoint, we must update the breakpoint list
     * because the hit count changes. Also, if the breakpoint was temporary
     * it would go away now.
     */
    if (refreshBreaklist || refreshNeeded || m_bpTable.haveTemporaryBP()) {
	queueCmd(DCinfobreak, "info breakpoints", QMoverride);
    }

    /*
     * If we haven't listed the shared libraries yet, do so. We must do
     * this before we emit any commands that list variables, since the type
     * libraries depend on the shared libraries.
     */
    if (!m_sharedLibsListed) {
	// must be a high-priority command!
	executeCmd(DCinfosharedlib, "info sharedlibrary");
    }

    // get the backtrace if the program is running or if we have a core file
    if (m_programActive || !m_corefile.isEmpty()) {
	queueCmd(DCbt, "bt", QMoverride);
    } else {
	// program finished: erase PC
	emit updatePC(QString(), -1, 0);
    }

    m_programRunning = false;
}

void KDebugger::updateAllExprs()
{
    if (!m_programActive)
	return;

    // retrieve local variables
    queueCmd(DCinfolocals, "info locals", QMoverride);

    // update watch expressions
    KTreeViewItem* item = m_watchVariables.itemAt(0);
    for (; item != 0; item = item->getSibling()) {
	m_watchEvalExpr.append(static_cast<VarTree*>(item));
    }
}

void KDebugger::updateBreakptTable()
{
    m_bpTable.parseBreakpoint(m_gdbOutput);
    queueCmd(DCinfobreak, "info breakpoints", QMoverride);
}

void KDebugger::updateProgEnvironment(const char* args, const QDict<EnvVar>& newVars)
{
    m_programArgs = args;
    executeCmd(DCsetargs, "set args " + m_programArgs);
    TRACE("new pgm args: " + m_programArgs + "\n");

    QDictIterator<EnvVar> it = newVars;
    EnvVar* val;
    for (; (val = it) != 0; ++it) {
	switch (val->status) {
	case EnvVar::EVnew:
	    m_envVars.insert(it.currentKey(), val);
	    // fall thru
	case EnvVar::EVdirty:
	    // the value must be in our list
	    ASSERT(m_envVars[it.currentKey()] == val);
	    // update value
	    executeCmd(DCsetenv, QString("set env ") + it.currentKey() + " " + val->value);
	    break;
	case EnvVar::EVdeleted:
	    // must be in our list
	    ASSERT(m_envVars[it.currentKey()] == val);
	    // delete value
	    executeCmd(DCsetenv, QString("unset env ") + it.currentKey());
	    m_envVars.remove(it.currentKey());
	    break;
	default:
	    ASSERT(false);
	case EnvVar::EVclean:
	    // variable not changed
	    break;
	}
    }
}

void KDebugger::handleLocals()
{
    // retrieve old list of local variables
    QStrList oldVars;
    m_localVariables.exprList(oldVars);

    /*
     *  Parse local variables: that is a white-space separate list of
     *  variables.
     */
    QList<VarTree> newVars;
    parseLocals(newVars);

    /*
     * Clear any old VarTree item pointers, so that later we don't access
     * dangling pointers.
     */
    m_localVariables.clearPendingUpdates();

    // reduce flicker
    bool autoU = m_localVariables.autoUpdate();
    m_localVariables.setAutoUpdate(false);
    bool repaintNeeded = false;

    /*
     * Match old variables against new ones.
     */
    for (const char* n = oldVars.first(); n != 0; n = oldVars.next()) {
	// lookup this variable in the list of new variables
	VarTree* v = newVars.first();
	while (v != 0 && strcmp(v->getText(), n) != 0) {
	    v = newVars.next();
	}
	if (v == 0) {
	    // old variable not in the new variables
	    TRACE(QString("old var deleted: ") + n);
	    v = m_localVariables.topLevelExprByName(n);
	    removeExpr(&m_localVariables, v);
	    if (v != 0) repaintNeeded = true;
	} else {
	    // variable in both old and new lists: update
	    TRACE(QString("update var: ") + n);
	    m_localVariables.updateExpr(newVars.current());
	    // remove the new variable from the list
	    newVars.remove();
	    delete v;
	}
    }
    // insert all remaining new variables
    for (VarTree* v = newVars.first(); v != 0; v = newVars.next()) {
	TRACE("new var: " + v->getText());
	m_localVariables.insertExpr(v);
	repaintNeeded = true;
    }

    // repaint
    if (repaintNeeded && autoU && m_localVariables.isVisible())
	m_localVariables.repaint();
    m_localVariables.setAutoUpdate(autoU);
}

void KDebugger::parseLocals(QList<VarTree>& newVars)
{
    // check for possible error conditions
    if (strncmp(m_gdbOutput, "No symbol table", 15) == 0 ||
	strncmp(m_gdbOutput, "No locals", 9) == 0)
    {
	return;
    }
    QString origName;			/* used in renaming variables */
    const char* p = m_gdbOutput;
    while (*p != '\0') {
	VarTree* variable = parseVar(p);
	if (variable == 0) {
	    break;
	}
	// get some types
	variable->inferTypesOfChildren(*m_typeTable);
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
	origName = variable->getText();
	for (VarTree* v = newVars.first(); v != 0; v = newVars.next()) {
	    if (variable->getText() == v->getText()) {
		// we found a duplicate, change name
		block++;
#if QT_VERSION < 200
		QString newName(origName.length()+20);
#else
		QString newName;
#endif
		newName.sprintf("%s (%d)", origName.data(), block);
		variable->setText(newName);
	    }
	}
	newVars.append(variable);
    }
}

bool KDebugger::handlePrint(CmdQueueItem* cmd)
{
    ASSERT(cmd->m_expr != 0);

    VarTree* variable = parseExpr(cmd->m_expr->getText());
    if (variable == 0)
	return false;

    if (cmd->m_expr->m_varKind == VarTree::VKpointer) {
	/*
	 * We must insert a dummy parent, because otherwise variable's value
	 * would overwrite cmd->m_expr's value.
	 */
	VarTree* dummyParent = new VarTree(variable->getText(), VarTree::NKplain);
	dummyParent->m_varKind = VarTree::VKdummy;
	// the name of the parsed variable is the address of the pointer
	QString addr = "*" + cmd->m_expr->m_value;
	variable->setText(addr);
	variable->m_nameKind = VarTree::NKaddress;

	dummyParent->appendChild(variable);
	dummyParent->setDeleteChildren(true);
	TRACE("update ptr: " + cmd->m_expr->getText());
	cmd->m_exprWnd->updateExpr(cmd->m_expr, dummyParent);
	delete dummyParent;
    } else {
	TRACE("update expr: " + cmd->m_expr->getText());
	cmd->m_exprWnd->updateExpr(cmd->m_expr, variable);
	delete variable;
    }

    evalExpressions();			/* enqueue dereferenced pointers */

    return true;
}

static bool isErrorExpr(const char* output)
{
    return
	strncmp(output, "Cannot access memory at", 23) == 0 ||
	strncmp(output, "Attempt to dereference a generic pointer", 40) == 0 ||
	strncmp(output, "Attempt to take contents of ", 28) == 0 ||
	strncmp(output, "There is no member or method named", 34) == 0 ||
	strncmp(output, "No symbol \"", 11) == 0;
}

static bool parseOffErrorExpr(char* output, const char* name,
			      VarTree*& variable, bool wantErrorValue)
{
    if (isErrorExpr(output))
    {
	if (wantErrorValue) {
	    // put the error message as value in the variable
	    char* endMsg = strchr(output, '\n');
	    if (endMsg != 0)
		*endMsg = '\0';
	    variable = new VarTree(name, VarTree::NKplain);
	    variable->m_value = output;
	} else {
	    variable = 0;
	}
	return true;
    }
    return false;
}

VarTree* KDebugger::parseExpr(const char* name, bool wantErrorValue)
{
    VarTree* variable;

    // check for error conditions
    if (!parseOffErrorExpr(m_gdbOutput, name, variable, wantErrorValue))
    {
	// parse the variable
	const char* p = m_gdbOutput;
	variable = parseVar(p);
	if (variable == 0) {
	    return 0;
	}
	// set name
	variable->setText(name);
	// get some types
	variable->inferTypesOfChildren(*m_typeTable);
    }
    return variable;
}

#if QT_VERSION < 200
struct QChar {
    // this is the XChar2b on X11
    uchar row;
    uchar cell;
    static QString toQString(QChar* unicode, int len);
    QChar() : row(0), cell(0) { }
    QChar(char c) : row(0), cell(c) { }
    operator char() const { return row ? 0 : cell; }
};

QString QChar::toQString(QChar* unicode, int len)
{
    QString result(len+1);
    char* data = result.data();
    data[len] = '\0';
    while (len >= 0) {
	data[len] = unicode[len].cell;
	--len;
    }
    return result;
}
#endif

VarTree* KDebugger::parseQCharArray(const char* name, bool wantErrorValue)
{
    VarTree* variable = 0;

    /*
     * Parse off white space. gdb sometimes prints white space first if the
     * printed array leaded to an error.
     */
    char* p = m_gdbOutput;
    while (isspace(*p))
	p++;

    // check for error conditions
    if (parseOffErrorExpr(p, name, variable, wantErrorValue))
	return variable;

    // parse the array

    // find '='
    p = strchr(p, '=');
    if (p == 0) {
	goto error;
    }
    // skip white space
    do {
	p++;
    } while (isspace(*p));

    if (*p == '{')
    {
	// this is the real data
	p++;				/* skip '{' */

	// parse the array twice: once to get the length, again to create the string
	char* start = p;
	int realLen = 0;
	QString result;
	for (int fill = 0; fill <= 1; fill++) {
	    p = start;
	    if (fill) {
#if QT_VERSION < 200
		result.resize(realLen+2+1);	/* provide space for quotes */
		result[realLen+2] = '\0';
#endif
		result[realLen+1] = '\"';
		result[0] = '\"';
		realLen = 1;		/* skip quote */
	    }
	    while (isdigit(*p)) {
		// parse a number
		char* end;
		unsigned short value = (unsigned short) strtoul(p, &end, 0);
		if (end == p)
		    goto error;		/* huh? no valid digits */
		// skip separator and search for a repeat count
		p = end;
		while (isspace(*p) || *p == ',')
		    p++;
		int repeats = 1;
		if (strncmp(p, "<repeats ", 9) == 0) {
		    p += 9;
		    repeats = strtol(p, &end, 0);
		    if (end == p)
			goto error;	/* no valid digits */
		    p = end+7;		/* skip " times>" */
		    while (isspace(*p) || *p == ',')
			p++;
		}
		
		// interpret the value as a QChar
		// TODO: make cross-architecture compatible
		QChar ch;
		(unsigned short&)ch = value;

		// escape a few frequently used characters
		char escapeCode = '\0';
		switch (char(ch)) {
		case '\n': escapeCode = 'n'; break;
		case '\r': escapeCode = 'r'; break;
		case '\t': escapeCode = 't'; break;
		case '\b': escapeCode = 'b'; break;
		case '\"': escapeCode = '\"'; break;
		case '\\': escapeCode = '\\'; break;
#if QT_VERSION < 200
		    // since we only deal with ascii values must always escape '\0'
		case '\0': escapeCode = '0'; break;
#else
		case '\0': if (value == 0) { escapeCode = '0'; } break;
#endif
		}
		if (fill) {
		    while (repeats > 0) {
			if (escapeCode != '\0') {
			    result[realLen] = '\\';
			    ++realLen;
			    ch = escapeCode;
			}
			result[realLen] = ch;
			++realLen;
			--repeats;
		    }
		} else {
		    realLen += repeats;
		    if (escapeCode != '\0')
			realLen += repeats;
		}
	    }
	    if (*p != '}')
		goto error;
	} // while 2 passes
	TRACE(QString().sprintf("QCharArray: realLen=%d",realLen));
	// assign the value
	variable = new VarTree(name, VarTree::NKplain);
	variable->m_value = result;
    }
    else if (strncmp(p, "true", 4) == 0)
    {
	variable = new VarTree(name, VarTree::NKplain);
	variable->m_value = "QString::null";
    }
    else if (strncmp(p, "false", 5) == 0)
    {
	variable = new VarTree(name, VarTree::NKplain);
	variable->m_value = "(null)";
    }
    else
	goto error;
    return variable;

error:
    if (wantErrorValue) {
	variable = new VarTree(name, VarTree::NKplain);
	variable->m_value = "internal parse error";
    }
    return variable;
}

bool parseFrame(const char*& s, int& frameNo, QString& func, QString& file, int& lineNo)
{
    // Example:
    //  #1  0x8048881 in Dl::Dl (this=0xbffff418, r=3214) at testfile.cpp:72

    // must start with a hash mark followed by number
    if (s[0] != '#' || !isdigit(s[1]))
	return false;

    const char* p = s;
    p++;				/* skip the hash mark */
    // frame number
    frameNo = atoi(p);
    while (isdigit(*p))
	p++;
    // space
    while (isspace(*p))
	p++;
    // next may be a hexadecimal address
    if (*p == '0') {
	p++;
	if (*p == 'x')
	    p++;
	while (isxdigit(*p))
	    p++;
	if (strncmp(p, " in ", 4) == 0)
	    p += 4;
    }
    const char* start = p;
    // search opening parenthesis
goForParen:
    while (*p != '\0' && *p != '(')
	p++;
    if (*p == '\0') {
	func = start;
	file = "";
	lineNo = 0;
	s = p;
	return true;
    }
    // we might have hit the () of xyz::operator()
    if (p-start >= 10 && strncmp(p-8, "operator()", 10) == 0) {
	// skip over it and continue search
	p += 2;
	goto goForParen;
    }
    skipNestedWithString(p, '(', ')');
    while (isspace(*p))
	p++;
    // check for file position
    if (strncmp(p, "at ", 3) == 0) {
	p += 3;
	const char* fileStart = p;
	// go for the end of the line
	while (*p != '\0' && *p != '\n')
	    p++;
	// search back for colon
	const char* colon = p;
	do {
	    --colon;
	} while (*colon != ':');
	file = QString(fileStart, colon-fileStart+1);
	lineNo = atoi(colon+1);
	// skip new-line
	if (*p != '\0')
	    p++;
    } else {
	file = "";
	lineNo = 0;
    }
    // construct the function name (including file info)
    if (*p == '\0') {
	func = start;
    } else {
	func = QString(start, p-start);	/* don't include \n */
    }
    s = p;

    // replace \n (and whitespace around it) in func by a blank
    ASSERT(!isspace(func[0]));		/* there must be non-white before first \n */
    int nl = 0;
    while ((nl = func.find('\n', nl)) >= 0) {
	// search back to the beginning of the whitespace
	int startWhite = nl;
	do {
	    --startWhite;
	} while (isspace(func[startWhite]));
	startWhite++;
	// search forward to the end of the whitespace
	do {
	    nl++;
	} while (isspace(func[nl]));
	// replace
	func.replace(startWhite, nl-startWhite, " ");
	/* continue searching for more \n's at this place: */
	nl = startWhite+1;
    }
    return true;
}

// parse the output of bt
void KDebugger::handleBacktrace()
{
    const char* s = m_gdbOutput;

    QString func, file;
    int lineNo, frameNo;

    // reduce flicker
    m_btWindow.setAutoUpdate(false);

    m_btWindow.clear();
    if (parseFrame(s, frameNo, func, file, lineNo)) {
	// first frame must set PC
	TRACE("frame " + func + " (" + file + QString().sprintf(":%d)",lineNo));
	m_btWindow.insertItem(func);
	emit updatePC(file, lineNo-1, frameNo);

	while (parseFrame(s, frameNo, func, file, lineNo)) {
	    TRACE("frame " + func + " (" + file + QString().sprintf(":%d)",lineNo));
	    m_btWindow.insertItem(func);
	}
    }

    m_btWindow.setAutoUpdate(true);
    m_btWindow.repaint();
}

void KDebugger::gotoFrame(int frame)
{
    executeCmd(DCframe, QString().sprintf("frame %d", frame));
}

void KDebugger::handleFrameChange()
{
    QString func;
    QString fileName;
    int frameNo;
    int lineNo;
    const char* s = m_gdbOutput;
    if (parseFrame(s, frameNo, func, fileName, lineNo)) {
	/* lineNo can be 0 here if we can't find a file name */
	emit updatePC(fileName, lineNo-1, frameNo);
    } else {
	emit updatePC(fileName, -1, frameNo);
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
    //   pointers in 'this'
    //   types in 'this'
    
    VarTree* exprItem = m_watchEvalExpr.first();
    if (exprItem != 0) {
	m_watchEvalExpr.remove();
	QString expr = exprItem->computeExpr();
	TRACE("watch expr: " + expr);
	CmdQueueItem* cmd = queueCmd(DCprint, "print " + expr, QMoverride);
	// remember which expr this was
	cmd->m_expr = exprItem;
	cmd->m_exprWnd = &m_watchVariables;
    } else {
	ExprWnd* wnd;
	VarTree* exprItem;
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
    QString queueExpr = "print *(" + expr + ")";
    CmdQueueItem* cmd;
    if (immediate) {
	cmd = queueCmd(DCprint, queueExpr, QMoverrideMoreEqual);
    } else {
	cmd = queueCmd(DCprint, queueExpr, QMoverride);
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
    QString queueExpr = "whatis " + expr;
    CmdQueueItem* cmd;
    cmd = queueCmd(DCfindType, queueExpr, QMoverride);

    // remember which expr this was
    cmd->m_expr = exprItem;
    cmd->m_exprWnd = wnd;
}

void KDebugger::handleFindType(CmdQueueItem* cmd)
{
    const char* str = m_gdbOutput;
    if (strncmp(str, "type = ", 7) == 0) {
	str += 7;

	/*
	 * Everything else is the type. We strip off all white-space from
	 * the type.
	 */
	QString type = str;
	type.replace(QRegExp("\\s+"), "");

	ASSERT(cmd != 0 && cmd->m_expr != 0);

	TypeInfo* info = m_typeTable->lookup(type);

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
	    TRACE("unknown type");
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

void KDebugger::handlePrintStruct(CmdQueueItem* cmd)
{
    VarTree* var = cmd->m_expr;
    ASSERT(var != 0);
    ASSERT(var->m_varKind == VarTree::VKstruct);

    VarTree* partExpr;
    if (cmd->m_cmd != DCprintQStringStruct) {
	partExpr = parseExpr(cmd->m_expr->getText(), false);
    } else {
	partExpr = parseQCharArray(cmd->m_expr->getText(), false);
    }
    bool errorValue =
	partExpr == 0 ||
	/* we only allow simple values at the moment */
	partExpr->childCount() != 0;

    QString partValue;
    if (errorValue)
    {
	partValue = "???";
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
     * go on to the next index. (We must still add the ??? to the value).
     * 
     * Next, if this was the length expression, we still have not seen the
     * real expression, but the length of a QString.
     */
    ASSERT(var->m_exprIndex >= 0 && var->m_exprIndex <= typeInfoMaxExpr);

    if (errorValue || !var->m_exprIndexUseGuard)
    {
	// add current partValue (which might be ???)
#if QT_VERSION < 200
	var->m_partialValue.detach();
#endif
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
    var->m_exprIndexUseGuard = true;
    var->m_partialValue = var->m_type->m_displayString[0];
    evalStructExpression(var, wnd, immediate);
}

/* queues a printStruct command; var must have been initialized correctly */
void KDebugger::evalStructExpression(VarTree* var, ExprWnd* wnd, bool immediate)
{
    QString base = var->computeExpr();
    QString exprFmt;
    if (var->m_exprIndexUseGuard) {
	exprFmt = var->m_type->m_guardStrings[var->m_exprIndex];
	if (exprFmt.isEmpty()) {
	    // no guard, omit it and go to expression
	    var->m_exprIndexUseGuard = false;
	}
    }
    if (!var->m_exprIndexUseGuard) {
	exprFmt = var->m_type->m_exprStrings[var->m_exprIndex];
    }

#if QT_VERSION < 200
    QString expr(exprFmt.length() + base.length() + 10);
#else
    QString expr;
#endif
    expr.sprintf(exprFmt, base.data());

    DbgCommand dbgCmd = DCprintStruct;
    // check if this is a QString::Data
    if (strncmp(expr, "/QString::Data ", 15) == 0)
    {
	if (m_typeTable->parseQt2QStrings())
	{
	    expr = expr.mid(15, expr.length());	/* strip off /QString::Data */
	    expr =
		// if the string data is junk, fail early
		"print ($qstrunicode=($qstrdata=(" + expr + "))->unicode)?"
		// print an array of shorts
		"(*(unsigned short*)$qstrunicode)@"
		// limit the length and add 1 because length 0 is an error
		"(($qstrlen=(unsigned int)($qstrdata->len))>100?100:$qstrlen)"
		// if unicode data is 0, check if it is QString::null
		":($qstrdata==QString::null.d)";
	    dbgCmd = DCprintQStringStruct;
	} else {
	    /*
	     * This should not happen: the type libraries should be set up
	     * in a way that this can't happen. If this happens
	     * nevertheless it means that, eg., kdecore was loaded but qt2
	     * was not (only qt2 enables the QString feature).
	     */
	    // TODO: remove this "print"; queue the next printStruct instead
	    expr = "print *0";
	}
    } else {
	expr = "print " + expr;
    }
    TRACE("evalStruct: " + expr + (var->m_exprIndexUseGuard ? " // guard" : " // real"));
    CmdQueueItem* cmd = queueCmd(dbgCmd, expr,
				 immediate  ?  QMoverrideMoreEqual : QMnormal);

    // remember which expression this was
    cmd->m_expr = var;
    cmd->m_exprWnd = wnd;
}

/* removes expression from window */
void KDebugger::removeExpr(ExprWnd* wnd, VarTree* var)
{
    if (var == 0)
	return;

    // must remove any references to var from command queues

    /*
     * Check the low-priority queue: We start at the back end, but skip the
     * last element for now. The reason is that if we delete an element the
     * current element is stepped to the next one - except if it's on the
     * last: then it's stepped to the previous element. By checking the
     * last element separately we avoid that special case.
     */
    CmdQueueItem* cmd = m_lopriCmdQueue.last();
    while ((cmd = m_lopriCmdQueue.prev()) != 0) {
	if (cmd->m_expr != 0 && var->isAncestorEq(cmd->m_expr)) {
	    // this is indeed a critical command; delete it
	    TRACE("removing critical lopri-cmd: " + cmd->m_cmdString);
	    m_lopriCmdQueue.remove();	/* steps to next element */
	}
    }
    cmd = m_lopriCmdQueue.last();
    if (cmd != 0) {
	if (cmd->m_expr != 0 && var->isAncestorEq(cmd->m_expr)) {
	    TRACE("removing critical lopri-cmd: " + cmd->m_cmdString);
	    m_lopriCmdQueue.remove();	/* steps to next element */
	}
    }

    wnd->removeExpr(var);
}

void KDebugger::handleSharedLibs()
{
    // delete all known libraries
    m_sharedLibs.clear();

    // parse the table of shared libraries
    if (strncmp(m_gdbOutput, "No shared libraries loaded", 26) != 0) {
	const char* str = m_gdbOutput;
	// strip off head line
	str = strchr(str, '\n');
	if (str == 0)
	    goto doneParse;
	str++;				/* skip '\n' */
	QString shlibName;
	while (str != 0 && *str != '\0') {
	    // format of a line is
	    // 0x404c5000  0x40580d90  Yes         /lib/libc.so.5
	    // 3 blocks of non-space followed by space
	    for (int i = 0; *str != '\0' && i < 3; i++) {
		while (*str != '\0' && !isspace(*str)) {   /* non-space */
		    str++;
		}
		while (isspace(*str)) {	/* space */
		    str++;
		}
	    }
	    if (*str == '\0')
		goto doneParse;
	    const char* start = str;
	    str = strchr(str, '\n');
	    if (str == 0) {
		shlibName = start;
	    } else {
		shlibName = QString(start, str-start+1);
		str++;
	    }
	    m_sharedLibs.append(shlibName);
	    TRACE("found shared lib " + shlibName);
	}
    }
doneParse:;
    m_sharedLibsListed = true;

    // get type libraries
    m_typeTable->loadLibTypes(m_sharedLibs);
}

KDebugger::CmdQueueItem* KDebugger::loadCoreFile()
{
    if (m_gdbMajor > 4 || (m_gdbMajor == 4 && m_gdbMinor >= 16)) {
	return queueCmd(DCcorefile, "target core " + m_corefile, QMoverride);
    } else {
	return queueCmd(DCcorefile, "core-file " + m_corefile, QMoverride);
    }
}

void KDebugger::slotLocalsExpanding(KTreeViewItem* item, bool& allow)
{
    exprExpandingHelper(&m_localVariables, item, allow);
}

void KDebugger::slotWatchExpanding(KTreeViewItem* item, bool& allow)
{
    exprExpandingHelper(&m_watchVariables, item, allow);
}

void KDebugger::exprExpandingHelper(ExprWnd* wnd, KTreeViewItem* item, bool&)
{
    VarTree* exprItem = static_cast<VarTree*>(item);
    if (exprItem->m_varKind != VarTree::VKpointer) {
	return;
    }
    dereferencePointer(wnd, exprItem, true);
}

// add the expression in the edit field to the watch expressions
void KDebugger::addWatch(const QString& t)
{
    QString expr = t.stripWhiteSpace();
    if (expr.isEmpty())
	return;
    VarTree* exprItem = new VarTree(expr, VarTree::NKplain);
    m_watchVariables.insertExpr(exprItem);
    
    m_watchEvalExpr.append(exprItem);

    // if we are boring ourselves, send down the command
    if (m_state == DSidle) {
	evalExpressions();
    }
}

// delete a toplevel watch expression
void KDebugger::slotDeleteWatch()
{
    // delete only allowed while debugger is idle; or else we might delete
    // the very expression the debugger is currently working on...
    if (m_state != DSidle)
	return;

    int index = m_watchVariables.currentItem();
    if (index < 0)
	return;
    
    VarTree* item = static_cast<VarTree*>(m_watchVariables.itemAt(index));
    if (!item->isToplevelExpr())
	return;

    // remove the variable from the list to evaluate
    if (m_watchEvalExpr.findRef(item) >= 0) {
	m_watchEvalExpr.remove();
    }
    removeExpr(&m_watchVariables, item);
    // item is invalid at this point!
}

void KDebugger::startAnimation(bool fast)
{
    int interval = fast ? 50 : 150;
    if (!m_animationTimer.isActive()) {
	m_animationTimer.start(interval);
    } else if (m_animationInterval != interval) {
	m_animationTimer.changeInterval(interval);
    }
    m_animationInterval = interval;
}

void KDebugger::stopAnimation()
{
    if (m_animationTimer.isActive()) {
	m_animationTimer.stop();
	m_animationInterval = 0;
    }
}

void KDebugger::slotToggleBreak(const QString& fileName, int lineNo)
{
    // lineNo is zero-based
    if (isReady()) {
	m_bpTable.doBreakpoint(fileName, lineNo, false);
    }
}

void KDebugger::slotEnaDisBreak(const QString& fileName, int lineNo)
{
    // lineNo is zero-based
    if (isReady()) {
	m_bpTable.doEnableDisableBreakpoint(fileName, lineNo);
    }
}

void KDebugger::slotUpdateAnimation()
{
    if (m_state == DSidle) {
	stopAnimation();
    } else {
	/*
	 * Slow animation while program is stopped (i.e. while variables
	 * are displayed)
	 */
	bool slow = isReady() && m_programActive && !m_programRunning;
	startAnimation(!slow);
    }
}

#include "debugger.moc"
