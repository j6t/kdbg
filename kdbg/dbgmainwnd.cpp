// $Id$

// Copyright by Johannes Sixt
// This file is under GPL, the GNU General Public Licence

#include <kapp.h>
#include <klocale.h>			/* i18n */
#include <kmessagebox.h>
#include <kconfig.h>
#include <kstatusbar.h>
#include <kiconloader.h>
#include <kstdaccel.h>
#include <kstdaction.h>
#include <kaction.h>
#include <kpopupmenu.h>
#include <kfiledialog.h>
#include <kprocess.h>
#include <kkeydialog.h>
#include <kanimwidget.h>
#include <kwin.h>
#include <qlistbox.h>
#include <qfileinfo.h>
#include "dbgmainwnd.h"
#include "debugger.h"
#include "commandids.h"
#include "winstack.h"
#include "brkpt.h"
#include "threadlist.h"
#include "memwindow.h"
#include "ttywnd.h"
#include "procattach.h"
#include "dbgdriver.h"
#include "mydebug.h"
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif


DebuggerMainWnd::DebuggerMainWnd(const char* name) :
	KDockMainWindow(0, name),
	DebuggerMainWndBase()
{
    QPixmap p;

    KDockWidget* dw0 = createDockWidget("Source", p, 0, i18n("Source"));
    m_filesWindow = new WinStack(dw0, "files");
    dw0->setWidget(m_filesWindow);
    dw0->setDockSite(KDockWidget::DockCorner);
    dw0->setEnableDocking(KDockWidget::DockNone);
    setView(dw0);
    setMainDockWidget(dw0);

    KDockWidget* dw1 = createDockWidget("Stack", p, 0, i18n("Stack"));
    m_btWindow = new QListBox(dw1, "backtrace");
    dw1->setWidget(m_btWindow);
    KDockWidget* dw2 = createDockWidget("Locals", p, 0, i18n("Locals"));
    m_localVariables = new ExprWnd(dw2, "locals");
    dw2->setWidget(m_localVariables);
    KDockWidget* dw3 = createDockWidget("Watches", p, 0, i18n("Watches"));
    m_watches = new WatchWindow(dw3, "watches");
    dw3->setWidget(m_watches);
    KDockWidget* dw4 = createDockWidget("Registers", p, 0, i18n("Registers"));
    m_registers = new RegisterView(dw4, "registers");
    dw4->setWidget(m_registers);
    KDockWidget* dw5 = createDockWidget("Breakpoints", p, 0, i18n("Breakpoints"));
    m_bpTable = new BreakpointTable(dw5, "breakpoints");
    dw5->setWidget(m_bpTable);
    KDockWidget* dw6 = createDockWidget("Output", p, 0, i18n("Output"));
    m_ttyWindow = new TTYWindow(dw6, "output");
    dw6->setWidget(m_ttyWindow);
    KDockWidget* dw7 = createDockWidget("Threads", p, 0, i18n("Threads"));
    m_threads = new ThreadList(dw7, "threads");
    dw7->setWidget(m_threads);
    KDockWidget* dw8 = createDockWidget("Memory", p, 0, i18n("Memory"));
    m_memoryWindow = new MemoryWindow(dw8, "memory");
    dw8->setWidget(m_memoryWindow);

    setupDebugger(this, m_localVariables, m_watches->watchVariables(), m_btWindow);
    m_bpTable->setDebugger(m_debugger);
    m_memoryWindow->setDebugger(m_debugger);

    initKAction();
    initToolbar(); // kind of obsolete?

    connect(m_watches, SIGNAL(addWatch()), SLOT(slotAddWatch()));
    connect(m_watches, SIGNAL(deleteWatch()), m_debugger, SLOT(slotDeleteWatch()));

    KAction* windowMenu = actionCollection()->action("window");
    m_filesWindow->setWindowMenu(static_cast<KActionMenu*>(windowMenu)->popupMenu());
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
    connect(m_filesWindow, SIGNAL(moveProgramCounter(const QString&,int,const DbgAddr&)),
	    m_debugger, SLOT(setProgramCounter(const QString&,int,const DbgAddr&)));
    // program stopped
    connect(m_debugger, SIGNAL(programStopped()), SLOT(slotProgramStopped()));
    connect(&m_backTimer, SIGNAL(timeout()), SLOT(slotBackTimer()));
    // tab width
    connect(this, SIGNAL(setTabWidth(int)), m_filesWindow, SIGNAL(setTabWidth(int)));

    // Establish communication when right clicked on file window.
    connect(m_filesWindow, SIGNAL(filesRightClick(const QPoint &)),
	    SLOT(slotFileWndMenu(const QPoint &)));

    // Connection when right clicked on file window before any file is
    // loaded.
    connect(m_filesWindow, SIGNAL(clickedRight(const QPoint &)),
	    SLOT(slotFileWndEmptyMenu(const QPoint &)));

    // file/line updates
    connect(m_filesWindow, SIGNAL(fileChanged()), SLOT(slotFileChanged()));
    connect(m_filesWindow, SIGNAL(lineChanged()), SLOT(slotLineChanged()));

    // connect breakpoint table
    connect(m_bpTable, SIGNAL(activateFileLine(const QString&,int,const DbgAddr&)),
	    m_filesWindow, SLOT(activate(const QString&,int,const DbgAddr&)));
    connect(m_debugger, SIGNAL(updateUI()), m_bpTable, SLOT(updateUI()));
    connect(m_debugger, SIGNAL(breakpointsChanged()), m_bpTable, SLOT(updateBreakList()));
    connect(m_debugger, SIGNAL(breakpointsChanged()), m_bpTable, SLOT(updateUI()));

    connect(m_debugger, SIGNAL(registersChanged(QList<RegisterInfo>&)),
	    m_registers, SLOT(updateRegisters(QList<RegisterInfo>&)));

    connect(m_debugger, SIGNAL(memoryDumpChanged(const QString&, QList<MemoryDump>&)),
	    m_memoryWindow, SLOT(slotNewMemoryDump(const QString&, QList<MemoryDump>&)));
    connect(m_debugger, SIGNAL(saveProgramSpecific(KConfigBase*)),
	    m_memoryWindow, SLOT(saveProgramSpecific(KConfigBase*)));
    connect(m_debugger, SIGNAL(restoreProgramSpecific(KConfigBase*)),
	    m_memoryWindow, SLOT(restoreProgramSpecific(KConfigBase*)));

    // thread window
    connect(m_debugger, SIGNAL(threadsChanged(QList<ThreadInfo>&)),
	    m_threads, SLOT(updateThreads(QList<ThreadInfo>&)));
    connect(m_threads, SIGNAL(setThread(int)),
	    m_debugger, SLOT(setThread(int)));

    // view menu changes when docking state changes
    connect(dockManager, SIGNAL(change()), SLOT(updateUI()));

    // popup menu of the local variables window
    connect(m_localVariables, SIGNAL(contextMenuRequested(QListViewItem*, const QPoint&, int)),
	    this, SLOT(slotLocalsPopup(QListViewItem*, const QPoint&)));

    restoreSettings(kapp->config());

    updateUI();
    m_bpTable->updateUI();
    slotFileChanged();
}

DebuggerMainWnd::~DebuggerMainWnd()
{
    saveSettings(kapp->config());
    // must delete m_debugger early since it references our windows
    delete m_debugger;
    m_debugger = 0;

    delete m_memoryWindow;
    delete m_threads;
    delete m_ttyWindow;
    delete m_bpTable;
    delete m_registers;
    delete m_watches;
    delete m_localVariables;
    delete m_btWindow;
    delete m_filesWindow;
}

void DebuggerMainWnd::initKAction()
{
    // file menu
    KAction* open = KStdAction::open(this, SLOT(slotFileOpen()), 
                      actionCollection());
    open->setText(i18n("&Open Source..."));
    (void)new KAction(i18n("&Reload Source"), "reload", 0, m_filesWindow, 
                      SLOT(slotFileReload()), actionCollection(), 
                      "file_reload");
    (void)new KAction(i18n("&Executable..."), "execopen", 0, this, 
                      SLOT(slotFileExe()), actionCollection(), 
                      "file_executable");
    m_recentExecAction = new KRecentFilesAction(i18n("Recent E&xecutables"), 0,
		      this, SLOT(slotRecentExec(const KURL&)),
		      actionCollection(), "file_executable_recent");
    (void)new KAction(i18n("&Core dump..."), 0, this, SLOT(slotFileCore()),
                      actionCollection(), "file_core_dump");
    KStdAction::quit(this, SLOT(slotFileQuit()), actionCollection());

    // settings menu
    (void)new KAction(i18n("This &Program..."), 0, this,
		      SLOT(slotFileProgSettings()), actionCollection(),
		      "settings_program");
    (void)new KAction(i18n("&Global Options..."), 0, this, 
                      SLOT(slotFileGlobalSettings()), actionCollection(),
                      "settings_global");
    KStdAction::keyBindings(this, SLOT(slotConfigureKeys()), actionCollection());
    KStdAction::showToolbar(this, SLOT(slotViewToolbar()), actionCollection());
    KStdAction::showStatusbar(this, SLOT(slotViewStatusbar()), actionCollection());

    // view menu
    (void)new KToggleAction(i18n("&Find"), "find", CTRL+Key_F, m_filesWindow,
			    SLOT(slotViewFind()), actionCollection(),
			    "view_find");
    i18n("Source &code");
    struct { QString text; QWidget* w; QString id; } dw[] = {
	{ i18n("Stac&k"), m_btWindow, "view_stack"},
	{ i18n("&Locals"), m_localVariables, "view_locals"},
	{ i18n("&Watched expressions"), m_watches, "view_watched_expressions"},
	{ i18n("&Registers"), m_registers, "view_registers"},
	{ i18n("&Breakpoints"), m_bpTable, "view_breakpoints"},
	{ i18n("T&hreads"), m_threads, "view_threads"},
	{ i18n("&Output"), m_ttyWindow, "view_output"},
	{ i18n("&Memory"), m_memoryWindow, "view_memory"}
    };
    for (unsigned i = 0; i < sizeof(dw)/sizeof(dw[0]); i++) {
	KDockWidget* d = dockParent(dw[i].w);
	(void)new KToggleAction(dw[i].text, 0, d, SLOT(changeHideShowState()),
			  actionCollection(), dw[i].id);
    }

    
    // execution menu
    KAction* a = new KAction(i18n("&Run"), "pgmrun", Key_F5, m_debugger, 
		      SLOT(programRun()), actionCollection(), "exec_run");
    connect(a, SIGNAL(activated()), this, SLOT(intoBackground()));
    a = new KAction(i18n("Step &into"), "pgmstep", Key_F8, m_debugger, 
                      SLOT(programStep()), actionCollection(), 
                      "exec_step_into");
    connect(a, SIGNAL(activated()), this, SLOT(intoBackground()));
    a = new KAction(i18n("Step &over"), "pgmnext", Key_F10, m_debugger, 
                      SLOT(programNext()), actionCollection(), 
                      "exec_step_over");
    connect(a, SIGNAL(activated()), this, SLOT(intoBackground()));
    a = new KAction(i18n("Step o&ut"), "pgmfinish", Key_F6, m_debugger,
                      SLOT(programFinish()), actionCollection(), 
                      "exec_step_out");
    connect(a, SIGNAL(activated()), this, SLOT(intoBackground()));
    a = new KAction(i18n("Run to &cursor"), Key_F7, this,
                      SLOT(slotExecUntil()), actionCollection(), 
                      "exec_run_to_cursor");
    connect(a, SIGNAL(activated()), this, SLOT(intoBackground()));
    a = new KAction(i18n("Step i&nto by instruction"), "pgmstepi", 
		      SHIFT+Key_F8, m_debugger, SLOT(programStepi()), 
		      actionCollection(), "exec_step_into_by_insn");
    connect(a, SIGNAL(activated()), this, SLOT(intoBackground()));
    a = new KAction(i18n("Step o&ver by instruction"), "pgmnexti", 
		      SHIFT+Key_F10, m_debugger, SLOT(programNexti()), 
		      actionCollection(), "exec_step_over_by_insn");
    connect(a, SIGNAL(activated()), this, SLOT(intoBackground()));
    (void)new KAction(i18n("&Program counter to current line"), 0,
		      m_filesWindow, SLOT(slotMoveProgramCounter()),
		      actionCollection(), "exec_movepc");
    (void)new KAction(i18n("&Break"), 0, m_debugger,
                      SLOT(programBreak()), actionCollection(),
                      "exec_break");
    (void)new KAction(i18n("&Kill"), 0, m_debugger,
                      SLOT(programKill()), actionCollection(),
                      "exec_kill");
    (void)new KAction(i18n("Re&start"), 0, m_debugger,
                      SLOT(programRunAgain()), actionCollection(),
                      "exec_restart");
    (void)new KAction(i18n("A&ttach..."), 0, this,
                      SLOT(slotExecAttach()), actionCollection(),
                      "exec_attach");
    (void)new KAction(i18n("&Arguments..."), 0, this,
                      SLOT(slotExecArgs()), actionCollection(),
                      "exec_arguments");
   
    // breakpoint menu
    (void)new KAction(i18n("Set/Clear &breakpoint"), "brkpt", Key_F9,
                      m_filesWindow, SLOT(slotBrkptSet()), actionCollection(),
                      "breakpoint_set");
    (void)new KAction(i18n("Set &temporary breakpoint"), SHIFT+Key_F9,
                      m_filesWindow, SLOT(slotBrkptSetTemp()), actionCollection(),
                      "breakpoint_set_temporary");
    (void)new KAction(i18n("&Enable/Disable breakpoint"), CTRL+Key_F9,
                      m_filesWindow, SLOT(slotBrkptEnable()), actionCollection(),
                      "breakpoint_enable");
   
    // only in popup menus
    (void)new KAction(i18n("Watch Expression"), 0, this,
                      SLOT(slotLocalsToWatch()), actionCollection(),
                      "watch_expression");
    (void)new KAction(i18n("Edit Value"), Key_F2, this,
		      SLOT(slotEditValue()), actionCollection(),
		      "edit_value");

    (void)new KActionMenu(i18n("&Window"), actionCollection(), "window");

    // all actions force an UI update
    QValueList<KAction*> actions = actionCollection()->actions();
    QValueList<KAction*>::Iterator it = actions.begin();
    for (; it != actions.end(); ++it) {
	connect(*it, SIGNAL(activated()), this, SLOT(updateUI()));
    }

    createGUI("kdbgui.rc");
}

void DebuggerMainWnd::initToolbar()
{
    KToolBar* toolbar = toolBar("mainToolBar");
    toolbar->setBarPos(KToolBar::Top);
    //moveToolBar(toolbar);
    
    toolbar->insertAnimatedWidget(ID_STATUS_BUSY,
	actionCollection()->action("exec_break"), SLOT(activate()),
	"pulse", -1);
    toolbar->alignItemRight(ID_STATUS_BUSY, true);
    m_animRunning = false;

    KStatusBar* statusbar = statusBar();
    statusbar->insertItem(m_statusActive, ID_STATUS_ACTIVE);
    m_lastActiveStatusText = m_statusActive;
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
    clearWFlags(WDestructiveClose);

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
	debugProgram(execName, "");
    }
}

const char WindowGroup[] = "Windows";
const char RecentExecutables[] = "RecentExecutables";

void DebuggerMainWnd::saveSettings(KConfig* config)
{
    KConfigGroupSaver g(config, WindowGroup);

    writeDockConfig(config);
    fixDockConfig(config, false);	// downgrade

    m_recentExecAction->saveEntries(config, RecentExecutables);

    DebuggerMainWndBase::saveSettings(config);
}

void DebuggerMainWnd::restoreSettings(KConfig* config)
{
    KConfigGroupSaver g(config, WindowGroup);

    fixDockConfig(config, true);	// upgrade
    readDockConfig(config);

    // Workaround bug #87787: KDockManager stores the titles of the KDockWidgets
    // in the config files, although they are localized:
    // If the user changes the language, the titles remain in the previous language.
    struct { QString text; QWidget* w; } dw[] = {
	{ i18n("Stack"), m_btWindow },
	{ i18n("Locals"), m_localVariables },
	{ i18n("Watches"), m_watches },
	{ i18n("Registers"), m_registers },
	{ i18n("Breakpoints"), m_bpTable },
	{ i18n("Threads"), m_threads },
	{ i18n("Output"), m_ttyWindow },
	{ i18n("Memory"), m_memoryWindow }
    };
    for (int i = 0; i < sizeof(dw)/sizeof(dw[0]); i++)
    {
	KDockWidget* w = dockParent(dw[i].w);
	w->setTabPageLabel(dw[i].text);
	// this actually changes the captions in the tabs:
	QEvent ev(QEvent::CaptionChange);
	w->event(&ev);
    }

    m_recentExecAction->loadEntries(config, RecentExecutables);

    DebuggerMainWndBase::restoreSettings(config);

    emit setTabWidth(m_tabWidth);
}

void DebuggerMainWnd::updateUI()
{
    KToggleAction* viewFind =
	static_cast<KToggleAction*>(actionCollection()->action("view_find"));
    viewFind->setChecked(m_filesWindow->m_findDlg.isVisible());
    viewFind->setEnabled(m_filesWindow->hasWindows());
    actionCollection()->action("breakpoint_set")->setEnabled(m_debugger->canChangeBreakpoints());
    actionCollection()->action("breakpoint_set_temporary")->setEnabled(m_debugger->canChangeBreakpoints());
    actionCollection()->action("breakpoint_enable")->setEnabled(m_debugger->canChangeBreakpoints());
    dockUpdateHelper("view_breakpoints", m_bpTable);
    dockUpdateHelper("view_stack", m_btWindow);
    dockUpdateHelper("view_locals", m_localVariables);
    dockUpdateHelper("view_watched_expressions", m_watches);
    dockUpdateHelper("view_registers", m_registers);
    dockUpdateHelper("view_threads", m_threads);
    dockUpdateHelper("view_memory", m_memoryWindow);
    dockUpdateHelper("view_output", m_ttyWindow);

    // AB: maybe in mainwndbase.cpp?
    actionCollection()->action("file_executable")->setEnabled(m_debugger->isIdle());
    actionCollection()->action("settings_program")->setEnabled(m_debugger->haveExecutable());
    actionCollection()->action("file_core_dump")->setEnabled(m_debugger->canStart());
    actionCollection()->action("exec_step_into")->setEnabled(m_debugger->canSingleStep());
    actionCollection()->action("exec_step_into_by_insn")->setEnabled(m_debugger->canSingleStep());
    actionCollection()->action("exec_step_over")->setEnabled(m_debugger->canSingleStep());
    actionCollection()->action("exec_step_over_by_insn")->setEnabled(m_debugger->canSingleStep());
    actionCollection()->action("exec_step_out")->setEnabled(m_debugger->canSingleStep());
    actionCollection()->action("exec_run_to_cursor")->setEnabled(m_debugger->canSingleStep());
    actionCollection()->action("exec_movepc")->setEnabled(m_debugger->canSingleStep());
    actionCollection()->action("exec_restart")->setEnabled(m_debugger->canSingleStep());
    actionCollection()->action("exec_attach")->setEnabled(m_debugger->isReady());
    actionCollection()->action("exec_run")->setEnabled(m_debugger->canStart() || m_debugger->canSingleStep());
    actionCollection()->action("exec_kill")->setEnabled(m_debugger->haveExecutable() && m_debugger->isProgramActive());
    actionCollection()->action("exec_break")->setEnabled(m_debugger->isProgramRunning());
    actionCollection()->action("exec_arguments")->setEnabled(m_debugger->haveExecutable());
    actionCollection()->action("edit_value")->setEnabled(m_debugger->canSingleStep());

    // animation
    KAnimWidget* w = toolBar("mainToolBar")->animatedWidget(ID_STATUS_BUSY);
    if (m_debugger->isIdle()) {
	if (m_animRunning) {
	    w->stop();
	    m_animRunning = false;
	}
    } else {
	if (!m_animRunning) {
	    w->start();
	    m_animRunning = true;
	}
    }

    // update statusbar
    QString newStatus;
    if (m_debugger->isProgramActive())
	newStatus = m_statusActive;
    if (newStatus != m_lastActiveStatusText) {
	statusBar()->changeItem(newStatus, ID_STATUS_ACTIVE);
	m_lastActiveStatusText = newStatus;
    }
    // line number is updated in slotLineChanged
}

void DebuggerMainWnd::dockUpdateHelper(QString action, QWidget* w)
{
    KToggleAction* item =
	static_cast<KToggleAction*>(actionCollection()->action(action));
    bool canChange = canChangeDockVisibility(w);
    item->setEnabled(canChange);
    item->setChecked(canChange && isDockVisible(w));
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
    QString caption;

    if (m_debugger->haveExecutable()) {
	// basename part of executable
	QString executable = m_debugger->executable();
	const char* execBase = executable.data();
	int lastSlash = executable.findRev('/');
	if (lastSlash >= 0)
	    execBase += lastSlash + 1;
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

KDockWidget* DebuggerMainWnd::dockParent(QWidget* w)
{
    while ((w = w->parentWidget()) != 0) {
	if (w->isA("KDockWidget"))
	    return static_cast<KDockWidget*>(w);
    }
    return 0;
}

bool DebuggerMainWnd::isDockVisible(QWidget* w)
{
    KDockWidget* d = dockParent(w);
    return d != 0 && d->mayBeHide();
}

bool DebuggerMainWnd::canChangeDockVisibility(QWidget* w)
{
    KDockWidget* d = dockParent(w);
    return d != 0 && (d->mayBeHide() || d->mayBeShow());
}

// upgrades the entries from version 0.0.4 to 0.0.5 and back
void DebuggerMainWnd::fixDockConfig(KConfig* c, bool upgrade)
{
    static const char dockGroup[] = "dock_setting_default";
    if (!c->hasGroup(dockGroup))
	return;

    static const char oldVersion[] = "0.0.4";
    static const char newVersion[] = "0.0.5";
    const char* from = upgrade ? oldVersion : newVersion;
    const char* to   = upgrade ? newVersion : oldVersion;
    QMap<QString,QString> e = c->entryMap(dockGroup);
    if (e["Version"] != from)
	return;

    KConfigGroupSaver g(c, dockGroup);
    c->writeEntry("Version", to);
    TRACE(upgrade ? "upgrading dockconfig" : "downgrading dockconfig");

    // turn all orientation entries from 0 to 1 and from 1 to 0
    QMap<QString,QString>::Iterator i;
    for (i = e.begin(); i != e.end(); ++i)
    {
	if (i.key().right(12) == ":orientation") {
	    TRACE("upgrading " + i.key() + " old value: " + *i);
	    int orientation = c->readNumEntry(i.key(), -1);
	    if (orientation >= 0) {	// paranoia
		c->writeEntry(i.key(), 1 - orientation);
	    }
	}
    }
}

TTYWindow* DebuggerMainWnd::ttyWindow()
{
    return m_ttyWindow;
}

bool DebuggerMainWnd::debugProgram(const QString& exe, QCString lang)
{
    // check the file name
    QFileInfo fi(exe);

    bool success = fi.isFile();
    if (!success)
    {
	QString msg = i18n("`%1' is not a file or does not exist");
	KMessageBox::sorry(this, msg.arg(exe));
    }
    else
    {
	success = DebuggerMainWndBase::debugProgram(fi.absFilePath(), lang, this);
    }

    if (success)
    {
	m_recentExecAction->addURL(KURL(fi.absFilePath()));

	// keep the directory
	m_lastDirectory = fi.dirPath(true);
	m_filesWindow->setExtraDirectory(m_lastDirectory);
    }
    else
    {
	m_recentExecAction->removeURL(KURL(fi.absFilePath()));
    }

    return true;
}

void DebuggerMainWnd::slotNewStatusMsg()
{
    newStatusMsg(statusBar());
}

void DebuggerMainWnd::slotFileGlobalSettings()
{
    int oldTabWidth = m_tabWidth;

    doGlobalOptions(this);

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

void DebuggerMainWnd::slotProgramStopped()
{
    // when the program stopped, move the window to the foreground
    if (m_popForeground) {
	// unfortunately, this requires quite some force to work :-(
	KWin::raiseWindow(winId());
	KWin::forceActiveWindow(winId());
    }
    m_backTimer.stop();
}

void DebuggerMainWnd::intoBackground()
{
    if (m_popForeground) {
        m_backTimer.start(m_backTimeout, true);	/* single-shot */
    }
}

void DebuggerMainWnd::slotBackTimer()
{
    lower();
}

void DebuggerMainWnd::slotRecentExec(const KURL& url)
{
    QString exe = url.path();
    debugProgram(exe, "");
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

/*
 * Pop up the context menu in the locals window
 */
void DebuggerMainWnd::slotLocalsPopup(QListViewItem*, const QPoint& pt)
{
    QPopupMenu* popup =
	static_cast<QPopupMenu*>(factory()->container("popup_locals", this));
    if (popup == 0) {
        return;
    }
    if (popup->isVisible()) {
	popup->hide();
    } else {
	popup->popup(pt);
    }
}

/*
 * Copies the currently selected item to the watch window.
 */
void DebuggerMainWnd::slotLocalsToWatch()
{
    VarTree* item = m_localVariables->selectedItem();

    if (item != 0 && m_debugger != 0) {
	QString text = item->computeExpr();
	m_debugger->addWatch(text);
    }
}

/*
 * Starts editing a value in a value display
 */
void DebuggerMainWnd::slotEditValue()
{
    // does one of the value trees have the focus
    QWidget* f = kapp->focusWidget();
    ExprWnd* wnd;
    if (f == m_localVariables) {
	wnd = m_localVariables;
    } else if (f == m_watches->watchVariables()) {
	wnd = m_watches->watchVariables();
    } else {
	return;
    }

    if (m_localVariables->isEditing() ||
	m_watches->watchVariables()->isEditing())
    {
	return;				/* don't edit twice */
    }
    
    VarTree* expr = wnd->currentItem();
    if (expr != 0 && m_debugger != 0 && m_debugger->canSingleStep())
    {
	TRACE("edit value");
	// determine the text to edit
	QString text = m_debugger->driver()->editableValue(expr);
	wnd->editValue(expr, text);
    }
}

// Pop up the context menu of the files window (for loaded files)
void DebuggerMainWnd::slotFileWndMenu(const QPoint& pos)
{
    QPopupMenu* popup =
	static_cast<QPopupMenu*>(factory()->container("popup_files", this));
    if (popup == 0) {
        return;
    }
    if (popup->isVisible()) {
	popup->hide();
    } else {
	// pos is still in widget coordinates of the sender
	const QWidget* w = static_cast<const QWidget*>(sender());
	popup->popup(w->mapToGlobal(pos));
    }
}

// Pop up the context menu of the files window (while no file is loaded)
void DebuggerMainWnd::slotFileWndEmptyMenu(const QPoint& pos)
{
    QPopupMenu* popup =
	static_cast<QPopupMenu*>(factory()->container("popup_files_empty", this));
    if (popup == 0) {
        return;
    }
    if (popup->isVisible()) {
        popup->hide();
    } else {
	// pos is still in widget coordinates of the sender
	const QWidget* w = static_cast<const QWidget*>(sender());
	popup->popup(w->mapToGlobal(pos));
    }
}

void DebuggerMainWnd::slotFileOpen()
{
    // start browsing in the active file's directory
    // fall back to last used directory (executable)
    QString dir = m_lastDirectory;
    QString fileName = m_filesWindow->activeFileName();
    if (!fileName.isEmpty()) {
	QFileInfo fi(fileName);
	dir = fi.dirPath();
    }

    fileName = myGetFileName(i18n("Open"),
			     dir,
			     makeSourceFilter(), this);

    if (!fileName.isEmpty())
    {
	QFileInfo fi(fileName);
	m_lastDirectory = fi.dirPath();
	m_filesWindow->setExtraDirectory(m_lastDirectory);
	m_filesWindow->activateFile(fileName);
    }
}

void DebuggerMainWnd::slotFileQuit()
{
    if (m_debugger != 0) {
	m_debugger->shutdown();
    }
    kapp->quit();
}

void DebuggerMainWnd::slotFileExe()
{
    if (m_debugger->isIdle())
    {
	// open a new executable
	QString executable = myGetFileName(i18n("Select the executable to debug"),
					   m_lastDirectory, 0, this);
	if (executable.isEmpty())
	    return;

	debugProgram(executable, "");
    }
}

void DebuggerMainWnd::slotFileCore()
{
    if (m_debugger->canStart())
    {
	QString corefile = myGetFileName(i18n("Select core dump"),
					 m_lastDirectory, 0, this);
	if (!corefile.isEmpty()) {
	    m_debugger->useCoreFile(corefile, false);
	}
    }
}

void DebuggerMainWnd::slotFileProgSettings()
{
    if (m_debugger != 0) {
	m_debugger->programSettings(this);
    }
}

void DebuggerMainWnd::slotViewToolbar()
{
    if (toolBar()->isVisible())
	toolBar()->hide();
    else
	toolBar()->show();
}

void DebuggerMainWnd::slotViewStatusbar()
{
    if (statusBar()->isVisible())
	statusBar()->hide();
    else
	statusBar()->show();
}

void DebuggerMainWnd::slotExecUntil()
{
    if (m_debugger != 0)
    {
	QString file;
	int lineNo;
	if (m_filesWindow->activeLine(file, lineNo))
	    m_debugger->runUntil(file, lineNo);
    }
}

void DebuggerMainWnd::slotExecAttach()
{
#ifdef PS_COMMAND
    ProcAttachPS dlg(this);
    // seed filter with executable name
    QFileInfo fi = m_debugger->executable();
    dlg.filterEdit->setText(fi.fileName());
#else
    ProcAttach dlg(this);
    dlg.setText(m_debugger->attachedPid());
#endif
    if (dlg.exec()) {
	m_debugger->attachProgram(dlg.text());
    }
}

void DebuggerMainWnd::slotExecArgs()
{
    if (m_debugger != 0) {
	m_debugger->programArgs(this);
    }
}

void DebuggerMainWnd::slotConfigureKeys()
{
    KKeyDialog::configure(actionCollection(), this);
}

#include "dbgmainwnd.moc"
