// $Id$

// Copyright by Johannes Sixt
// This file is under GPL, the GNU General Public Licence

#include <kapp.h>
#if QT_VERSION >= 200
#include <klocale.h>			/* i18n */
#include <kmenubar.h>
#include <kconfig.h>
#endif
#include <kiconloader.h>
#include <kstdaccel.h>
#include <kfiledialog.h>
#include <kprocess.h>
#include <qlistbox.h>
#include <qfileinfo.h>
#include "dbgmainwnd.h"
#include "debugger.h"
#include "updateui.h"
#include "commandids.h"
#include "winstack.h"
#include "brkpt.h"
#include "threadlist.h"
#include "ttywnd.h"
#include "mydebug.h"


DebuggerMainWnd::DebuggerMainWnd(const char* name) :
	DockMainWindow(name),
	DebuggerMainWndBase()
{
    setDockManager(new DockManager( this, QString(name)+"_DockManager"));

    QPixmap p;

    DockWidget* dw0 = createDockWidget("Source", p);
    dw0->setCaption(i18n("Source"));
    m_filesWindow = new WinStack(dw0, "files");
    setView(dw0);

    DockWidget* dw1 = createDockWidget("Stack", p);
    dw1->setCaption(i18n("Stack"));
    m_btWindow = new QListBox(dw1, "backtrace");
    DockWidget* dw2 = createDockWidget("Locals", p);
    dw2->setCaption(i18n("Locals"));
    m_localVariables = new ExprWnd(dw2, "locals");
    DockWidget* dw3 = createDockWidget("Watches", p);
    dw3->setCaption(i18n("Watches"));
    m_watches = new WatchWindow(dw3, "watches");
    DockWidget* dw4 = createDockWidget("Registers", p);
    dw4->setCaption(i18n("Registers"));
    m_registers = new RegisterView(dw4, "registers");
    DockWidget* dw5 = createDockWidget("Breakpoints", p);
    dw5->setCaption(i18n("Breakpoints"));
    m_bpTable = new BreakpointTable(dw5, "breakpoints");
    DockWidget* dw6 = createDockWidget("Output", p);
    dw6->setCaption(i18n("Output"));
    m_ttyWindow = new TTYWindow(dw6, "output");
    DockWidget* dw7 = createDockWidget("Threads", p);
    dw7->setCaption(i18n("Threads"));
    m_threads = new ThreadList(dw7, "threads");

    m_menuRecentExecutables = new QPopupMenu();

    initMenu();
    initToolbar();

    setupDebugger(m_localVariables, m_watches->watchVariables(), m_btWindow);
    m_bpTable->setDebugger(m_debugger);

    connect(m_watches, SIGNAL(addWatch()), SLOT(slotAddWatch()));
    connect(m_watches, SIGNAL(deleteWatch()), m_debugger, SLOT(slotDeleteWatch()));

    m_filesWindow->setWindowMenu(m_menuWindow);
    connect(&m_filesWindow->m_findDlg, SIGNAL(closed()), SLOT(updateUI()));
    connect(m_filesWindow, SIGNAL(newFileLoaded()),
	    SLOT(slotNewFileLoaded()));
    connect(m_filesWindow, SIGNAL(toggleBreak(const QString&,int,const DbgAddr&,bool)),
	    this, SLOT(slotToggleBreak(const QString&,int,const DbgAddr&,bool)));
    connect(m_filesWindow, SIGNAL(enadisBreak(const QString&,int,const DbgAddr&)),
	    this, SLOT(slotEnaDisBreak(const QString&,int,const DbgAddr&)));
    connect(m_debugger, SIGNAL(activateFileLine(const QString&,int,const DbgAddr&)),
	    m_filesWindow, SLOT(activate(const QString&,int,const DbgAddr&)));
    connect(m_debugger, SIGNAL(executableUpdated()),
	    m_filesWindow, SLOT(reloadAllFiles()));
    connect(m_debugger, SIGNAL(updatePC(const QString&,int,const DbgAddr&,int)),
	    m_filesWindow, SLOT(updatePC(const QString&,int,const DbgAddr&,int)));
    // value popup communication
    connect(m_filesWindow, SIGNAL(initiateValuePopup(const QString&)),
	    m_debugger, SLOT(slotValuePopup(const QString&)));
    connect(m_debugger, SIGNAL(valuePopup(const QString&)),
	    m_filesWindow, SLOT(slotShowValueTip(const QString&)));
    // disassembling
    connect(m_filesWindow, SIGNAL(disassemble(const QString&, int)),
	    m_debugger, SLOT(slotDisassemble(const QString&, int)));
    connect(m_debugger, SIGNAL(disassembled(const QString&,int,const QList<DisassembledCode>&)),
	    m_filesWindow, SLOT(slotDisassembled(const QString&,int,const QList<DisassembledCode>&)));
    // program stopped
    connect(m_debugger, SIGNAL(programStopped()), SLOT(slotProgramStopped()));
    connect(&m_backTimer, SIGNAL(timeout()), SLOT(slotBackTimer()));
    // tab width
    connect(this, SIGNAL(setTabWidth(int)), m_filesWindow, SIGNAL(setTabWidth(int)));

    // Establish communication when right clicked on file window.
    connect(&m_filesWindow->m_menuFloat, SIGNAL(activated(int)),
	    SLOT(menuCallback(int)));

    // Connection when right clicked on file window before any file is
    // loaded.
    connect(&m_filesWindow->m_menuFileFloat, SIGNAL(activated(int)),
	    SLOT(menuCallback(int)));

    // route unhandled menu items to winstack
    connect(this, SIGNAL(forwardMenuCallback(int)), m_filesWindow, SLOT(menuCallback(int)));
    // file/line updates
    connect(m_filesWindow, SIGNAL(fileChanged()), SLOT(slotFileChanged()));
    connect(m_filesWindow, SIGNAL(lineChanged()), SLOT(slotLineChanged()));

    // connect breakpoint table
    connect(m_bpTable, SIGNAL(activateFileLine(const QString&,int,const DbgAddr&)),
	    m_filesWindow, SLOT(activate(const QString&,int,const DbgAddr&)));
    connect(m_debugger, SIGNAL(updateUI()), m_bpTable, SLOT(updateUI()));
    connect(m_debugger, SIGNAL(breakpointsChanged()), m_bpTable, SLOT(updateBreakList()));

    connect(m_debugger, SIGNAL(registersChanged(QList<RegisterInfo>&)),
	    m_registers, SLOT(updateRegisters(QList<RegisterInfo>&)));

    // thread window
    connect(m_debugger, SIGNAL(threadsChanged(QList<ThreadInfo>&)),
	    m_threads, SLOT(updateThreads(QList<ThreadInfo>&)));
    connect(m_threads, SIGNAL(setThread(int)),
	    m_debugger, SLOT(setThread(int)));

    // view menu changes when docking state changes
    connect(dockManager, SIGNAL(change()), SLOT(updateUI()));

    restoreSettings(kapp->getConfig());

    connect(m_menuRecentExecutables, SIGNAL(activated(int)), SLOT(slotRecentExec(int)));
    fillRecentExecMenu();

    updateUI();
    m_bpTable->updateUI();
    slotFileChanged();
}

DebuggerMainWnd::~DebuggerMainWnd()
{
    saveSettings(kapp->getConfig());
    // must delete m_debugger early since it references our windows
    delete m_debugger;
    m_debugger = 0;
    // must disconnect from dockManager since it keeps emitting signals
    dockManager->disconnect(this);

    delete m_menuWindow;
    delete m_menuBrkpt;
    delete m_menuProgram;
    delete m_menuView;
    delete m_menuFile;
}

void DebuggerMainWnd::initMenu()
{
    m_menuFile = new QPopupMenu;
    m_menuFile->insertItem(i18n("&Open Source..."), ID_FILE_OPEN);
    m_menuFile->insertItem(i18n("&Reload Source"), ID_FILE_RELOAD);
    m_menuFile->insertSeparator();
    m_menuFile->insertItem(i18n("&Executable..."), ID_FILE_EXECUTABLE);
    m_menuFile->insertItem(i18n("Recent E&xecutables"), m_menuRecentExecutables);
    m_menuFile->insertItem(i18n("&Settings..."), ID_FILE_PROG_SETTINGS);
    m_menuFile->insertItem(i18n("&Core dump..."), ID_FILE_COREFILE);
    m_menuFile->insertSeparator();
    m_menuFile->insertItem(i18n("&Global Options..."), ID_FILE_GLOBAL_OPTIONS);
    m_menuFile->insertSeparator();
    m_menuFile->insertItem(i18n("&Quit"), ID_FILE_QUIT);
    m_menuFile->setAccel(keys->open(), ID_FILE_OPEN);
    m_menuFile->setAccel(keys->quit(), ID_FILE_QUIT);

    m_menuView = new QPopupMenu;
    m_menuView->setCheckable(true);
    m_menuView->insertItem(i18n("&Find..."), ID_VIEW_FINDDLG);
    m_menuView->insertSeparator();
    m_menuView->insertItem(i18n("Source &code"), ID_VIEW_SOURCE);
    m_menuView->insertItem(i18n("Stac&k"), ID_VIEW_STACK);
    m_menuView->insertItem(i18n("&Locals"), ID_VIEW_LOCALS);
    m_menuView->insertItem(i18n("&Watched expressions"), ID_VIEW_WATCHES);
    m_menuView->insertItem(i18n("&Registers"), ID_VIEW_REGISTERS);
    m_menuView->insertItem(i18n("&Breakpoints"), ID_BRKPT_LIST);
    m_menuView->insertItem(i18n("T&hreads"), ID_VIEW_THREADS);
    m_menuView->insertItem(i18n("&Output"), ID_VIEW_OUTPUT);
    m_menuView->insertSeparator();
    m_menuView->insertItem(i18n("Toggle &Toolbar"), ID_VIEW_TOOLBAR);
    m_menuView->insertItem(i18n("Toggle &Statusbar"), ID_VIEW_STATUSBAR);
    m_menuView->setAccel(keys->find(), ID_VIEW_FINDDLG);

    m_menuProgram = new QPopupMenu;
    m_menuProgram->insertItem(i18n("&Run"), ID_PROGRAM_RUN);
    m_menuProgram->insertItem(i18n("Step &into"), ID_PROGRAM_STEP);
    m_menuProgram->insertItem(i18n("Step &over"), ID_PROGRAM_NEXT);
    m_menuProgram->insertItem(i18n("Step o&ut"), ID_PROGRAM_FINISH);
    m_menuProgram->insertItem(i18n("Run to &cursor"), ID_PROGRAM_UNTIL);
    m_menuProgram->insertSeparator();
    m_menuProgram->insertItem(i18n("&Break"), ID_PROGRAM_BREAK);
    m_menuProgram->insertItem(i18n("&Kill"), ID_PROGRAM_KILL);
    m_menuProgram->insertItem(i18n("Re&start"), ID_PROGRAM_RUN_AGAIN);
    m_menuProgram->insertItem(i18n("A&ttach..."), ID_PROGRAM_ATTACH);
    m_menuProgram->insertSeparator();
    m_menuProgram->insertItem(i18n("&Arguments..."), ID_PROGRAM_ARGS);
    m_menuProgram->setAccel(Key_F5, ID_PROGRAM_RUN);
    m_menuProgram->setAccel(Key_F8, ID_PROGRAM_STEP);
    m_menuProgram->setAccel(Key_F10, ID_PROGRAM_NEXT);
    m_menuProgram->setAccel(Key_F6, ID_PROGRAM_FINISH);
    m_menuProgram->setAccel(Key_F7, ID_PROGRAM_UNTIL);

    m_menuBrkpt = new QPopupMenu;
    m_menuBrkpt->insertItem(i18n("Set/Clear &breakpoint"), ID_BRKPT_SET);
    m_menuBrkpt->insertItem(i18n("Set &temporary breakpoint"), ID_BRKPT_TEMP);
    m_menuBrkpt->insertItem(i18n("&Enable/Disable breakpoint"), ID_BRKPT_ENABLE);
    m_menuBrkpt->setAccel(Key_F9, ID_BRKPT_SET);
    m_menuBrkpt->setAccel(SHIFT+Key_F9, ID_BRKPT_TEMP);
    m_menuBrkpt->setAccel(CTRL+Key_F9, ID_BRKPT_ENABLE);

    m_menuWindow = new QPopupMenu;
    m_menuWindow->insertItem(i18n("&More..."), ID_WINDOW_MORE);
  
    connect(m_menuFile, SIGNAL(activated(int)), SLOT(menuCallback(int)));
    connect(m_menuView, SIGNAL(activated(int)), SLOT(menuCallback(int)));
    connect(m_menuProgram, SIGNAL(activated(int)), SLOT(menuCallback(int)));
    connect(m_menuBrkpt, SIGNAL(activated(int)), SLOT(menuCallback(int)));
    connect(m_menuWindow, SIGNAL(activated(int)), SLOT(menuCallback(int)));

    KMenuBar* menu = menuBar();
    menu->insertItem(i18n("&File"), m_menuFile);
    menu->insertItem(i18n("&View"), m_menuView);
    menu->insertItem(i18n("E&xecution"), m_menuProgram);
    menu->insertItem(i18n("&Breakpoint"), m_menuBrkpt);
    menu->insertItem(i18n("&Window"), m_menuWindow);
}

#if QT_VERSION < 200
static QPixmap BarIcon(const char* name)
{
    return kapp->getIconLoader()->loadIcon(name);
}
#endif

void DebuggerMainWnd::initToolbar()
{
    KToolBar* toolbar = toolBar();
    toolbar->insertButton(BarIcon("execopen.xpm"),ID_FILE_EXECUTABLE, true,
			   i18n("Executable"));
    toolbar->insertButton(BarIcon("fileopen.xpm"),ID_FILE_OPEN, true,
			   i18n("Open a source file"));
    toolbar->insertButton(BarIcon("reload.xpm"),ID_FILE_RELOAD, true,
			   i18n("Reload source file"));
    toolbar->insertSeparator();
    toolbar->insertButton(BarIcon("pgmrun.xpm"),ID_PROGRAM_RUN, true,
			   i18n("Run/Continue"));
    toolbar->insertButton(BarIcon("pgmstep.xpm"),ID_PROGRAM_STEP, true,
			   i18n("Step into"));
    toolbar->insertButton(BarIcon("pgmnext.xpm"),ID_PROGRAM_NEXT, true,
			   i18n("Step over"));
    toolbar->insertButton(BarIcon("pgmfinish.xpm"),ID_PROGRAM_FINISH, true,
			   i18n("Step out"));
    toolbar->insertSeparator();
    toolbar->insertButton(BarIcon("brkpt.xpm"),ID_BRKPT_SET, true,
			   i18n("Breakpoint"));
    toolbar->insertSeparator();
    toolbar->insertButton(BarIcon("search.xpm"),ID_VIEW_FINDDLG, true,
			   i18n("Search"));

    connect(toolbar, SIGNAL(clicked(int)), SLOT(menuCallback(int)));
    
    initAnimation();

    KStatusBar* statusbar = statusBar();
    statusbar->insertItem(m_statusActive, ID_STATUS_ACTIVE);
    statusbar->insertItem(i18n("Line 00000"), ID_STATUS_LINENO);
    statusbar->insertItem("", ID_STATUS_MSG);	/* message pane */

    // reserve some translations
    i18n("Restart");
    i18n("Core dump");
}

/*
 * We must override KTMainWindow's handling of close events since we have
 * only one toplevel window, which lives on the stack (which KTMainWindow
 * can't live with :-( )
 */
void DebuggerMainWnd::closeEvent(QCloseEvent* e)
{
#if QT_VERSION >= 200
    clearWFlags(WDestructiveClose);
#endif

    if (m_debugger != 0) {
	m_debugger->shutdown();
    }

    e->accept();
    kapp->quit();
}


// instance properties
void DebuggerMainWnd::saveProperties(KConfig* config)
{
    // session management
    QString executable = "";
    if (m_debugger != 0) {
	executable = m_debugger->executable();
    }
    config->writeEntry("executable", executable);
}

void DebuggerMainWnd::readProperties(KConfig* config)
{
    // session management
    QString execName = config->readEntry("executable");

    TRACE("readProperties: executable=" + execName);
    if (!execName.isEmpty()) {
	debugProgram(execName);
    }
}

const char WindowGroup[] = "Windows";

void DebuggerMainWnd::saveSettings(KConfig* config)
{
    KConfigGroupSaver g(config, WindowGroup);

    writeDockConfig(config);

    DebuggerMainWndBase::saveSettings(config);
}

void DebuggerMainWnd::restoreSettings(KConfig* config)
{
    KConfigGroupSaver g(config, WindowGroup);

    readDockConfig(config);

    DebuggerMainWndBase::restoreSettings(config);

    emit setTabWidth(m_tabWidth);
}

void DebuggerMainWnd::menuCallback(int item)
{
    switch (item) {
    case ID_FILE_OPEN:
	{
	    QString fileName = myGetFileName(i18n("Open"),
					     m_lastDirectory,
					     makeSourceFilter(), this);

	    if (!fileName.isEmpty())
	    {
		QFileInfo fi(fileName);
		m_lastDirectory = fi.dirPath();
		m_filesWindow->setExtraDirectory(m_lastDirectory);
		m_filesWindow->activateFile(fileName);
	    }
	}
	break;
    case ID_FILE_QUIT:
	if (m_debugger != 0) {
	    m_debugger->shutdown();
	}
	kapp->quit();
	break;
    case ID_VIEW_TOOLBAR:
	enableToolBar();
	break;
    case ID_VIEW_STATUSBAR:
	enableStatusBar();
	break;
    case ID_PROGRAM_UNTIL:
	if (m_debugger != 0)
	{
	    QString file;
	    int lineNo;
	    if (m_filesWindow->activeLine(file, lineNo))
		m_debugger->runUntil(file, lineNo);
	}
	break;
    case ID_BRKPT_LIST:
	showhideWindow(m_bpTable);
	break;
    case ID_VIEW_SOURCE:
	showhideWindow(m_filesWindow);
	break;
    case ID_VIEW_STACK:
	showhideWindow(m_btWindow);
	break;
    case ID_VIEW_LOCALS:
	showhideWindow(m_localVariables);
	break;
    case ID_VIEW_WATCHES:
	showhideWindow(m_watches);
	break;
    case ID_VIEW_REGISTERS:
	showhideWindow(m_registers);
	break;
    case ID_VIEW_THREADS:
	showhideWindow(m_threads);
	break;
    case ID_VIEW_OUTPUT:
	showhideWindow(m_ttyWindow);
	break;
    default:
	// forward all others
	if (!handleCommand(item))
	    emit forwardMenuCallback(item);
	else if (item == ID_FILE_EXECUTABLE) {
	    // special: this may have changed m_lastDirectory
	    m_filesWindow->setExtraDirectory(m_lastDirectory);
	    fillRecentExecMenu();
	} else {
	    // start timer to move window into background
	    switch (item) {
	    case ID_PROGRAM_STEP:
	    case ID_PROGRAM_NEXT:
	    case ID_PROGRAM_FINISH:
	    case ID_PROGRAM_UNTIL:
	    case ID_PROGRAM_RUN:
		if (m_popForeground)
		    intoBackground();
		break;
	    }
	}
    }
    updateUI();
}

void DebuggerMainWnd::updateUI()
{
    // enumerate all menus
    {
	UpdateMenuUI updateMenu(m_menuFile, this, SLOT(updateUIItem(UpdateUI*)));
	updateMenu.iterateMenu();
    }
    {
	UpdateMenuUI updateMenu(m_menuView, this, SLOT(updateUIItem(UpdateUI*)));
	updateMenu.iterateMenu();
    }
    {
	UpdateMenuUI updateMenu(m_menuProgram, this, SLOT(updateUIItem(UpdateUI*)));
	updateMenu.iterateMenu();
    }
    {
	UpdateMenuUI updateMenu(m_menuBrkpt, this, SLOT(updateUIItem(UpdateUI*)));
	updateMenu.iterateMenu();
    }

    // Update winstack float popup items
    {
	UpdateMenuUI updateMenu(&m_filesWindow->m_menuFloat, this,
				SLOT(updateUIItem(UpdateUI*)));
	updateMenu.iterateMenu();
    }

    // Update winstack float file popup items
    {
	UpdateMenuUI updateMenu(&m_filesWindow->m_menuFileFloat, this,
				SLOT(updateUIItem(UpdateUI*)));
	updateMenu.iterateMenu();
    }

    // toolbar
    static const int toolIds[] = {
	ID_PROGRAM_RUN, ID_PROGRAM_STEP, ID_PROGRAM_NEXT, ID_PROGRAM_FINISH,
	ID_BRKPT_SET
    };
    UpdateToolbarUI updateToolbar(toolBar(), this, SLOT(updateUIItem(UpdateUI*)),
				  toolIds, sizeof(toolIds)/sizeof(toolIds[0]));
    updateToolbar.iterateToolbar();
}

void DebuggerMainWnd::dockUpdateHelper(UpdateUI* item, QWidget* w)
{
    if (canChangeDockVisibility(w)) {
	item->enable(true);
	item->setCheck(isDockVisible(w));
    } else {
	item->enable(false);
	item->setCheck(false);
    }
}

void DebuggerMainWnd::updateUIItem(UpdateUI* item)
{
    switch (item->id) {
    case ID_VIEW_FINDDLG:
	item->setCheck(m_filesWindow->m_findDlg.isVisible());
	break;
    case ID_BRKPT_SET:
    case ID_BRKPT_TEMP:
	item->enable(m_debugger->canChangeBreakpoints());
	break;
    case ID_BRKPT_ENABLE:
	item->enable(m_debugger->canChangeBreakpoints());
	break;
    case ID_BRKPT_LIST:
	dockUpdateHelper(item, m_bpTable);
	break;
    case ID_VIEW_SOURCE:
	dockUpdateHelper(item, m_filesWindow);
	break;
    case ID_VIEW_STACK:
	dockUpdateHelper(item, m_btWindow);
	break;
    case ID_VIEW_LOCALS:
	dockUpdateHelper(item, m_localVariables);
	break;
    case ID_VIEW_WATCHES:
	dockUpdateHelper(item, m_watches);
	break;
    case ID_VIEW_REGISTERS:
	dockUpdateHelper(item, m_registers);
	break;
    case ID_VIEW_THREADS:
	dockUpdateHelper(item, m_threads);
	break;
    case ID_VIEW_OUTPUT:
	dockUpdateHelper(item, m_ttyWindow);
	break;
    default:
	DebuggerMainWndBase::updateUIItem(item);
	break;
    }
    // line number is updated in slotLineChanged

    // update statusbar
    statusBar()->changeItem(m_debugger->isProgramActive() ?
			    static_cast<const char*>(m_statusActive) : "",
			    ID_STATUS_ACTIVE);
}

void DebuggerMainWnd::updateLineItems()
{
    m_filesWindow->updateLineItems(m_debugger);
}

void DebuggerMainWnd::slotAddWatch()
{
    if (m_debugger != 0) {
	QString t = m_watches->watchText();
	m_debugger->addWatch(t);
    }
}

void DebuggerMainWnd::slotFileChanged()
{
    // set caption
#if QT_VERSION < 200
    QString caption = kapp->getCaption();
#else
    QString caption;
#endif
    if (m_debugger->haveExecutable()) {
	// basename part of executable
	QString executable = m_debugger->executable();
	const char* execBase = executable.data();
	int lastSlash = executable.findRev('/');
	if (lastSlash >= 0)
	    execBase += lastSlash + 1;
#if QT_VERSION < 200
	caption += ": ";
#endif
	caption += execBase;
    }
    QString file;
    int line;
    bool anyWindows = m_filesWindow->activeLine(file, line);
    updateLineStatus(anyWindows ? line : -1);
    if (anyWindows) {
	caption += " (";
	caption += file;
	caption += ")";
    }
    setCaption(caption);
}

void DebuggerMainWnd::slotLineChanged()
{
    QString file;
    int line;
    bool anyWindows = m_filesWindow->activeLine(file, line);
    updateLineStatus(anyWindows ? line : -1);
}

void DebuggerMainWnd::slotNewFileLoaded()
{
    // updates program counter in the new file
    if (m_debugger != 0)
	m_filesWindow->updateLineItems(m_debugger);
}

void DebuggerMainWnd::updateLineStatus(int lineNo)
{
    if (lineNo < 0) {
	statusBar()->changeItem("", ID_STATUS_LINENO);
    } else {
	QString strLine;
	strLine.sprintf(i18n("Line %d"), lineNo + 1);
	statusBar()->changeItem(strLine, ID_STATUS_LINENO);
    }
}

DockWidget* DebuggerMainWnd::dockParent(QWidget* w)
{
    while ((w = w->parentWidget()) != 0) {
	if (w->isA("DockWidget"))
	    return static_cast<DockWidget*>(w);
    }
    return 0;
}

bool DebuggerMainWnd::isDockVisible(QWidget* w)
{
    DockWidget* d = dockParent(w);
    return d != 0 && d->mayBeHide();
}

bool DebuggerMainWnd::canChangeDockVisibility(QWidget* w)
{
    DockWidget* d = dockParent(w);
    return d != 0 && (d->mayBeHide() || d->mayBeShow());
}

void DebuggerMainWnd::showhideWindow(QWidget* w)
{
    DockWidget* d = dockParent(w);
    if (d == 0) {
	TRACE(QString("no dockParent found: ") + d->name());
	return;
    }
    d->changeHideShowState();
//    updateUI();
}

KToolBar* DebuggerMainWnd::dbgToolBar()
{
    return toolBar();
}

KStatusBar* DebuggerMainWnd::dbgStatusBar()
{
    return statusBar();
}

QWidget* DebuggerMainWnd::dbgMainWnd()
{
    return this;
}

TTYWindow* DebuggerMainWnd::ttyWindow()
{
    return m_ttyWindow;
}

void DebuggerMainWnd::slotNewStatusMsg()
{
    DebuggerMainWndBase::slotNewStatusMsg();
}

void DebuggerMainWnd::slotAnimationTimeout()
{
    DebuggerMainWndBase::slotAnimationTimeout();
}

void DebuggerMainWnd::doGlobalOptions()
{
    int oldTabWidth = m_tabWidth;

    DebuggerMainWndBase::doGlobalOptions();

    if (m_tabWidth != oldTabWidth) {
	emit setTabWidth(m_tabWidth);
    }
}

void DebuggerMainWnd::slotDebuggerStarting()
{
    DebuggerMainWndBase::slotDebuggerStarting();
}

void DebuggerMainWnd::slotToggleBreak(const QString& fileName, int lineNo,
				      const DbgAddr& address, bool temp)
{
    // lineNo is zero-based
    if (m_debugger != 0) {
	m_debugger->setBreakpoint(fileName, lineNo, address, temp);
    }
}

void DebuggerMainWnd::slotEnaDisBreak(const QString& fileName, int lineNo,
				      const DbgAddr& address)
{
    // lineNo is zero-based
    if (m_debugger != 0) {
	m_debugger->enableDisableBreakpoint(fileName, lineNo, address);
    }
}

QString DebuggerMainWnd::createOutputWindow()
{
    QString tty = DebuggerMainWndBase::createOutputWindow();
    if (!tty.isEmpty()) {
	connect(m_outputTermProc, SIGNAL(processExited(KProcess*)),
		SLOT(slotTermEmuExited()));
    }
    return tty;
}

void DebuggerMainWnd::slotTermEmuExited()
{
    shutdownTermWindow();
}

#include <X11/Xlib.h>			/* XRaiseWindow, XLowerWindow */

void DebuggerMainWnd::slotProgramStopped()
{
    // when the program stopped, move the window to the foreground
    if (m_popForeground) {
	::XRaiseWindow(x11Display(), winId());
    }
    m_backTimer.stop();
}

void DebuggerMainWnd::intoBackground()
{
    m_backTimer.start(m_backTimeout, true);	/* single-shot */
}

void DebuggerMainWnd::slotBackTimer()
{
    ::XLowerWindow(x11Display(), winId());
}

void DebuggerMainWnd::slotRecentExec(int item)
{
    if (item >= 0 && item < int(m_recentExecList.count())) {
	QString exe = m_recentExecList.at(item);
	if (debugProgramInteractive(exe)) {
	    addRecentExec(exe);
	} else {
	    removeRecentExec(exe);
	}
	fillRecentExecMenu();
    }
}

void DebuggerMainWnd::fillRecentExecMenu()
{
    m_menuRecentExecutables->clear();
    for (uint i = 0; i < m_recentExecList.count(); i++) {
	m_menuRecentExecutables->insertItem(m_recentExecList.at(i));
    }
}

QString DebuggerMainWnd::makeSourceFilter()
{
    QString f;
    f = m_sourceFilter + " " + m_headerFilter + i18n("|All source files\n");
    f += m_sourceFilter + i18n("|Source files\n");
    f += m_headerFilter + i18n("|Header files\n");
    f += i18n("*|All files");
    return f;
}


#include "dbgmainwnd.moc"
