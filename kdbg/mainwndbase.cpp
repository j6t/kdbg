/*
 * Copyright Johannes Sixt
 * This file is licensed under the GNU General Public License Version 2.
 * See the file COPYING in the toplevel directory of the source directory.
 */

#include <kapplication.h>
#include <klocale.h>			/* i18n */
#include <ksimpleconfig.h>
#include <kmessagebox.h>
#include <qtabdialog.h>
#include <qfile.h>
#include "mainwndbase.h"
#include "debugger.h"
#include "gdbdriver.h"
#include "xsldbgdriver.h"
#include "prefdebugger.h"
#include "prefmisc.h"
#ifdef HAVE_CONFIG
#include "config.h"
#endif
#include "mydebug.h"


const char defaultTermCmdStr[] = "xterm -name kdbgio -title %T -e sh -c %C";
const char defaultSourceFilter[] = "*.c *.cc *.cpp *.c++ *.C *.CC";
const char defaultHeaderFilter[] = "*.h *.hh *.hpp *.h++";


DebuggerMainWndBase::DebuggerMainWndBase() :
	m_outputTermCmdStr(defaultTermCmdStr),
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
}

DebuggerMainWndBase::~DebuggerMainWndBase()
{
    delete m_debugger;
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

const char GeneralGroup[] = "General";

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

void DebuggerMainWndBase::setTerminalCmd(const QString& cmd)
{
    m_outputTermCmdStr = cmd;
    // revert to default if empty
    if (m_outputTermCmdStr.isEmpty()) {
	m_outputTermCmdStr = defaultTermCmdStr;
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
