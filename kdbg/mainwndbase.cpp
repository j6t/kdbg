/*
 * Copyright Johannes Sixt
 * This file is licensed under the GNU General Public License Version 2.
 * See the file COPYING in the toplevel directory of the source directory.
 */

#include <kapplication.h>
#include <klocale.h>			/* i18n */
#include <kconfig.h>
#include <kmessagebox.h>
#include <kstatusbar.h>
#include <kfiledialog.h>
#include <qtabdialog.h>
#include <qfile.h>
#include <qdragobject.h>
#include "mainwndbase.h"
#include "debugger.h"
#include "gdbdriver.h"
#include "xsldbgdriver.h"
#include "prefdebugger.h"
#include "prefmisc.h"
#include "ttywnd.h"
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
#include <unistd.h>			/* getpid, unlink etc. */
#endif

#define MAX_RECENT_FILES 4

WatchWindow::WatchWindow(QWidget* parent, const char* name, WFlags f) :
	QWidget(parent, name, f),
	m_watchEdit(this, "watch_edit"),
	m_watchAdd(i18n(" Add "), this, "watch_add"),
	m_watchDelete(i18n(" Del "), this, "watch_delete"),
	m_watchVariables(this, i18n("Expression"), "watch_variables"),
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
    connect(&m_watchVariables, SIGNAL(currentChanged(QListViewItem*)),
	    SLOT(slotWatchHighlighted()));

    m_watchVariables.installEventFilter(this);
    setAcceptDrops(true);
}

WatchWindow::~WatchWindow()
{
}

bool WatchWindow::eventFilter(QObject*, QEvent* ev)
{
    if (ev->type() == QEvent::KeyPress)
    {
	QKeyEvent* kev = static_cast<QKeyEvent*>(ev);
	if (kev->key() == Key_Delete) {
	    emit deleteWatch();
	    return true;
	}
    }
    return false;
}

void WatchWindow::dragEnterEvent(QDragEnterEvent* event)
{
    event->accept(QTextDrag::canDecode(event));
}

void WatchWindow::dropEvent(QDropEvent* event)
{
    QString text;
    if (QTextDrag::decode(event, text))
    {
	// pick only the first line
	text = text.stripWhiteSpace();
	int pos = text.find('\n');
	if (pos > 0)
	    text.truncate(pos);
	text = text.stripWhiteSpace();
	if (!text.isEmpty())
	    emit textDropped(text);
    }
}


// place the text of the hightlighted watch expr in the edit field
void WatchWindow::slotWatchHighlighted()
{
    VarTree* expr = m_watchVariables.selectedItem();
    QString text = expr ? expr->computeExpr() : QString();
    m_watchEdit.setText(text);
}


const char defaultTermCmdStr[] = "xterm -name kdbgio -title %T -e sh -c %C";
const char defaultSourceFilter[] = "*.c *.cc *.cpp *.c++ *.C *.CC";
const char defaultHeaderFilter[] = "*.h *.hh *.hpp *.h++";


DebuggerMainWndBase::DebuggerMainWndBase() :
	m_outputTermCmdStr(defaultTermCmdStr),
	m_outputTermProc(0),
	m_ttyLevel(-1),			/* no tty yet */
#ifdef GDB_TRANSCRIPT
	m_transcriptFile(GDB_TRANSCRIPT),
#endif
	m_popForeground(false),
	m_backTimeout(1000),
	m_tabWidth(0),
	m_sourceFilter(defaultSourceFilter),
	m_headerFilter(defaultHeaderFilter),
	m_debugger(0)
{
    m_statusActive = i18n("active");
}

DebuggerMainWndBase::~DebuggerMainWndBase()
{
    delete m_debugger;

    // if the output window is open, close it
    if (m_outputTermProc != 0) {
	m_outputTermProc->disconnect();	/* ignore signals */
	m_outputTermProc->kill();
	shutdownTermWindow();
    }
}

void DebuggerMainWndBase::setupDebugger(QWidget* parent,
					ExprWnd* localVars,
					ExprWnd* watchVars,
					QListBox* backtrace)
{
    m_debugger = new KDebugger(parent, localVars, watchVars, backtrace);

    QObject::connect(m_debugger, SIGNAL(updateStatusMessage()),
		     parent, SLOT(slotNewStatusMsg()));
    QObject::connect(m_debugger, SIGNAL(updateUI()),
		     parent, SLOT(updateUI()));

    QObject::connect(m_debugger, SIGNAL(breakpointsChanged()),
		     parent, SLOT(updateLineItems()));
    
    QObject::connect(m_debugger, SIGNAL(debuggerStarting()),
		     parent, SLOT(slotDebuggerStarting()));
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

void DebuggerMainWndBase::overrideProgramArguments(const QString& args)
{
    assert(m_debugger != 0);
    m_debugger->overrideProgramArguments(args);
}

void DebuggerMainWndBase::setTranscript(const QString& name)
{
    m_transcriptFile = name;
    if (m_debugger != 0 && m_debugger->driver() != 0)
	m_debugger->driver()->setLogFileName(m_transcriptFile);
}

const char OutputWindowGroup[] = "OutputWindow";
const char TermCmdStr[] = "TermCmdStr";
const char KeepScript[] = "KeepScript";
const char DebuggerGroup[] = "Debugger";
const char DebuggerCmdStr[] = "DebuggerCmdStr";
const char PreferencesGroup[] = "Preferences";
const char PopForeground[] = "PopForeground";
const char BackTimeout[] = "BackTimeout";
const char TabWidth[] = "TabWidth";
const char FilesGroup[] = "Files";
const char SourceFileFilter[] = "SourceFileFilter";
const char HeaderFileFilter[] = "HeaderFileFilter";
const char GeneralGroup[] = "General";

void DebuggerMainWndBase::saveSettings(KConfig* config)
{
    if (m_debugger != 0) {
	m_debugger->saveSettings(config);
    }

    KConfigGroupSaver g(config, OutputWindowGroup);
    config->writeEntry(TermCmdStr, m_outputTermCmdStr);

    config->setGroup(DebuggerGroup);
    config->writeEntry(DebuggerCmdStr, m_debuggerCmdStr);

    config->setGroup(PreferencesGroup);
    config->writeEntry(PopForeground, m_popForeground);
    config->writeEntry(BackTimeout, m_backTimeout);
    config->writeEntry(TabWidth, m_tabWidth);
    config->writeEntry(SourceFileFilter, m_sourceFilter);
    config->writeEntry(HeaderFileFilter, m_headerFilter);
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

    config->setGroup(PreferencesGroup);
    m_popForeground = config->readBoolEntry(PopForeground, false);
    m_backTimeout = config->readNumEntry(BackTimeout, 1000);
    m_tabWidth = config->readNumEntry(TabWidth, 0);
    m_sourceFilter = config->readEntry(SourceFileFilter, m_sourceFilter);
    m_headerFilter = config->readEntry(HeaderFileFilter, m_headerFilter);
}

void DebuggerMainWndBase::setAttachPid(const QString& pid)
{
    assert(m_debugger != 0);
    m_debugger->setAttachPid(pid);
}

bool DebuggerMainWndBase::debugProgram(const QString& executable,
				       QString lang, QWidget* parent)
{
    assert(m_debugger != 0);

    TRACE(QString("trying language '%1'...").arg(lang));
    DebuggerDriver* driver = driverFromLang(lang);

    if (driver == 0)
    {
	// see if there is a language in the per-program config file
	QString configName = m_debugger->getConfigForExe(executable);
	if (QFile::exists(configName))
	{
	    KSimpleConfig c(configName, true);	// read-only
	    c.setGroup(GeneralGroup);

	    // Using "GDB" as default here is for backwards compatibility:
	    // The config file exists but doesn't have an entry,
	    // so it must have been created by an old version of KDbg
	    // that had only the GDB driver.
	    lang = c.readEntry(KDebugger::DriverNameEntry, "GDB");

	    TRACE(QString("...bad, trying config driver %1...").arg(lang));
	    driver = driverFromLang(lang);
	}

    }
    if (driver == 0)
    {
	QString name = driverNameFromFile(executable);

	TRACE(QString("...no luck, trying %1 derived"
		" from file contents").arg(name));
	driver = driverFromLang(name);
    }
    if (driver == 0)
    {
	// oops
	QString msg = i18n("Don't know how to debug language `%1'");
	KMessageBox::sorry(parent, msg.arg(lang));
	return false;
    }

    driver->setLogFileName(m_transcriptFile);

    bool success = m_debugger->debugProgram(executable, driver);

    if (!success)
    {
	delete driver;

	QString msg = i18n("Could not start the debugger process.\n"
			   "Please shut down KDbg and resolve the problem.");
	KMessageBox::sorry(parent, msg);
    }

    return success;
}

// derive driver from language
DebuggerDriver* DebuggerMainWndBase::driverFromLang(QString lang)
{
    // lang is needed in all lowercase
    lang = lang.lower();

    // The following table relates languages and debugger drivers
    static const struct L {
	const char* shortest;	// abbreviated to this is still unique
	const char* full;	// full name of language
	int driver;
    } langs[] = {
	{ "c",       "c++",     1 },
	{ "f",       "fortran", 1 },
	{ "p",       "python",  3 },
	{ "x",       "xslt",    2 },
	// the following are actually driver names
	{ "gdb",     "gdb",     1 },
	{ "xsldbg",  "xsldbg",  2 },
    };
    const int N = sizeof(langs)/sizeof(langs[0]);

    // lookup the language name
    int driverID = 0;
    for (int i = 0; i < N; i++)
    {
	const L& l = langs[i];

	// shortest must match
	if (!lang.startsWith(l.shortest))
	    continue;

	// lang must not be longer than the full name, and it must match
	if (QString(l.full).startsWith(lang))
	{
	    driverID = l.driver;
	    break;
	}
    }
    DebuggerDriver* driver = 0;
    switch (driverID) {
    case 1:
	{
	    GdbDriver* gdb = new GdbDriver;
	    gdb->setDefaultInvocation(m_debuggerCmdStr);
	    driver = gdb;
	}
	break;
    case 2:
	driver = new XsldbgDriver;
	break;
    default:
	// unknown language
	break;
    }
    return driver;
}

/**
 * Try to guess the language to use from the contents of the file.
 */
QString DebuggerMainWndBase::driverNameFromFile(const QString& exe)
{
    /* Inprecise but simple test to see if file is in XSLT language */
    if (exe.right(4).lower() == ".xsl")
	return "XSLT";

    return "GDB";
}

// helper that gets a file name (it only differs in the caption of the dialog)
QString DebuggerMainWndBase::myGetFileName(QString caption,
			   QString dir, QString filter,
			   QWidget* parent)
{
    QString filename;
    KFileDialog dlg(dir, filter, parent, "filedialog", true);

    dlg.setCaption(caption);

    if (dlg.exec() == QDialog::Accepted)
	filename = dlg.selectedFile();

    return filename;
}

void DebuggerMainWndBase::newStatusMsg(KStatusBar* statusbar)
{
    QString msg = m_debugger->statusMessage();
    statusbar->changeItem(msg, ID_STATUS_MSG);
}

void DebuggerMainWndBase::doGlobalOptions(QWidget* parent)
{
    QTabDialog dlg(parent, "global_options", true);
    QString title = kapp->caption();
    title += i18n(": Global options");
    dlg.setCaption(title);
    dlg.setCancelButton(i18n("Cancel"));
    dlg.setOKButton(i18n("OK"));

    PrefDebugger prefDebugger(&dlg);
    prefDebugger.setDebuggerCmd(m_debuggerCmdStr.isEmpty()  ?
				GdbDriver::defaultGdb()  :  m_debuggerCmdStr);
    prefDebugger.setTerminal(m_outputTermCmdStr);

    PrefMisc prefMisc(&dlg);
    prefMisc.setPopIntoForeground(m_popForeground);
    prefMisc.setBackTimeout(m_backTimeout);
    prefMisc.setTabWidth(m_tabWidth);
    prefMisc.setSourceFilter(m_sourceFilter);
    prefMisc.setHeaderFilter(m_headerFilter);

    dlg.addTab(&prefDebugger, i18n("&Debugger"));
    dlg.addTab(&prefMisc, i18n("&Miscellaneous"));
    if (dlg.exec() == QDialog::Accepted)
    {
	setDebuggerCmdStr(prefDebugger.debuggerCmd());
	setTerminalCmd(prefDebugger.terminal());
	m_popForeground = prefMisc.popIntoForeground();
	m_backTimeout = prefMisc.backTimeout();
	m_tabWidth = prefMisc.tabWidth();
	m_sourceFilter = prefMisc.sourceFilter();
	if (m_sourceFilter.isEmpty())
	    m_sourceFilter = defaultSourceFilter;
	m_headerFilter = prefMisc.headerFilter();
	if (m_headerFilter.isEmpty())
	    m_headerFilter = defaultHeaderFilter;
    }
}

const char fifoNameBase[] = "/tmp/kdbgttywin%05d";

/*
 * We use the scope operator :: in this function, so that we don't
 * accidentally use the wrong close() function (I've been bitten ;-),
 * outch!) (We use it for all the libc functions, to be consistent...)
 */
QString DebuggerMainWndBase::createOutputWindow()
{
    // create a name for a fifo
    QString fifoName;
    fifoName.sprintf(fifoNameBase, ::getpid());

    // create a fifo that will pass in the tty name
    QFile::remove(fifoName);		// remove remnants
#ifdef HAVE_MKFIFO
    if (::mkfifo(fifoName.local8Bit(), S_IRUSR|S_IWUSR) < 0) {
	// failed
	TRACE("mkfifo " + fifoName + " failed");
	return QString();
    }
#else
    if (::mknod(fifoName.local8Bit(), S_IFIFO | S_IRUSR|S_IWUSR, 0) < 0) {
	// failed
	TRACE("mknod " + fifoName + " failed");
	return QString();
    }
#endif

    m_outputTermProc = new KProcess;

    {
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
	QString shellScript;
	if (!m_outputTermKeepScript.isEmpty()) {
	    shellScript = m_outputTermKeepScript;
	} else {
	    shellScript = shellScriptFmt;
	}

	shellScript.replace("%s", fifoName);
	TRACE("output window script is " + shellScript);

	QString title = kapp->caption();
	title += i18n(": Program output");

	// parse the command line specified in the preferences
	QStringList cmdParts = QStringList::split(' ', m_outputTermCmdStr);

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

	for (QStringList::iterator i = cmdParts.begin(); i != cmdParts.end(); ++i)
	{
	    QString& str = *i;
	    for (int j = sizeof(substitute)/sizeof(substitute[0])-1; j >= 0; j--) {
		int pos = str.find(substitute[j].seq);
		if (pos >= 0) {
		    str.replace(pos, 2, substitute[j].replace);
		    break;		/* substitute only one sequence */
		}
	    }
	    *m_outputTermProc << str;
	}

    }

    if (m_outputTermProc->start())
    {
	QString tty;

	// read the ttyname from the fifo
	QFile f(fifoName);
	if (f.open(IO_ReadOnly))
	{
	    QByteArray t = f.readAll();
	    tty = QString::fromLocal8Bit(t, t.size());
	    f.close();
	}
	f.remove();

	// remove whitespace
	tty = tty.stripWhiteSpace();
	TRACE("tty=" + tty);
	return tty;
    }
    else
    {
	// error, could not start xterm
	TRACE("fork failed for fifo " + fifoName);
	QFile::remove(fifoName);
	shutdownTermWindow();
	return QString();
    }
}

void DebuggerMainWndBase::shutdownTermWindow()
{
    delete m_outputTermProc;
    m_outputTermProc = 0;
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
    if (m_debugger == 0)		/* paranoia check */
	return;

    if (m_ttyLevel == m_debugger->ttyLevel())
    {
    }
    else
    {
	// shut down terminal emulations we will not need
	switch (m_ttyLevel) {
	case KDebugger::ttySimpleOutputOnly:
	    ttyWindow()->deactivate();
	    break;
	case KDebugger::ttyFull:
	    if (m_outputTermProc != 0) {
		m_outputTermProc->kill();
		// will be deleted in slot
	    }
	    break;
	default: break;
	}

	m_ttyLevel = m_debugger->ttyLevel();

	QString ttyName;
	switch (m_ttyLevel) {
	case KDebugger::ttySimpleOutputOnly:
	    ttyName = ttyWindow()->activate();
	    break;
	case KDebugger::ttyFull:
	    if (m_outputTermProc == 0) {
		// create an output window
		ttyName = createOutputWindow();
		TRACE(ttyName.isEmpty() ?
		      "createOuputWindow failed" : "successfully created output window");
	    }
	    break;
	default: break;
	}

	m_debugger->setTerminal(ttyName);
    }
}

void DebuggerMainWndBase::setDebuggerCmdStr(const QString& cmd)
{
    m_debuggerCmdStr = cmd;
    // make empty if it is the default
    if (m_debuggerCmdStr == GdbDriver::defaultGdb()) {
	m_debuggerCmdStr = QString();
    }
}


#include "mainwndbase.moc"
