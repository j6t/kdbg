// $Id$

// Copyright by Johannes Sixt
// This file is under GPL, the GNU General Public Licence

#include "debugger.h"
#include "debugger.moc"
#include "commandids.h"
#include "updateui.h"
#include "parsevar.h"
#include "pgmargs.h"
#include "typetable.h"
#include <qregexp.h>
#include <qfileinf.h>
#include <kapp.h>
#include <kfiledialog.h>
#include <kiconloader.h>
#include <kmsgbox.h>
#include <kstdaccel.h>
#include <ksimpleconfig.h>
#include <ctype.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#else
# ifndef VERSION
# define VERSION ""
# endif
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


KStdAccel* keys = 0;

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


KDebugger::KDebugger(const char* name) :
	KTopLevelWidget(name),
	m_outputTermPID(0),
	m_state(DSidle),
	m_gdbOutput(0),
	m_gdbOutputLen(0),
	m_gdbOutputAlloc(0),
	m_activeCmd(0),
	m_delayedPrintThis(false),
	m_haveExecutable(false),
	m_programActive(false),
	m_programRunning(false),
	m_programConfig(0),
	m_gdb(),
	m_gdbMajor(4), m_gdbMinor(16),
#ifdef GDB_TRANSCRIPT
	m_logFile(GDB_TRANSCRIPT),
#endif
	m_bpTable(*this),
	m_menu(this, "menu"),
	m_toolbar(this, "toolbar"),
	m_statusbar(this, "statusbar"),
	m_mainPanner(this, "main_pane", KNewPanner::Vertical, KNewPanner::Percent, 60),
	m_leftPanner(&m_mainPanner, "left_pane", KNewPanner::Horizontal, KNewPanner::Percent, 70),
	m_rightPanner(&m_mainPanner, "right_pane", KNewPanner::Horizontal, KNewPanner::Percent, 50),
	m_filesWindow(&m_leftPanner, "files", m_bpTable),
	m_btWindow(&m_leftPanner, "backtrace"),
#ifdef WANT_THIS_PANE
	m_frameVariables(&m_rightPanner, "frame_variables"),
	m_localVariables(&m_frameVariables, "locals"),
	m_this(&m_frameVariables, "this"),
#else
	m_localVariables(&m_rightPanner, "locals"),
#endif
	m_watches(&m_rightPanner, "watches"),
	m_watchEdit(&m_watches, "watch_edit"),
	m_watchAdd(i18n(" Add "), &m_watches, "watch_add"),
	m_watchDelete(i18n(" Del "), &m_watches, "watch_delete"),
	m_watchVariables(&m_watches, "watch_variables"),
	m_watchV(&m_watches, 0),
	m_watchH(0)
{
    m_statusBusy = i18n("busy");
    m_statusActive = i18n("active");

    m_gdbOutputAlloc = 4000;
    m_gdbOutput = new char[m_gdbOutputAlloc];

    initMenu();
    setMenu(&m_menu);
    
    initToolbar();
    addToolBar(&m_toolbar);
    
    setStatusBar(&m_statusbar);

    m_mainPanner.activate(&m_leftPanner, &m_rightPanner);

    setView(&m_mainPanner, true);	/* show frame */
    
#ifdef WANT_THIS_PANE
//    m_frameVariables.addTab(new QWidget(&m_frameVariables), i18n("Auto"));
    m_frameVariables.addTab(&m_localVariables, i18n("Locals"));
    m_frameVariables.addTab(&m_this, "this");
    connect(&m_this, SIGNAL(expanding(KTreeViewItem*,bool&)),
	    SLOT(slotThisExpanding(KTreeViewItem*,bool&)));
#endif
    connect(&m_localVariables, SIGNAL(expanding(KTreeViewItem*,bool&)),
	    SLOT(slotLocalsExpanding(KTreeViewItem*,bool&)));

    m_leftPanner.activate(&m_filesWindow, &m_btWindow);
    connect(&m_btWindow, SIGNAL(selected(int)), SLOT(gotoFrame(int)));
    connect(&m_watchVariables, SIGNAL(expanding(KTreeViewItem*,bool&)),
	    SLOT(slotWatchExpanding(KTreeViewItem*,bool&)));

#ifdef WANT_THIS_PANE
    // must set minimum size to make panner work
    m_frameVariables.setMinimumSize(10,10);
    m_rightPanner.activate(&m_frameVariables, &m_watches);
#else
    m_rightPanner.activate(&m_localVariables, &m_watches);
#endif
    connect(&m_watchEdit, SIGNAL(returnPressed()), SLOT(slotAddWatch()));
    connect(&m_watchAdd, SIGNAL(clicked()), SLOT(slotAddWatch()));
    connect(&m_watchDelete, SIGNAL(clicked()), SLOT(slotDeleteWatch()));
    connect(&m_watchVariables, SIGNAL(highlighted(int)), SLOT(slotWatchHighlighted(int)));
    
    // setup the watch window layout
    m_watchAdd.setMinimumSize(m_watchAdd.sizeHint());
    m_watchDelete.setMinimumSize(m_watchDelete.sizeHint());
    m_watchV.addLayout(&m_watchH, 0);
    m_watchV.addWidget(&m_watchVariables, 10);
    m_watchH.addWidget(&m_watchEdit, 10);
    m_watchH.addWidget(&m_watchAdd, 0);
    m_watchH.addWidget(&m_watchDelete, 0);

    m_filesWindow.setWindowMenu(&m_menuWindow);
    connect(&m_filesWindow.m_findDlg, SIGNAL(closed()), SLOT(updateUI()));

    m_bpTable.setCaption(i18n("Breakpoints"));
    connect(&m_bpTable, SIGNAL(closed()), SLOT(updateUI()));

    // route unhandled menu items to winstack
    connect(this, SIGNAL(forwardMenuCallback(int)), &m_filesWindow, SLOT(menuCallback(int)));
    // file/line updates
    connect(&m_filesWindow, SIGNAL(fileChanged()), SLOT(slotFileChanged()));
    connect(&m_filesWindow, SIGNAL(lineChanged()), SLOT(slotLineChanged()));
    
    // debugger process
    connect(&m_gdb, SIGNAL(receivedStdout(KProcess*,char*,int)),
	    SLOT(receiveOutput(KProcess*,char*,int)));
    connect(&m_gdb, SIGNAL(wroteStdin(KProcess*)), SLOT(commandRead(KProcess*)));
    connect(&m_gdb, SIGNAL(processExited(KProcess*)), SLOT(gdbExited(KProcess*)));

    restoreSettings(kapp->getConfig());
    resize(700, 700);

    emit updateUI();
    slotFileChanged();
}

KDebugger::~KDebugger()
{
    TRACE(__PRETTY_FUNCTION__);

    saveSettings(kapp->getConfig());

    // if the output window is open, close it
    if (m_outputTermPID != 0) {
	kill(m_outputTermPID, SIGTERM);
    }

    if (m_programConfig != 0) {
	saveProgramSettings();
	m_programConfig->sync();
	delete m_programConfig;
    }
    delete[] m_gdbOutput;
}


// instance properties
void KDebugger::saveProperties(KConfig* config)
{
    // session management
    config->writeEntry("executable", m_executable);
}

void KDebugger::readProperties(KConfig* config)
{
    // session management
    QString execName = config->readEntry("executable");

    TRACE("readProperties: executable=" + execName);
    if (!execName.isEmpty()) {
	debugProgram(execName);
    }
}

const char WindowGroup[] = "Windows";
const char MainPane[] = "MainPane";
const char LeftPane[] = "LeftPane";
const char RightPane[] = "RightPane";
const char BreaklistVisible[] = "BreaklistVisible";
const char Breaklist[] = "Breaklist";

void KDebugger::saveSettings(KConfig* config)
{
    KConfigGroupSaver g(config, WindowGroup);
    // panner positions
    int vsep = m_mainPanner.separatorPos();
    int lsep = m_leftPanner.separatorPos();
    int rsep = m_rightPanner.separatorPos();
    config->writeEntry(MainPane, vsep);
    config->writeEntry(LeftPane, lsep);
    config->writeEntry(RightPane, rsep);
    // breakpoint window
    bool visible = m_bpTable.isVisible();
    const QRect& r = m_bpTable.geometry();
    config->writeEntry(BreaklistVisible, visible);
    config->writeEntry(Breaklist, r);
}

void KDebugger::restoreSettings(KConfig* config)
{
    KConfigGroupSaver g(config, WindowGroup);
    // panner positions
    int vsep = config->readNumEntry(MainPane, 60);
    int lsep = config->readNumEntry(LeftPane, 70);
    int rsep = config->readNumEntry(RightPane, 50);
    m_mainPanner.setSeparatorPos(vsep);
    m_leftPanner.setSeparatorPos(lsep);
    m_rightPanner.setSeparatorPos(rsep);
    // breakpoint window
    bool visible = config->readBoolEntry(BreaklistVisible, false);
    if (config->hasKey(Breaklist)) {
	QRect r = config->readRectEntry(Breaklist);
	m_bpTable.setGeometry(r);
    }
    visible ? m_bpTable.show() : m_bpTable.hide();
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

// slots
void KDebugger::menuCallback(int item)
{
    switch (item) {
    case ID_FILE_EXECUTABLE:
	if (m_state == DSidle)
	{
	    // open a new executable
	    QString executable = getFileName(i18n("Select the executable to debug"),
					     0, 0, this);
	    if (!executable.isEmpty()) {
		if (m_haveExecutable)
		{
		    stopGdb();
		    /*
		     * We MUST wait until the slot gdbExited() has been
		     * called. But to avoid a deadlock, we wait only for
		     * some certain maximum time. Should this timeout be
		     * reached, the only reasonable thing one could do then
		     * is exiting kdbg.
		     */
		    int maxTime = 20;	/* about 20 seconds */
		    while (m_haveExecutable && maxTime > 0) {
			kapp->processEvents(1000);
			// give gdb time to die (and send a SIGCLD)
			::sleep(1);
			--maxTime;
		    }
		    if (m_haveExecutable) {
			/* timed out! We can't really do anything useful now */
			TRACE("timed out while waiting for gdb to die!");
			break;
		    }
		}
		debugProgram(executable);
	    }
	}
	break;
    case ID_FILE_COREFILE:
	if (isReady() && !m_programRunning) {
	    // use a core dump
	    QString corefile = getFileName(i18n("Select core dump"),
					   0, 0, this);
	    if (!corefile.isEmpty()) {
		m_corefile = corefile;
		CmdQueueItem* cmd = loadCoreFile();
		cmd->m_byUser = true;
	    }
	}
	break;
    case ID_FILE_QUIT:
	kapp->quit();
	break;
    case ID_VIEW_TOOLBAR:
	enableToolBar();
	break;
    case ID_VIEW_STATUSBAR:
	enableStatusBar();
	break;
    case ID_PROGRAM_RUN:
	if (isReady()) {
	    if (m_programActive) {
		// gdb command: continue
		executeCmd(DCcont, "cont", true);
	    } else {
		// gdb command: run
		executeCmd(DCrun, "run " + m_programArgs, true);
		m_programActive = true;
	    }
	    m_programRunning = true;
	}
	break;
    case ID_PROGRAM_RUN_AGAIN:
	if (isReady() && m_programActive && !m_programRunning) {
	    executeCmd(DCrun, "run " + m_programArgs, true);
	    m_programRunning = true;
	}
	break;
    case ID_PROGRAM_STEP:
	if (isReady() && m_programActive && !m_programRunning) {
	    executeCmd(DCstep, "step", true);
	    m_programRunning = true;
	}
	break;
    case ID_PROGRAM_NEXT:
	if (isReady() && m_programActive && !m_programRunning) {
	    executeCmd(DCnext, "next", true);
	    m_programRunning = true;
	}
	break;
    case ID_PROGRAM_FINISH:
	if (isReady() && m_programActive && !m_programRunning) {
	    executeCmd(DCfinish, "finish", true);
	    m_programRunning = true;
	}
	break;
    case ID_PROGRAM_UNTIL:
	if (isReady() && m_programActive && !m_programRunning) {
	    QString file;
	    int lineNo;
	    if (m_filesWindow.activeLine(file, lineNo)) {
		// strip off directory part of file name
		file.detach();
		int offset = file.findRev("/");
		if (offset >= 0) {
		    file.remove(0, offset+1);
		}
		QString cmdString(file.length() + 30);
		cmdString.sprintf("until %s:%d", file.data(), lineNo+1);
		executeCmd(DCuntil, cmdString, true);
		m_programRunning = true;
	    }
	}
	break;
    case ID_PROGRAM_BREAK:
	if (m_haveExecutable && m_programRunning) {
	    m_gdb.kill(SIGINT);
	}
	break;
    case ID_PROGRAM_ARGS:
	if (m_haveExecutable) {
	    PgmArgs dlg(this, m_executable);
	    dlg.setText(m_programArgs);
	    if (dlg.exec()) {
		m_programArgs = dlg.text();
		TRACE("new pgm args: " + m_programArgs + "\n");
	    }
	}
	break;
    case ID_BRKPT_SET:
    case ID_BRKPT_TEMP:
	if (isReady()) {
	    QString file;
	    int lineNo;
	    if (m_filesWindow.activeLine(file, lineNo)) {
		m_bpTable.doBreakpoint(file, lineNo, item == ID_BRKPT_TEMP);
	    }
	}
	break;
    case ID_BRKPT_ENABLE:
	if (isReady()) {
	    QString file;
	    int lineNo;
	    if (m_filesWindow.activeLine(file, lineNo)) {
		m_bpTable.doEnableDisableBreakpoint(file, lineNo);
	    }
	}
	break;
    case ID_BRKPT_LIST:
	if (m_bpTable.isVisible()) {
	    m_bpTable.hide();
	} else {
	    m_bpTable.show();
	}
	break;
    default:
	// forward all others
	emit forwardMenuCallback(item);
    }
    emit updateUI();
}

void KDebugger::updateUI()
{
    // enumerate all menus
    {
	UpdateMenuUI updateMenu(&m_menuFile, this, SLOT(updateUIItem(UpdateUI*)));
	updateMenu.iterateMenu();
    }
    {
	UpdateMenuUI updateMenu(&m_menuView, this, SLOT(updateUIItem(UpdateUI*)));
	updateMenu.iterateMenu();
    }
    {
	UpdateMenuUI updateMenu(&m_menuProgram, this, SLOT(updateUIItem(UpdateUI*)));
	updateMenu.iterateMenu();
    }
    {
	UpdateMenuUI updateMenu(&m_menuBrkpt, this, SLOT(updateUIItem(UpdateUI*)));
	updateMenu.iterateMenu();
    }

    // toolbar
    static const int toolIds[] = {
	ID_PROGRAM_RUN, ID_PROGRAM_STEP, ID_PROGRAM_NEXT, ID_PROGRAM_FINISH,
	ID_BRKPT_SET
    };
    UpdateToolbarUI updateToolbar(&m_toolbar, this, SLOT(updateUIItem(UpdateUI*)),
				  toolIds, sizeof(toolIds)/sizeof(toolIds[0]));
    updateToolbar.iterateToolbar();
}

void KDebugger::updateUIItem(UpdateUI* item)
{
    switch (item->id) {
    case ID_VIEW_FINDDLG:
	item->setCheck(m_filesWindow.m_findDlg.isVisible());
	break;
    case ID_FILE_EXECUTABLE:
	item->enable(m_state == DSidle);
	break;
    case ID_FILE_COREFILE:
	item->enable(isReady() && !m_programRunning);
	break;
    case ID_PROGRAM_STEP:
    case ID_PROGRAM_NEXT:
    case ID_PROGRAM_FINISH:
    case ID_PROGRAM_UNTIL:
    case ID_PROGRAM_RUN_AGAIN:
	item->enable(isReady() && m_programActive && !m_programRunning);
	break;
    case ID_PROGRAM_RUN:
	item->enable(isReady());
	break;
    case ID_PROGRAM_BREAK:
	item->enable(m_haveExecutable && m_programRunning);
	break;
    case ID_PROGRAM_ARGS:
	item->enable(m_haveExecutable);
    case ID_BRKPT_SET:
    case ID_BRKPT_TEMP:
	item->enable(isReady());
	break;
    case ID_BRKPT_ENABLE:
	item->enable(isReady());
	break;
    case ID_BRKPT_LIST:
	item->setCheck(m_bpTable.isVisible());
	break;
    }
    
    // update statusbar
    m_statusbar.changeItem(m_state == DSidle ? "" : static_cast<const char*>(m_statusBusy),
			   ID_STATUS_BUSY);
    m_statusbar.changeItem(m_programActive ? static_cast<const char*>(m_statusActive) : "",
			   ID_STATUS_ACTIVE);
    // line number is updated in slotLineChanged
}


// implementation helpers
void KDebugger::initMenu()
{
    m_menuFile.insertItem(i18n("&Open Source..."), ID_FILE_OPEN);
    m_menuFile.insertItem(i18n("&Reload Source"), ID_FILE_RELOAD);
    m_menuFile.insertSeparator();
    m_menuFile.insertItem(i18n("&Executable..."), ID_FILE_EXECUTABLE);
    m_menuFile.insertItem(i18n("&Core dump..."), ID_FILE_COREFILE);
    m_menuFile.insertSeparator();
    m_menuFile.insertItem(i18n("&Quit"), ID_FILE_QUIT);
    m_menuFile.setAccel(keys->open(), ID_FILE_OPEN);
    m_menuFile.setAccel(keys->quit(), ID_FILE_QUIT);

    m_menuView.insertItem(i18n("&Find..."), ID_VIEW_FINDDLG);
    m_menuView.insertItem(i18n("Toggle &Toolbar"), ID_VIEW_TOOLBAR);
    m_menuView.insertItem(i18n("Toggle &Statusbar"), ID_VIEW_STATUSBAR);
    m_menuView.setAccel(keys->find(), ID_VIEW_FINDDLG);

    m_menuProgram.insertItem(i18n("&Run"), ID_PROGRAM_RUN);
    m_menuProgram.insertItem(i18n("Step &into"), ID_PROGRAM_STEP);
    m_menuProgram.insertItem(i18n("Step &over"), ID_PROGRAM_NEXT);
    m_menuProgram.insertItem(i18n("Step o&ut"), ID_PROGRAM_FINISH);
    m_menuProgram.insertItem(i18n("Run to &cursor"), ID_PROGRAM_UNTIL);
    m_menuProgram.insertSeparator();
    m_menuProgram.insertItem(i18n("&Break"), ID_PROGRAM_BREAK);
    m_menuProgram.insertItem(i18n("Re&start"), ID_PROGRAM_RUN_AGAIN);
    m_menuProgram.insertSeparator();
    m_menuProgram.insertItem(i18n("&Arguments..."), ID_PROGRAM_ARGS);
    m_menuProgram.setAccel(Key_F5, ID_PROGRAM_RUN);
    m_menuProgram.setAccel(Key_F8, ID_PROGRAM_STEP);
    m_menuProgram.setAccel(Key_F10, ID_PROGRAM_NEXT);
    m_menuProgram.setAccel(Key_F6, ID_PROGRAM_FINISH);
    m_menuProgram.setAccel(Key_F7, ID_PROGRAM_UNTIL);

    m_menuBrkpt.setCheckable(true);
    m_menuBrkpt.insertItem(i18n("Set/Clear &breakpoint"), ID_BRKPT_SET);
    m_menuBrkpt.insertItem(i18n("Set &temporary breakpoint"), ID_BRKPT_TEMP);
    m_menuBrkpt.insertItem(i18n("&Enable/Disable breakpoint"), ID_BRKPT_ENABLE);
    m_menuBrkpt.insertItem(i18n("&List..."), ID_BRKPT_LIST);
    m_menuBrkpt.setAccel(Key_F9, ID_BRKPT_SET);
    m_menuBrkpt.setAccel(SHIFT+Key_F9, ID_BRKPT_TEMP);
    m_menuBrkpt.setAccel(CTRL+Key_F9, ID_BRKPT_ENABLE);

    m_menuWindow.insertItem(i18n("&More..."), ID_WINDOW_MORE);
  
    m_menuHelp.insertItem(i18n("&Contents"), ID_HELP_HELP);
    m_menuHelp.insertItem(i18n("&About"), ID_HELP_ABOUT);
  
    connect(&m_menuFile, SIGNAL(activated(int)), SLOT(menuCallback(int)));
    connect(&m_menuView, SIGNAL(activated(int)), SLOT(menuCallback(int)));
    connect(&m_menuProgram, SIGNAL(activated(int)), SLOT(menuCallback(int)));
    connect(&m_menuBrkpt, SIGNAL(activated(int)), SLOT(menuCallback(int)));
    connect(&m_menuWindow, SIGNAL(activated(int)), SLOT(menuCallback(int)));
    connect(&m_menuHelp, SIGNAL(activated(int)), SLOT(menuCallback(int)));

    m_menu.insertItem(i18n("&File"), &m_menuFile);
    m_menu.insertItem(i18n("&View"), &m_menuView);
    m_menu.insertItem(i18n("E&xecution"), &m_menuProgram);
    m_menu.insertItem(i18n("&Breakpoint"), &m_menuBrkpt);
    m_menu.insertItem(i18n("&Window"), &m_menuWindow);
    m_menu.insertSeparator();
    
    QString about = "KDbg " VERSION " - ";
    about += i18n("A Debugger\n"
		  "by Johannes Sixt <Johannes.Sixt@telecom.at>\n"
		  "with the help of many others");
    m_menu.insertItem(i18n("&Help"),
		      kapp->getHelpMenu(false, about));
}

void KDebugger::initToolbar()
{
    KIconLoader* loader = kapp->getIconLoader();

    m_toolbar.insertButton(loader->loadIcon("fileopen.xpm"),ID_FILE_OPEN, true,
			   i18n("Open a source file"));
    m_toolbar.insertButton(loader->loadIcon("reload.xpm"),ID_FILE_RELOAD, true,
			   i18n("Reload source file"));
    m_toolbar.insertSeparator();
    m_toolbar.insertButton(loader->loadIcon("pgmrun.xpm"),ID_PROGRAM_RUN, true,
			   i18n("Run/Continue"));
    m_toolbar.insertButton(loader->loadIcon("pgmstep.xpm"),ID_PROGRAM_STEP, true,
			   i18n("Step into"));
    m_toolbar.insertButton(loader->loadIcon("pgmnext.xpm"),ID_PROGRAM_NEXT, true,
			   i18n("Step over"));
    m_toolbar.insertButton(loader->loadIcon("pgmfinish.xpm"),ID_PROGRAM_FINISH, true,
			   i18n("Step out"));
    m_toolbar.insertSeparator();
    m_toolbar.insertButton(loader->loadIcon("brkpt.xpm"),ID_BRKPT_SET, true,
			   i18n("Breakpoint"));
    m_toolbar.insertSeparator();
    m_toolbar.insertButton(loader->loadIcon("search.xpm"),ID_VIEW_FINDDLG, true,
			   i18n("Search"));

    connect(&m_toolbar, SIGNAL(clicked(int)), SLOT(menuCallback(int)));

    m_statusbar.insertItem(m_statusBusy, ID_STATUS_BUSY);
    m_statusbar.insertItem(m_statusActive, ID_STATUS_ACTIVE);
    m_statusbar.insertItem(i18n("Line 00000"), ID_STATUS_LINENO);
    m_statusbar.insertItem("", ID_STATUS_MSG);	/* message pane */

    // reserve some translations
    i18n("Restart");
    i18n("Executable");
    i18n("Core dump");
}

#ifdef WANT_THIS_PANE
bool KDebugger::isThisPaneVisible()
{
    return m_frameVariables.isTabEnabled("this");
}
#endif


//////////////////////////////////////////////////////////
// debugger driver

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

    // debugger executable
    m_gdb.clearArguments();
    m_gdb << "gdb";
    // add options:
    m_gdb
	<< "-fullname"			/* to get standard file names each time the prog stops */
	<< "-nx";			/* do not execute initialization files */

    if (!m_gdb.start(KProcess::NotifyOnExit,
		     KProcess::Communication(KProcess::Stdin|KProcess::Stdout))) {
	return false;
    }

#ifdef GDB_TRANSCRIPT
    // open log file
    m_logFile.open(IO_WriteOnly);
#endif

    // change prompt string and synchronize with gdb
    m_state = DSidle;
    executeCmd(DCinitialize, "set prompt " PROMPT);
    executeCmd(DCnoconfirm, "set confirm off");
    executeCmd(DCtty, "tty " + m_outputTermName);

    return true;
}

void KDebugger::stopGdb()
{
    m_gdb.kill(SIGTERM);
    m_state = DSidle;
}

void KDebugger::gdbExited(KProcess*)
{
#ifdef GDB_TRANSCRIPT
    static const char txt[] = "\ngdb exited\n";
    m_logFile.writeBlock(txt,sizeof(txt)-1);
#endif

    // save settings
    if (m_programConfig != 0) {
	saveProgramSettings();
	m_programConfig->sync();
	delete m_programConfig;
	m_programConfig = 0;
    }

    // reset state
    m_state = DSidle;
    m_haveExecutable = false;
    m_executable = "";
    m_programActive = false;
    m_programRunning = false;
    // empty buffer
    m_gdbOutputLen = 0;
    *m_gdbOutput = '\0';
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
	 * Set up a special environment variable that is recognized by kdbg
	 * when it is started by the output window, so that it will reserve
	 * a tty, but do nothing otherwise (see main.cpp).
	 */
#ifdef HAVE_PUTENV
	QString specialVar = "_KDBG_TTYIOWIN=" + fifoName;
	::putenv(specialVar);
#else
	::setenv("_KDBG_TTYIOWIN", fifoName, 1);
#endif

	// spawn "xterm -name kdbgio -title "KDbg: Program output" -e kdbg"
	extern QString thisExecName;
	QString title = kapp->getCaption();
	title += i18n(": Program output");
	::execlp("xterm",
		 "xterm",
		 "-name", "kdbgio",
		 "-title", title.data(),
		 "-e", thisExecName.data(),
		 0);
	
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
	QString tty(ttyname, n+1);
	
	// parse the string: /dev/ttyp3,1234 (pid of xterm)
	int comma = tty.find(',');
	ASSERT(comma > 0);
	m_outputTermName = tty.left(comma);
	tty.remove(0, comma+1);
	m_outputTermPID = tty.toInt();
	TRACE(QString().sprintf("tty=%s, pid=%d",
				m_outputTermName.data(), m_outputTermPID));
    }
    return true;
}

bool KDebugger::debugProgram(const QString& name)
{
    TRACE(__PRETTY_FUNCTION__);
    // TODO: check for running program

    // check the file name
    QFileInfo fi(name);
    if (!fi.isFile()) {
	QString msgFmt = i18n("`%s' is not a file or does not exist");
	QString msg(msgFmt.length() + name.length() + 20);
	msg.sprintf(msgFmt, name.data());
	KMsgBox::message(this, kapp->appName(),
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
    }

    emit updateUI();
    slotFileChanged();

    return true;
}

const char GeneralGroup[] = "General";
const char FileVersion[] = "FileVersion";
const char ProgramArgs[] = "ProgramArgs";

void KDebugger::saveProgramSettings()
{
    ASSERT(m_programConfig != 0);
    m_programConfig->setGroup(GeneralGroup);
    m_programConfig->writeEntry(FileVersion, 1);
    m_programConfig->writeEntry(ProgramArgs, m_programArgs);

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
    m_programArgs = m_programConfig->readEntry(ProgramArgs);

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
    cmdString.detach();
    cmdString += "\n";

    CmdQueueItem* cmdItem = 0;
    switch (mode) {
    case QMoverrideMoreEqual:
    case QMoverride:
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
    ASSERT(!m_activeCmd->m_cmdString.isEmpty());

    // commit the command
    m_activeCmd->m_cmdString = "";

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
	ASSERT(!m_activeCmd->m_cmdString.isEmpty());
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
	ASSERT(buflen >= PROMPT_LEN && strncmp(buffer, PROMPT, PROMPT_LEN) == 0);
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
	    parse(m_activeCmd);
	    delete m_activeCmd;
	    m_activeCmd = 0;
	}

	// empty buffer
	m_gdbOutputLen = 0;
	*m_gdbOutput = '\0';

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

static QRegExp MarkerRE = ":[0-9]+:[0-9]+:beg";

// parse len characters from m_gdbOutput
void KDebugger::parse(CmdQueueItem* cmd)
{
    ASSERT(cmd != 0);			/* queue mustn't be empty */

    TRACE(QString(__PRETTY_FUNCTION__) + " parsing " + m_gdbOutput);

    // command string must be committed
    if (!cmd->m_cmdString.isEmpty()) {
	// not commited!
	TRACE("calling KDebugger::parse with uncommited command:\n\t" +
	      cmd->m_cmdString);
	return;
    }

    bool parseMarker = false;
    
    switch (cmd->m_cmd) {
    case DCnoconfirm:
    case DCtty:
	// there is no output
	break;
    case DCinitialize:
	// get version number from preamble
	{
	    int len;
	    QRegExp GDBVersion = "\\nGDB [0-9]+\\.[0-9]+";
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
	    KMsgBox::message(this, kapp->appName(), msg,
			     KMsgBox::STOP, i18n("OK"));
	    m_corefile = "";		/* don't process core file */
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
	    KMsgBox::message(this, kapp->appName(), msg,
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
#ifdef WANT_THIS_PANE
    case DCprintthis:
	handlePrint("this", &m_this);
	break;
#endif
    case DCprint:
	handlePrint(cmd);
	break;
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
	m_filesWindow.updateLineItems();
	break;
    case DCfindType:
	handleFindType(cmd);
	break;
    case DCprintStruct:
	handlePrintStruct(cmd);
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
		    m_filesWindow.activate(startMarker, lineNo-1);
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
    QString msg;
    do {
	start++;			/* skip '\n' */

	if (strncmp(start, "Program ", 7) == 0) {
	    if (strncmp(start, "Program exited", 14) == 0 ||
		strncmp(start, "Program terminated", 18) == 0)
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
	} else if (strstr(start, "re-reading symbols.") != 0) {
	    refreshNeeded = true;
	}

	// next line, please
	start = strchr(start, '\n');
    } while (start != 0);

    m_statusbar.changeItem(msg, ID_STATUS_MSG);

    // refresh files if necessary
    if (refreshNeeded) {
	TRACE("re-reading files");
	m_filesWindow.reloadAllFiles();
    }

    /*
     * If we have any temporary breakpoints, we must update the breakpoint
     * list since this stop may be due to on of them, which would now go
     * away.
     */
    if (refreshNeeded || m_bpTable.haveTemporaryBP()) {
	queueCmd(DCinfobreak, "info breakpoints", QMoverride);
    }

    // get the backtrace if the program is running or if we have a core file
    if (m_programActive || !m_corefile.isEmpty()) {
	queueCmd(DCbt, "bt", QMoverride);
    } else {
	// program finished: erase PC
	m_filesWindow.updatePC(QString(), -1, 0);
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
#ifdef WANT_THIS_PANE
    bool thisPresent = parseLocals(newVars);
#else
    parseLocals(newVars);
#endif

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

#ifdef WANT_THIS_PANE
    // update this
    m_delayedPrintThis = false;
    if (thisPresent) {
	if (false/*isThisPaneVisible()*/) {
	    queueCmd(DCprintthis, "print *this", QMoverride);
	} else {
	    // delay "print *this" until the user makes the pane visible
	    m_delayedPrintThis = true;
	}
    } else {
	VarTree* v = m_this.topLevelExprByName("this");
	removeExpr(&m_this, v);
    }
#endif
}

bool KDebugger::parseLocals(QList<VarTree>& newVars)
{
    bool thisPresent = false;

    // check for possible error conditions
    if (strncmp(m_gdbOutput, "No symbol table", 15) == 0 ||
	strncmp(m_gdbOutput, "No locals", 9) == 0)
    {
	return false;
    }
    QString origName;			/* used in renaming variables */
    const char* p = m_gdbOutput;
    while (*p != '\0') {
	VarTree* variable = parseVar(p);
	if (variable == 0) {
	    break;
	}
	// get some types
	variable->inferTypesOfChildren();
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
		QString newName(origName.length()+20);
		newName.sprintf("%s (%d)", origName.data(), block);
		variable->setText(newName);
	    }
	}
	newVars.append(variable);
	// check for "this"
	if (origName == "this") {
	    thisPresent = true;
	}
    }
    return thisPresent;
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

// parse a printed expression and display it in the window
bool KDebugger::handlePrint(const char* var, ExprWnd* wnd)
{
    VarTree* variable = parseExpr(var);
    if (variable == 0)
	return false;

    // search the expression in the specified window
    QStrList vars;
    wnd->exprList(vars);

    const char* name;
    for (name = vars.first(); name != 0; name = vars.next()) {
	// lookup this variable in the list of new variables
	if (strcmp(var, name) == 0) {
	    break;			/* found! */
	}
    }
    if (name == 0) {
	// not found: append
	TRACE(QString().sprintf("append expression: %s", var));
	wnd->insertExpr(variable);
    } else {
	// found: update
	TRACE(QString().sprintf("update expression: %s", var));
	wnd->updateExpr(variable);
	delete variable;
    }
    
    evalExpressions();			/* enqueue dereferenced pointers */

    return true;
}

VarTree* KDebugger::parseExpr(const char* name, bool wantErrorValue)
{
    VarTree* variable;

    // check for error conditions
    if (strncmp(m_gdbOutput, "Cannot access memory at", 23) == 0 ||
	strncmp(m_gdbOutput, "Attempt to dereference a generic pointer", 40) == 0 ||
	strncmp(m_gdbOutput, "No symbol \"", 11) == 0)
    {
	if (wantErrorValue) {
	    // put the error message as value in the variable
	    char* endMsg = strchr(m_gdbOutput, '\n');
	    if (endMsg != 0)
		*endMsg = '\0';
	    variable = new VarTree(name, VarTree::NKplain);
	    variable->m_value = m_gdbOutput;
	} else {
	    variable = 0;
	}
    } else {
	// parse the variable
	const char* p = m_gdbOutput;
	variable = parseVar(p);
	if (variable == 0) {
	    return 0;
	}
	// set name
	variable->setText(name);
	// get some types
	variable->inferTypesOfChildren();
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
	TRACE(QString(func.length()+file.length()+100).sprintf("frame %s (%s:%d)",func.data(),file.data(),lineNo));
	m_btWindow.insertItem(func);
	m_filesWindow.updatePC(file, lineNo-1, frameNo);

	while (parseFrame(s, frameNo, func, file, lineNo)) {
	    TRACE(QString(func.length()+file.length()+100).sprintf("frame %s (%s:%d)",func.data(),file.data(),lineNo));
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
	m_filesWindow.updatePC(fileName, lineNo-1, frameNo);
    } else {
	m_filesWindow.updatePC(fileName, -1, frameNo);
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
#ifdef WANT_THIS_PANE
	POINTER(m_this);
	STRUCT(m_this);
	TYPE(m_this);
#endif
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
	if (exprItem->m_type == 0 || exprItem->m_type == TypeTable::unknownType())
	    goto repeat;
	exprItem->m_exprIndex = 0;
	exprItem->m_partialValue = exprItem->m_type->m_displayString[0];
	evalStructExpression(exprItem, wnd, false);
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
	cmd = executeCmd(DCprint, queueExpr);
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

	TypeInfo* info = typeTable()[type];

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
		typeTable().registerAlias(type, info);
	    }
	}
	if (info == 0) {
	    TRACE("unknown type");
	    cmd->m_expr->m_type = TypeTable::unknownType();
	} else {
	    cmd->m_expr->m_type = info;
	    /* since this node has a new type, we get its value immediately */
	    cmd->m_expr->m_exprIndex = 0;
	    cmd->m_expr->m_partialValue = info->m_displayString[0];
	    evalStructExpression(cmd->m_expr, cmd->m_exprWnd, false);
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

    QString partValue;
    VarTree* partExpr = parseExpr(cmd->m_expr->getText(), false);
    if (partExpr == 0 ||
	/* we only allow simple values at the moment */
	partExpr->childCount() != 0)
    {
	partValue = "???";
    } else {
	partValue = partExpr->m_value;
    }
    delete partExpr;

    /*
     * Updating a struct value works like this: var->m_partialValue holds
     * the value that we have gathered so far (it's been initialized with
     * var->m_type->m_displayString[0] earlier). Each time we arrive here,
     * we append the printed result followed by the next
     * var->m_type->m_displayString to var->m_partialValue.
     */
    ASSERT(var->m_exprIndex >= 0 && var->m_exprIndex <= typeInfoMaxExpr);
    var->m_partialValue.detach();
    var->m_partialValue += partValue;
    var->m_exprIndex++;			/* next part */
    var->m_partialValue += var->m_type->m_displayString[var->m_exprIndex];

    /* go for more sub-expressions if needed */
    if (var->m_exprIndex < var->m_type->m_numExprs) {
	/* queue a new print command with quite high priority */
	evalStructExpression(var, cmd->m_exprWnd, true);
	return;
    }

    cmd->m_exprWnd->updateStructValue(var);

    evalExpressions();			/* enqueue dereferenced pointers */
}

/* queues a printStruct command; var must have been initialized correctly */
void KDebugger::evalStructExpression(VarTree* var, ExprWnd* wnd, bool immediate)
{
    QString base = var->computeExpr();
    const QString& exprFmt = var->m_type->m_exprStrings[var->m_exprIndex];
    QString expr(exprFmt.length() + base.length() + 10);
    expr.sprintf(exprFmt, base.data());
    TRACE("evalStruct: " + expr);
    CmdQueueItem* cmd = queueCmd(DCprintStruct, "print " + expr,
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

void KDebugger::slotThisExpanding(KTreeViewItem* item, bool& allow)
{
#ifdef WANT_THIS_PANE
    exprExpandingHelper(&m_this, item, allow);
#endif
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
    if (m_state == DSidle) {
	dereferencePointer(wnd, exprItem, true);
    }
}

// add the expression in the edit field to the watch expressions
void KDebugger::slotAddWatch()
{
    QString t = m_watchEdit.text();
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

// place the text of the hightlighted watch expr in the edit field
void KDebugger::slotWatchHighlighted(int index)
{
    KTreeViewItem* item = m_watchVariables.itemAt(index);
    if (item == 0) return;		/* paranoia */
    VarTree* expr = static_cast<VarTree*>(item);
    QString text = expr->computeExpr();
    m_watchEdit.setText(text);
}

/*
 * When the user activates that "this" pane, print *this.
 */
void KDebugger::slotFrameTabChanged(int)
{
#ifdef WANT_THIS_PANE
    if (m_delayedPrintThis && isThisPaneVisible()) {
	/*
	 * We do not use a high-priority command to display "this". This is
	 * just a decision out of the stomach and we can change it if
	 * necessary. But we put it in front of all other low-priority
	 * commands.
	 */
	queueCmd(DCprintthis, "print *this", QMoverrideMoreEqual);
    }
#endif
}

void KDebugger::slotFileChanged()
{
    // set caption
    QString caption = kapp->getCaption();
    if (m_haveExecutable) {
	// basename part of executable
	const char* execBase = m_executable.data();
	int lastSlash = m_executable.findRev('/');
	if (lastSlash >= 0)
	    execBase += lastSlash + 1;
	caption += ": ";
	caption += execBase;
    }
    QString file;
    int line;
    bool anyWindows = m_filesWindow.activeLine(file, line);
    updateLineStatus(anyWindows ? line : -1);
    if (anyWindows) {
	caption += " (";
	caption += file;
	caption += ")";
    }
    setCaption(caption);
}

void KDebugger::slotLineChanged()
{
    QString file;
    int line;
    bool anyWindows = m_filesWindow.activeLine(file, line);
    updateLineStatus(anyWindows ? line : -1);
}

void KDebugger::updateLineStatus(int lineNo)
{
    if (lineNo < 0) {
	m_statusbar.changeItem("", ID_STATUS_LINENO);
    } else {
	QString strLine;
	strLine.sprintf(i18n("Line %d"), lineNo + 1);
	m_statusbar.changeItem(strLine, ID_STATUS_LINENO);
    }
}
