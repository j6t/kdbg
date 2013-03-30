/*
 * Copyright Johannes Sixt
 * This file is licensed under the GNU General Public License Version 2.
 * See the file COPYING in the toplevel directory of the source directory.
 */

#include "dbgdriver.h"
#include "exprwnd.h"
#include <QStringList>
#include <ctype.h>
#include <signal.h>
#include <stdlib.h>			/* strtol, atoi */
#include <algorithm>
#include "mydebug.h"
#include <assert.h>


DebuggerDriver::DebuggerDriver() :
	m_state(DSidle),
	m_activeCmd(0)
{
    // debugger process
    connect(this, SIGNAL(readyReadStandardOutput()), SLOT(slotReceiveOutput()));
    connect(this, SIGNAL(bytesWritten(qint64)), SLOT(slotCommandRead()));
    connect(this, SIGNAL(finished(int, QProcess::ExitStatus)),
	    SLOT(slotExited()));
}

DebuggerDriver::~DebuggerDriver()
{
    flushHiPriQueue();
    flushLoPriQueue();
}


bool DebuggerDriver::startup(QString cmdStr)
{
    // clear command queues
    delete m_activeCmd;
    m_activeCmd = 0;
    flushHiPriQueue();
    flushLoPriQueue();
    m_state = DSidle;

    // debugger executable
    if (cmdStr.isEmpty())
	cmdStr = defaultInvocation();

    QStringList cmd = cmdStr.split(' ', QString::SkipEmptyParts);
    if (cmd.isEmpty())
	return false;
    QString pgm = cmd.takeFirst();

#if QT_VERSION >= QT_VERSION_CHECK(4, 6, 0)
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert(QLatin1String("LC_ALL"), QLatin1String("C"));
    env.remove(QLatin1String("LANG"));
    setProcessEnvironment(env);
#endif

    setProcessChannelMode(MergedChannels);
    start(pgm, cmd);
    if (!waitForStarted(-1))
	return false;

    // open log file
    if (!m_logFile.isOpen() && !m_logFileName.isEmpty()) {
	m_logFile.setFileName(m_logFileName);
	m_logFile.open(QIODevice::WriteOnly);
    }

    return true;
}

void DebuggerDriver::slotExited()
{
    static const char txt[] = "\n====== debugger exited ======\n";
    if (m_logFile.isOpen()) {
	m_logFile.write(txt,sizeof(txt)-1);
    }

    // reset state
    m_state = DSidle;
    // empty buffer
    m_output.clear();
}


CmdQueueItem* DebuggerDriver::executeCmdString(DbgCommand cmd,
					       QString cmdString, bool clearLow)
{
    // place a new command into the high-priority queue
    CmdQueueItem* cmdItem = new CmdQueueItem(cmd, cmdString);
    m_hipriCmdQueue.push(cmdItem);

    if (clearLow) {
	if (m_state == DSrunningLow) {
	    // take the liberty to interrupt the running command
	    m_state = DSinterrupted;
	    ::kill(pid(), SIGINT);
	    ASSERT(m_activeCmd != 0);
	    TRACE(QString().sprintf("interrupted the command %d",
		  (m_activeCmd ? m_activeCmd->m_cmd : -1)));
	    delete m_activeCmd;
	    m_activeCmd = 0;
	}
	flushLoPriQueue();
    }
    // if gdb is idle, send it the command
    if (m_state == DSidle) {
	ASSERT(m_activeCmd == 0);
	writeCommand();
    }

    return cmdItem;
}

bool CmdQueueItem::IsEqualCmd::operator()(CmdQueueItem* cmd) const
{
    return cmd->m_cmd == m_cmd && cmd->m_cmdString == m_str;
}

CmdQueueItem* DebuggerDriver::queueCmdString(DbgCommand cmd,
					     QString cmdString, QueueMode mode)
{
    // place a new command into the low-priority queue
    std::list<CmdQueueItem*>::iterator i;
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
	i = find_if(m_lopriCmdQueue.begin(), m_lopriCmdQueue.end(), CmdQueueItem::IsEqualCmd(cmd, cmdString));
	if (i != m_lopriCmdQueue.end()) {
	    // found one
	    cmdItem = *i;
	    if (mode == QMoverrideMoreEqual) {
		// All commands are equal, but some are more equal than others...
		// put this command in front of all others
		m_lopriCmdQueue.erase(i);
		m_lopriCmdQueue.push_front(cmdItem);
	    }
	    break;
	} // else none found, so add it
	// drop through
    case QMnormal:
	cmdItem = new CmdQueueItem(cmd, cmdString);
	m_lopriCmdQueue.push_back(cmdItem);
    }

    // if gdb is idle, send it the command
    if (m_state == DSidle) {
	ASSERT(m_activeCmd == 0);
	writeCommand();
    }

    return cmdItem;
}

// dequeue a pending command, make it the active one and send it to gdb
void DebuggerDriver::writeCommand()
{
//    ASSERT(m_activeCmd == 0);
    assert(m_activeCmd == 0);

    // first check the high-priority queue - only if it is empty
    // use a low-priority command.
    CmdQueueItem* cmd;
    DebuggerState newState = DScommandSent;
    if (!m_hipriCmdQueue.empty()) {
	cmd = m_hipriCmdQueue.front();
	m_hipriCmdQueue.pop();
    } else if (!m_lopriCmdQueue.empty()) {
	cmd = m_lopriCmdQueue.front();
	m_lopriCmdQueue.pop_front();
	newState = DScommandSentLow;
    } else {
	// nothing to do
	m_state = DSidle;		/* is necessary if command was interrupted earlier */
	return;
    }

    m_activeCmd = cmd;
    TRACE("in writeCommand: " + cmd->m_cmdString);

    QByteArray str = cmd->m_cmdString.toLocal8Bit();
    const char* data = str.data();
    qint64 len = str.length();
    while (len > 0) {
	qint64 n = write(data, len);
	if (n <= 0)
	    break;	// ignore error
	len -= n;
	data += n;
    }

    // write also to log file
    if (m_logFile.isOpen()) {
	m_logFile.write(str);
	m_logFile.flush();
    }

    m_state = newState;
}

void DebuggerDriver::flushLoPriQueue()
{
    while (!m_lopriCmdQueue.empty()) {
	delete m_lopriCmdQueue.back();
	m_lopriCmdQueue.pop_back();
    }
}

void DebuggerDriver::flushHiPriQueue()
{
    while (!m_hipriCmdQueue.empty()) {
	delete m_hipriCmdQueue.front();
	m_hipriCmdQueue.pop();
    }
}

void DebuggerDriver::flushCommands(bool hipriOnly)
{
    flushHiPriQueue();
    if (!hipriOnly) {
	flushLoPriQueue();
    }
}

void DebuggerDriver::slotCommandRead()
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
	emit inferiorRunning();
	break;
    default:
	break;
    }

    // process delayed output
    while (!m_delayedOutput.empty()) {
	QByteArray delayed = m_delayedOutput.front();
	m_delayedOutput.pop();
	processOutput(delayed);
    }
}

void DebuggerDriver::slotReceiveOutput()
{
    QByteArray data = readAllStandardOutput();

    /*
     * The debugger should be running (processing a command) at this point.
     * If it is not, it is still idle because we haven't received the
     * bytesWritten signal yet, in which case there must be an active command
     * which is not commited.
     */
    if (m_state == DScommandSent || m_state == DScommandSentLow) {
	ASSERT(m_activeCmd != 0);
	ASSERT(!m_activeCmd->m_committed);
	/*
	 * We received output before we got signal bytesWritten. Collect this
	 * output, it will be processed by commandRead when it gets the
	 * acknowledgment for the uncommitted command.
	 */
	m_delayedOutput.push(data);
	return;
    }
    processOutput(data);
}

void DebuggerDriver::processOutput(const QByteArray& data)
{
    // write to log file (do not log delayed output - it would appear twice)
    if (m_logFile.isOpen()) {
	m_logFile.write(data);
	m_logFile.flush();
    }
    
    /*
     * gdb sometimes produces stray output while it's idle. This happens if
     * it receives a signal, most prominently a SIGCONT after a SIGTSTP:
     * The user haltet kdbg with Ctrl-Z, then continues it with "fg", which
     * also continues gdb, which repeats the prompt!
     */
    if (m_activeCmd == 0 && m_state != DSinterrupted) {
	// ignore the output
	TRACE("ignoring stray output: " + QString(data));
	return;
    }
    ASSERT(m_state == DSrunning || m_state == DSrunningLow || m_state == DSinterrupted);
    ASSERT(m_activeCmd != 0 || m_state == DSinterrupted);

    // collect output until next prompt string is found
    
    // accumulate it
    m_output += data;

    // check for a prompt
    int promptStart = findPrompt(m_output);
    if (promptStart >= 0)
    {
	// found prompt!

	// terminate output before the prompt
	m_output.resize(promptStart);

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
	    commandFinished(cmd);
	    delete cmd;
	}

	// empty buffer
	m_output.clear();
	// also clear delayed output if interrupted
	if (m_state == DSinterrupted) {
	    m_delayedOutput = std::queue<QByteArray>();
	}

	/*
	 * We parsed some output successfully. Unless there's more delayed
	 * output, the debugger must be idle now, so send down the next
	 * command.
	 */
	if (m_delayedOutput.empty()) {
	    if (m_hipriCmdQueue.empty() && m_lopriCmdQueue.empty()) {
		// no pending commands
		m_state = DSidle;
		emit enterIdleState();
	    } else {
		writeCommand();
	    }
	}
    }
}

void DebuggerDriver::dequeueCmdByVar(VarTree* var)
{
    if (var == 0)
	return;

    std::list<CmdQueueItem*>::iterator i = m_lopriCmdQueue.begin();
    while (i != m_lopriCmdQueue.end()) {
	if ((*i)->m_expr != 0 && var->isAncestorEq((*i)->m_expr)) {
	    // this is indeed a critical command; delete it
	    TRACE("removing critical lopri-cmd: " + (*i)->m_cmdString);
	    delete *i;
	    m_lopriCmdQueue.erase(i++);
	} else
	    ++i;
    }
}


QString DebuggerDriver::editableValue(VarTree* value)
{
    // by default, let the user edit what is visible
    return value->value();
}


StackFrame::~StackFrame()
{
    delete var;
}


DbgAddr::DbgAddr(const QString& aa) :
	a(aa)
{
    cleanAddr();
}

/*
 * We strip off the leading 0x and any leading zeros.
 */
void DbgAddr::cleanAddr()
{
    if (a.isEmpty())
	return;

    while (a[0] == '0' || a[0] == 'x') {
	a.remove(0, 1);
    }
}

void DbgAddr::operator=(const QString& aa)
{
    a = aa;
    fnoffs = QString();
    cleanAddr();
}

/* Re-attach 0x in front of the address */
QString DbgAddr::asString() const
{
    if (a.isEmpty())
	return QString();
    else
	return "0x" + a;
}

bool operator==(const DbgAddr& a1, const DbgAddr& a2)
{
    return QString::compare(a1.a, a2.a) == 0;
}

bool operator>(const DbgAddr& a1, const DbgAddr& a2)
{
    if (a1.a.length() > a2.a.length())
	return true;
    if (a1.a.length() < a2.a.length())
	return false;
    return QString::compare(a1.a, a2.a) > 0;
}


Breakpoint::Breakpoint() :
	id(0),
	type(breakpoint),
	temporary(false),
	enabled(true),
	ignoreCount(0),
	hitCount(0),
	lineNo(0)
{ }

#include "dbgdriver.moc"
