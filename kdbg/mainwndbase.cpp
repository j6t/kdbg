// $Id$

// Copyright by Johannes Sixt
// This file is under GPL, the GNU General Public Licence

#include <kapp.h>
#if QT_VERSION >= 200
#include <klocale.h>			/* i18n */
#include <kinstance.h>
#include <kconfig.h>
#include <kmessagebox.h>
#else
#include <kmsgbox.h>
#endif
#include <kiconloader.h>
#include <kstatusbar.h>
#include <ktoolbar.h>
#include <kfiledialog.h>
#include <qpainter.h>
#include <qtabdialog.h>
#include <qfileinfo.h>
#include "mainwndbase.h"
#include "debugger.h"
#include "gdbdriver.h"
#include "prefdebugger.h"
#include "updateui.h"
#include "commandids.h"
#ifdef HAVE_CONFIG
#include "config.h"
#endif
#include "mydebug.h"
#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>			/* mknod(2) */
#endif
#ifdef HAVE_FCNTL_H
#include <fcntl.h>			/* open(2) */
#endif
#ifdef HAVE_UNISTD_H
#include "unistd.h"			/* getpid, unlink, fork etc. */
#endif
#include <signal.h>


KStdAccel* keys = 0;

WatchWindow::WatchWindow(QWidget* parent, const char* name, WFlags f) :
	QWidget(parent, name, f),
	m_watchEdit(this, "watch_edit"),
	m_watchAdd(i18n(" Add "), this, "watch_add"),
	m_watchDelete(i18n(" Del "), this, "watch_delete"),
	m_watchVariables(this, "watch_variables"),
	m_watchV(this, 0),
	m_watchH(0)
{
    // setup the layout
    m_watchAdd.setMinimumSize(m_watchAdd.sizeHint());
    m_watchDelete.setMinimumSize(m_watchDelete.sizeHint());
    m_watchV.addLayout(&m_watchH, 0);
    m_watchV.addWidget(&m_watchVariables, 10);
    m_watchH.addWidget(&m_watchEdit, 10);
    m_watchH.addWidget(&m_watchAdd, 0);
    m_watchH.addWidget(&m_watchDelete, 0);

    connect(&m_watchEdit, SIGNAL(returnPressed()), SIGNAL(addWatch()));
    connect(&m_watchAdd, SIGNAL(clicked()), SIGNAL(addWatch()));
    connect(&m_watchDelete, SIGNAL(clicked()), SIGNAL(deleteWatch()));
    connect(&m_watchVariables, SIGNAL(highlighted(int)), SLOT(slotWatchHighlighted(int)));
}

WatchWindow::~WatchWindow()
{
}

// place the text of the hightlighted watch expr in the edit field
void WatchWindow::slotWatchHighlighted(int idx)
{
    QString text = m_watchVariables.exprStringAt(idx);
    m_watchEdit.setText(text);
}


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


const char defaultTermCmdStr[] = "xterm -name kdbgio -title %T -e sh -c %C";
const char defaultDebuggerCmdStr[] =
	"gdb"
	" --fullname"	/* to get standard file names each time the prog stops */
	" --nx";	/* do not execute initialization files */


DebuggerMainWndBase::DebuggerMainWndBase() :
	m_animationCounter(0),
	m_outputTermCmdStr(defaultTermCmdStr),
	m_outputTermPID(0),
	m_debuggerCmdStr(defaultDebuggerCmdStr),
	m_debugger(0)
{
    m_statusActive = i18n("active");
}

DebuggerMainWndBase::~DebuggerMainWndBase()
{
    delete m_debugger;

    // if the output window is open, close it
    if (m_outputTermPID != 0) {
	::kill(m_outputTermPID, SIGTERM);
    }
}

void DebuggerMainWndBase::setupDebugger(ExprWnd* localVars,
					ExprWnd* watchVars,
					QListBox* backtrace)
{
    QWidget* parent = dbgMainWnd();

    GdbDriver* driver = new GdbDriver;
#ifdef GDB_TRANSCRIPT
    driver->setLogFileName(GDB_TRANSCRIPT);
#endif
    m_debugger = new KDebugger(parent, localVars, watchVars, backtrace, driver);

    QObject::connect(m_debugger, SIGNAL(updateStatusMessage()),
		     parent, SLOT(slotNewStatusMsg()));
    QObject::connect(m_debugger, SIGNAL(updateUI()),
		     parent, SLOT(updateUI()));

    QObject::connect(m_debugger, SIGNAL(lineItemsChanged()),
		     parent, SLOT(updateLineItems()));
    
    QObject::connect(m_debugger, SIGNAL(animationTimeout()),
		     parent, SLOT(slotAnimationTimeout()));
    QObject::connect(m_debugger, SIGNAL(debuggerStarting()),
		     parent, SLOT(slotDebuggerStarting()));

    ValArray<QString> cmd;
    splitCmdStr(m_debuggerCmdStr, cmd);
    m_debugger->setDebuggerCmd(cmd);
}


void DebuggerMainWndBase::setCoreFile(const QString& corefile)
{
    assert(m_debugger != 0);
    m_debugger->useCoreFile(corefile, true);
}

void DebuggerMainWndBase::setRemoteDevice(const QString& remoteDevice)
{
    if (m_debugger != 0) {
	m_debugger->setRemoteDevice(remoteDevice);
    }
}

const char OutputWindowGroup[] = "OutputWindow";
const char TermCmdStr[] = "TermCmdStr";
const char KeepScript[] = "KeepScript";
const char DebuggerGroup[] = "Debugger";
const char DebuggerCmdStr[] = "DebuggerCmdStr";

void DebuggerMainWndBase::saveSettings(KConfig* config)
{
    if (m_debugger != 0) {
	m_debugger->saveSettings(config);
    }

    KConfigGroupSaver g(config, OutputWindowGroup);
    config->writeEntry(TermCmdStr, m_outputTermCmdStr);

    config->setGroup(DebuggerGroup);
    config->writeEntry(DebuggerCmdStr, m_debuggerCmdStr);
}

void DebuggerMainWndBase::restoreSettings(KConfig* config)
{
    if (m_debugger != 0) {
	m_debugger->restoreSettings(config);
    }

    KConfigGroupSaver g(config, OutputWindowGroup);
    /*
     * For debugging and emergency purposes, let the config file override
     * the shell script that is used to keep the output window open. This
     * string must have EXACTLY 1 %s sequence in it.
     */
    setTerminalCmd(config->readEntry(TermCmdStr, defaultTermCmdStr));
    m_outputTermKeepScript = config->readEntry(KeepScript);

    config->setGroup(DebuggerGroup);
    setDebuggerCmdStr(config->readEntry(DebuggerCmdStr));
}

bool DebuggerMainWndBase::debugProgram(const QString& executable)
{
    assert(m_debugger != 0);
    return m_debugger->debugProgram(executable);
}

// helper that gets a file name (it only differs in the caption of the dialog)
static QString getFileName(QString caption,
			   QString dir, QString filter,
			   QWidget* parent)
{
    QString filename;
    KFileDialog dlg(dir, filter, parent, 0, true);

    dlg.setCaption(caption);

    if (dlg.exec() == QDialog::Accepted)
	filename = dlg.selectedFile();

    return filename;
}

bool DebuggerMainWndBase::handleCommand(int item)
{
    /* first commands that don't require the debugger */
    switch (item) {
    case ID_FILE_GLOBAL_OPTIONS:
	slotGlobalOptions();
	return true;
    }

    // now commands that do
    if (m_debugger == 0)
	return false;

    switch (item) {
    case ID_FILE_EXECUTABLE:
	if (m_debugger->isIdle())
	{
	    // open a new executable
	    QString executable = getFileName(i18n("Select the executable to debug"),
					     m_lastDirectory, 0, dbgMainWnd());
	    if (executable.isEmpty())
		return true;

	    // check the file name
	    QFileInfo fi(executable);
	    m_lastDirectory = fi.dirPath(true);

	    if (!fi.isFile()) {
		QString msgFmt = i18n("`%s' is not a file or does not exist");
		SIZED_QString(msg, msgFmt.length() + executable.length() + 20);
#if QT_VERSION < 200
		msg.sprintf(msgFmt, executable.data());
		KMsgBox::message(dbgMainWnd(), kapp->appName(),
				 msg,
				 KMsgBox::STOP,
				 i18n("OK"));
#else
		msg.sprintf(msgFmt, executable.toLatin1());
		KMessageBox::sorry(dbgMainWnd(), msg);
#endif
		return true;
	    }

	    if (!m_debugger->debugProgram(executable)) {
		QString msg = i18n("Could not start the debugger process.\n"
				   "Please shut down KDbg and resolve the problem.");
#if QT_VERSION < 200
		KMsgBox::message(dbgMainWnd(), kapp->appName(),
				 msg,
				 KMsgBox::STOP,
				 i18n("OK"));
#else
		KMessageBox::sorry(dbgMainWnd(), msg);
#endif
	    }
	}
	return true;
    case ID_FILE_COREFILE:
	if (m_debugger->canUseCoreFile())
	{
	    QString corefile = getFileName(i18n("Select core dump"),
					   m_lastDirectory, 0, dbgMainWnd());
	    if (!corefile.isEmpty()) {
		m_debugger->useCoreFile(corefile, false);
	    }
	}
	return true;
    case ID_PROGRAM_RUN:
	m_debugger->programRun();
	return true;
    case ID_PROGRAM_ATTACH:
	m_debugger->programAttach();
	return true;
    case ID_PROGRAM_RUN_AGAIN:
	m_debugger->programRunAgain();
	return true;
    case ID_PROGRAM_STEP:
	m_debugger->programStep();
	return true;
    case ID_PROGRAM_NEXT:
	m_debugger->programNext();
	return true;
    case ID_PROGRAM_FINISH:
	m_debugger->programFinish();
	return true;
    case ID_PROGRAM_KILL:
	m_debugger->programKill();
	return true;
    case ID_PROGRAM_BREAK:
	m_debugger->programBreak();
	return true;
    case ID_PROGRAM_ARGS:
	m_debugger->programArgs();
	return true;
    }
    return false;
}

void DebuggerMainWndBase::updateUIItem(UpdateUI* item)
{
    switch (item->id) {
    case ID_FILE_EXECUTABLE:
	item->enable(m_debugger->isIdle());
	break;
    case ID_FILE_COREFILE:
	item->enable(m_debugger->canUseCoreFile());
	break;
    case ID_PROGRAM_STEP:
    case ID_PROGRAM_NEXT:
    case ID_PROGRAM_FINISH:
    case ID_PROGRAM_UNTIL:
    case ID_PROGRAM_RUN_AGAIN:
	item->enable(m_debugger->canSingleStep());
	break;
    case ID_PROGRAM_ATTACH:
    case ID_PROGRAM_RUN:
	item->enable(m_debugger->isReady());
	break;
    case ID_PROGRAM_KILL:
	item->enable(m_debugger->haveExecutable() && m_debugger->isProgramActive());
	break;
    case ID_PROGRAM_BREAK:
	item->enable(m_debugger->isProgramRunning());
	break;
    case ID_PROGRAM_ARGS:
	item->enable(m_debugger->haveExecutable());
	break;
    }
    
    // update statusbar
    dbgStatusBar()->changeItem(m_debugger->isProgramActive() ?
			    static_cast<const char*>(m_statusActive) : "",
			    ID_STATUS_ACTIVE);
}

void DebuggerMainWndBase::updateLineItems()
{
}

void DebuggerMainWndBase::initAnimation()
{
#if QT_VERSION < 200
    QString path = kapp->kde_datadir() + "/kfm/pics/";
    QPixmap pixmap;
    pixmap.load(path + "/kde1.xpm");
#else
    KInstance instance("kdbg");
    QPixmap pixmap = BarIcon("kde1", &instance);
#endif

    KToolBar* toolbar = dbgToolBar();
    toolbar->insertButton(pixmap, ID_STATUS_BUSY);
    toolbar->alignItemRight(ID_STATUS_BUSY, true);
    
    // Load animated logo
    m_animation.setAutoDelete(true);
    QString n;
    for (int i = 1; i <= 9; i++) {
#if QT_VERSION < 200
	n.sprintf("/kde%d.xpm", i);
	QPixmap* p = new QPixmap();
	p->load(path + n);
#else
	n.sprintf("kde%d", i);
	QPixmap* p = new QPixmap(BarIcon(n,&instance));
#endif
	if (!p->isNull()) {
	    m_animation.append(p);
	} else {
	    delete p;
	}
    }
    // safeguard: if we did not find a single icon, add a dummy
    if (m_animation.count() == 0) {
	QPixmap* pix = new QPixmap(2,2);
	QPainter p(pix);
#if QT_VERSION < 200
	p.fillRect(0,0,2,2,QBrush(white));
#else
	p.fillRect(0,0,2,2,QBrush(Qt::white));
#endif
	m_animation.append(pix);
    }
}

void DebuggerMainWndBase::slotAnimationTimeout()
{
    assert(m_animation.count() > 0);	/* must have been initialized */
    m_animationCounter++;
    if (m_animationCounter == m_animation.count())
	m_animationCounter = 0;
    dbgToolBar()->setButtonPixmap(ID_STATUS_BUSY,
			       *m_animation.at(m_animationCounter));
}

void DebuggerMainWndBase::slotNewStatusMsg()
{
    QString msg = m_debugger->statusMessage();
    dbgStatusBar()->changeItem(msg, ID_STATUS_MSG);
}

void DebuggerMainWndBase::slotGlobalOptions()
{
    QTabDialog dlg(dbgMainWnd(), "global_options", true);
    QString title = kapp->getCaption();
    title += i18n(": Global options");
    dlg.setCaption(title);
    dlg.setCancelButton(i18n("Cancel"));
    dlg.setOKButton(i18n("OK"));

    PrefDebugger prefDebugger(&dlg);
    prefDebugger.setDebuggerCmd(m_debuggerCmdStr);
    prefDebugger.setTerminal(m_outputTermCmdStr);
    
    dlg.addTab(&prefDebugger, "&Debugger");
    if (dlg.exec() == QDialog::Accepted)
    {
	setDebuggerCmdStr(prefDebugger.debuggerCmd());
	setTerminalCmd(prefDebugger.terminal());
    }
}

const char fifoNameBase[] = "/tmp/kdbgttywin%05d";

/*
 * We use the scope operator :: in this function, so that we don't
 * accidentally use the wrong close() function (I've been bitten ;-),
 * outch!) (We use it for all the libc functions, to be consistent...)
 */
bool DebuggerMainWndBase::createOutputWindow()
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
	ttyname[n] = '\0';
	QString tty = ttyname;
	m_outputTermName = tty.stripWhiteSpace();
	m_outputTermPID = pid;
	TRACE(QString().sprintf("tty=%s", m_outputTermName.data()));
    }
    return true;
}

void DebuggerMainWndBase::setTerminalCmd(const QString& cmd)
{
    m_outputTermCmdStr = cmd;
    // revert to default if empty
    if (m_outputTermCmdStr.isEmpty()) {
	m_outputTermCmdStr = defaultTermCmdStr;
    }
}

void DebuggerMainWndBase::slotDebuggerStarting()
{
    if (m_outputTermName.isEmpty()) {
	// create an output window
	if (!createOutputWindow()) {
	    TRACE("createOuputWindow failed");
	    m_outputTermName = QString();
	} else {
	    TRACE("successfully created output window");
	}
    }
    if (m_debugger != 0) {
	m_debugger->setTerminal(m_outputTermName);
    }
}

void DebuggerMainWndBase::setDebuggerCmdStr(const QString& cmd)
{
    m_debuggerCmdStr = cmd;
    // revert to default if empty
    if (m_debuggerCmdStr.isEmpty()) {
	m_debuggerCmdStr = defaultDebuggerCmdStr;
    }
    if (m_debugger != 0) {
	ValArray<QString> cmd;
	splitCmdStr(m_debuggerCmdStr, cmd);
	m_debugger->setDebuggerCmd(cmd);
    }
}


#include "mainwndbase.moc"
