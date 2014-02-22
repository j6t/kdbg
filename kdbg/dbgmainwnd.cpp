/*
 * Copyright Johannes Sixt
 * This file is licensed under the GNU General Public License Version 2.
 * See the file COPYING in the toplevel directory of the source directory.
 */

#include <kapplication.h>
#include <klocale.h>			/* i18n */
#include <kmessagebox.h>
#include <kconfig.h>
#include <kstatusbar.h>
#include <kicon.h>
#include <kstandardaction.h>
#include <kstandardshortcut.h>
#include <kaction.h>
#include <kactioncollection.h>
#include <krecentfilesaction.h>
#include <ktoggleaction.h>
#include <kfiledialog.h>
#include <kshortcutsdialog.h>
#include <kanimatedbutton.h>
#include <kwindowsystem.h>
#include <ktoolbar.h>
#include <kurl.h>
#include <kxmlguifactory.h>
#include <KPageDialog>
#include <QListWidget>
#include <QFile>
#include <QFileInfo>
#include <QList>
#include <QDockWidget>
#include <QProcess>
#include "dbgmainwnd.h"
#include "debugger.h"
#include "commandids.h"
#include "winstack.h"
#include "brkpt.h"
#include "threadlist.h"
#include "memwindow.h"
#include "ttywnd.h"
#include "watchwindow.h"
#include "procattach.h"
#include "prefdebugger.h"
#include "prefmisc.h"
#include "gdbdriver.h"
#include "xsldbgdriver.h"
#include "mydebug.h"
#include <sys/stat.h>			/* mknod(2) */
#include <unistd.h>			/* getpid */


static const char defaultTermCmdStr[] = "xterm -name kdbgio -title %T -e sh -c %C";
static const char defaultSourceFilter[] = "*.c *.cc *.cpp *.c++ *.C *.CC";
static const char defaultHeaderFilter[] = "*.h *.hh *.hpp *.h++";

DebuggerMainWnd::DebuggerMainWnd() :
	KXmlGuiWindow(),
	m_debugger(0),
#ifdef GDB_TRANSCRIPT
	m_transcriptFile(GDB_TRANSCRIPT),
#endif
	m_outputTermCmdStr(defaultTermCmdStr),
	m_outputTermProc(new QProcess),
	m_ttyLevel(-1),			/* no tty yet */
	m_popForeground(false),
	m_backTimeout(1000),
	m_tabWidth(0),
	m_sourceFilter(defaultSourceFilter),
	m_headerFilter(defaultHeaderFilter),
	m_statusActive(i18n("active"))
{
    setDockNestingEnabled(true);

    m_filesWindow = new WinStack(this);
    setCentralWidget(m_filesWindow);

    QDockWidget* dw1 = createDockWidget("Stack", i18n("Stack"));
    m_btWindow = new QListWidget(dw1);
    dw1->setWidget(m_btWindow);
    QDockWidget* dw2 = createDockWidget("Locals", i18n("Locals"));
    m_localVariables = new ExprWnd(dw2, i18n("Variable"));
    dw2->setWidget(m_localVariables);
    QDockWidget* dw3 = createDockWidget("Watches", i18n("Watches"));
    m_watches = new WatchWindow(dw3);
    dw3->setWidget(m_watches);
    QDockWidget* dw4 = createDockWidget("Registers", i18n("Registers"));
    m_registers = new RegisterView(dw4);
    dw4->setWidget(m_registers);
    QDockWidget* dw5 = createDockWidget("Breakpoints", i18n("Breakpoints"));
    m_bpTable = new BreakpointTable(dw5);
    dw5->setWidget(m_bpTable);
    QDockWidget* dw6 = createDockWidget("Output", i18n("Output"));
    m_ttyWindow = new TTYWindow(dw6);
    dw6->setWidget(m_ttyWindow);
    QDockWidget* dw7 = createDockWidget("Threads", i18n("Threads"));
    m_threads = new ThreadList(dw7);
    dw7->setWidget(m_threads);
    QDockWidget* dw8 = createDockWidget("Memory", i18n("Memory"));
    m_memoryWindow = new MemoryWindow(dw8);
    dw8->setWidget(m_memoryWindow);

    m_debugger = new KDebugger(this, m_localVariables, m_watches->watchVariables(), m_btWindow);

    connect(m_debugger, SIGNAL(updateStatusMessage()), SLOT(slotNewStatusMsg()));
    connect(m_debugger, SIGNAL(updateUI()), SLOT(updateUI()));
    connect(m_debugger, SIGNAL(breakpointsChanged()), SLOT(updateLineItems()));
    connect(m_debugger, SIGNAL(debuggerStarting()), SLOT(slotDebuggerStarting()));
    m_bpTable->setDebugger(m_debugger);
    m_memoryWindow->setDebugger(m_debugger);

    setStandardToolBarMenuEnabled(true);
    initKAction();
    initStatusBar();

    connect(m_watches, SIGNAL(addWatch()), SLOT(slotAddWatch()));
    connect(m_watches, SIGNAL(deleteWatch()), m_debugger, SLOT(slotDeleteWatch()));
    connect(m_watches, SIGNAL(textDropped(const QString&)), SLOT(slotAddWatch(const QString&)));

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
    connect(m_debugger, SIGNAL(disassembled(const QString&,int,const std::list<DisassembledCode>&)),
	    m_filesWindow, SLOT(slotDisassembled(const QString&,int,const std::list<DisassembledCode>&)));
    connect(m_filesWindow, SIGNAL(moveProgramCounter(const QString&,int,const DbgAddr&)),
	    m_debugger, SLOT(setProgramCounter(const QString&,int,const DbgAddr&)));
    // program stopped
    connect(m_debugger, SIGNAL(programStopped()), SLOT(slotProgramStopped()));
    connect(&m_backTimer, SIGNAL(timeout()), SLOT(slotBackTimer()));
    // tab width
    connect(this, SIGNAL(setTabWidth(int)), m_filesWindow, SIGNAL(setTabWidth(int)));

    // connect breakpoint table
    connect(m_bpTable, SIGNAL(activateFileLine(const QString&,int,const DbgAddr&)),
	    m_filesWindow, SLOT(activate(const QString&,int,const DbgAddr&)));
    connect(m_debugger, SIGNAL(updateUI()), m_bpTable, SLOT(updateUI()));
    connect(m_debugger, SIGNAL(breakpointsChanged()), m_bpTable, SLOT(updateBreakList()));
    connect(m_debugger, SIGNAL(breakpointsChanged()), m_bpTable, SLOT(updateUI()));

    connect(m_debugger, SIGNAL(registersChanged(const std::list<RegisterInfo>&)),
	    m_registers, SLOT(updateRegisters(const std::list<RegisterInfo>&)));

    connect(m_debugger, SIGNAL(memoryDumpChanged(const QString&, const std::list<MemoryDump>&)),
	    m_memoryWindow, SLOT(slotNewMemoryDump(const QString&, const std::list<MemoryDump>&)));
    connect(m_debugger, SIGNAL(saveProgramSpecific(KConfigBase*)),
	    m_memoryWindow, SLOT(saveProgramSpecific(KConfigBase*)));
    connect(m_debugger, SIGNAL(restoreProgramSpecific(KConfigBase*)),
	    m_memoryWindow, SLOT(restoreProgramSpecific(KConfigBase*)));

    // thread window
    connect(m_debugger, SIGNAL(threadsChanged(const std::list<ThreadInfo>&)),
	    m_threads, SLOT(updateThreads(const std::list<ThreadInfo>&)));
    connect(m_threads, SIGNAL(setThread(int)),
	    m_debugger, SLOT(setThread(int)));

    // popup menu of the local variables window
    m_localVariables->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_localVariables, SIGNAL(customContextMenuRequested(const QPoint&)),
	    this, SLOT(slotLocalsPopup(const QPoint&)));

    makeDefaultLayout();
    setupGUI(KXmlGuiWindow::Default, "kdbgui.rc");
    restoreSettings(KGlobal::config());

    // The animation button is not part of the restored window state.
    // We must create it after the toolbar was loaded.
    initAnimation();

    updateUI();
    m_bpTable->updateUI();
}

DebuggerMainWnd::~DebuggerMainWnd()
{
    saveSettings(KGlobal::config());
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

    delete m_outputTermProc;
}

QDockWidget* DebuggerMainWnd::createDockWidget(const char* name, const QString& title)
{
    QDockWidget* w = new QDockWidget(title, this);
    w->setObjectName(name);
    // view menu changes when docking state changes
    connect(w, SIGNAL(visibilityChanged(bool)), SLOT(updateUI()));
    return w;
}

QAction* DebuggerMainWnd::createAction(const QString& text, const char* icon,
			int shortcut, const QObject* receiver,
			const char* slot, const char* name)
{
    KAction* a = actionCollection()->addAction(name);
    a->setText(text);
    a->setIcon(KIcon(icon));
    if (shortcut)
	a->setShortcut(KShortcut(shortcut));
    connect(a, SIGNAL(triggered()), receiver, slot);
    return a;
}

QAction* DebuggerMainWnd::createAction(const QString& text,
			int shortcut, const QObject* receiver,
			const char* slot, const char* name)
{
    KAction* a = actionCollection()->addAction(name);
    a->setText(text);
    if (shortcut)
	a->setShortcut(KShortcut(shortcut));
    connect(a, SIGNAL(triggered()), receiver, slot);
    return a;
}


void DebuggerMainWnd::initKAction()
{
    // file menu
    KAction* open = KStandardAction::open(this, SLOT(slotFileOpen()),
                      actionCollection());
    open->setText(i18n("&Open Source..."));
    m_closeAction = KStandardAction::close(m_filesWindow, SLOT(slotClose()), actionCollection());
    m_reloadAction = createAction(i18n("&Reload Source"), "view-refresh", 0,
			m_filesWindow, SLOT(slotFileReload()), "file_reload");
    m_fileExecAction = createAction(i18n("&Executable..."),
			"document-open-executable", 0,
			this, SLOT(slotFileExe()), "file_executable");
    m_recentExecAction = KStandardAction::openRecent(this, SLOT(slotRecentExec(const KUrl&)),
		      actionCollection());
    m_recentExecAction->setObjectName("file_executable_recent");
    m_recentExecAction->setText(i18n("Recent E&xecutables"));
    m_coreDumpAction = createAction(i18n("&Core dump..."), 0,
			this, SLOT(slotFileCore()), "file_core_dump");
    KStandardAction::quit(kapp, SLOT(closeAllWindows()), actionCollection());

    // settings menu
    m_settingsAction = createAction(i18n("This &Program..."), 0,
			this, SLOT(slotFileProgSettings()), "settings_program");
    createAction(i18n("&Global Options..."), 0,
			this, SLOT(slotFileGlobalSettings()), "settings_global");
    KStandardAction::keyBindings(this, SLOT(slotConfigureKeys()), actionCollection());
    KStandardAction::showStatusbar(this, SLOT(slotViewStatusbar()), actionCollection());

    // view menu
    m_findAction = KStandardAction::find(m_filesWindow, SLOT(slotViewFind()), actionCollection());
    KStandardAction::findNext(m_filesWindow, SLOT(slotFindForward()), actionCollection());
    KStandardAction::findPrev(m_filesWindow, SLOT(slotFindBackward()), actionCollection());

    i18n("Source &code");
    struct { QString text; QWidget* w; QString id; QAction** act; } dw[] = {
	{ i18n("Stac&k"), m_btWindow, "view_stack", &m_btWindowAction },
	{ i18n("&Locals"), m_localVariables, "view_locals", &m_localVariablesAction },
	{ i18n("&Watched expressions"), m_watches, "view_watched_expressions", &m_watchesAction },
	{ i18n("&Registers"), m_registers, "view_registers", &m_registersAction },
	{ i18n("&Breakpoints"), m_bpTable, "view_breakpoints", &m_bpTableAction },
	{ i18n("T&hreads"), m_threads, "view_threads", &m_threadsAction },
	{ i18n("&Output"), m_ttyWindow, "view_output", &m_ttyWindowAction },
	{ i18n("&Memory"), m_memoryWindow, "view_memory", &m_memoryWindowAction }
    };
    for (unsigned i = 0; i < sizeof(dw)/sizeof(dw[0]); i++) {
	QDockWidget* d = dockParent(dw[i].w);
	*dw[i].act = new KToggleAction(dw[i].text, actionCollection());
	actionCollection()->addAction(dw[i].id, *dw[i].act);
	connect(*dw[i].act, SIGNAL(triggered()), d, SLOT(show()));
    }

    // execution menu
    m_runAction = createAction(i18n("&Run"),
			"debug-run", Qt::Key_F5,
			m_debugger, SLOT(programRun()), "exec_run");
    connect(m_runAction, SIGNAL(activated()), this, SLOT(intoBackground()));
    m_stepIntoAction = createAction(i18n("Step &into"),
			"debug-step-into", Qt::Key_F8,
			m_debugger, SLOT(programStep()), "exec_step_into");
    connect(m_stepIntoAction, SIGNAL(activated()), this, SLOT(intoBackground()));
    m_stepOverAction = createAction(i18n("Step &over"),
			"debug-step-over", Qt::Key_F10,
			m_debugger, SLOT(programNext()), "exec_step_over");
    connect(m_stepOverAction, SIGNAL(activated()), this, SLOT(intoBackground()));
    m_stepOutAction = createAction(i18n("Step o&ut"),
			"debug-step-out", Qt::Key_F6,
			m_debugger, SLOT(programFinish()), "exec_step_out");
    connect(m_stepOutAction, SIGNAL(activated()), this, SLOT(intoBackground()));
    m_toCursorAction = createAction(i18n("Run to &cursor"),
			"debug-execute-to-cursor", Qt::Key_F7,
			this, SLOT(slotExecUntil()), "exec_run_to_cursor");
    connect(m_toCursorAction, SIGNAL(activated()), this, SLOT(intoBackground()));
    m_stepIntoIAction = createAction(i18n("Step i&nto by instruction"),
			"debug-step-into-instruction", Qt::SHIFT+Qt::Key_F8,
			m_debugger, SLOT(programStepi()), "exec_step_into_by_insn");
    connect(m_stepIntoIAction, SIGNAL(activated()), this, SLOT(intoBackground()));
    m_stepOverIAction = createAction(i18n("Step o&ver by instruction"),
			"debug-step-instruction", Qt::SHIFT+Qt::Key_F10,
			m_debugger, SLOT(programNexti()), "exec_step_over_by_insn");
    connect(m_stepOverIAction, SIGNAL(activated()), this, SLOT(intoBackground()));
    m_execMovePCAction = createAction(i18n("&Program counter to current line"),
			"debug-run-cursor", 0,
			m_filesWindow, SLOT(slotMoveProgramCounter()), "exec_movepc");
    m_breakAction = createAction(i18n("&Break"), 0,
			m_debugger, SLOT(programBreak()), "exec_break");
    m_killAction = createAction(i18n("&Kill"), 0,
			m_debugger, SLOT(programKill()), "exec_kill");
    m_restartAction = createAction(i18n("Re&start"), 0,
			m_debugger, SLOT(programRunAgain()), "exec_restart");
    m_attachAction = createAction(i18n("A&ttach..."), 0,
			this, SLOT(slotExecAttach()), "exec_attach");
    m_argumentsAction = createAction(i18n("&Arguments..."), 0,
			this, SLOT(slotExecArgs()), "exec_arguments");

    // breakpoint menu
    m_bpSetAction = createAction(i18n("Set/Clear &breakpoint"), "brkpt", Qt::Key_F9,
			m_filesWindow, SLOT(slotBrkptSet()), "breakpoint_set");
    m_bpSetTempAction = createAction(i18n("Set &temporary breakpoint"), Qt::SHIFT+Qt::Key_F9,
			m_filesWindow, SLOT(slotBrkptSetTemp()), "breakpoint_set_temporary");
    m_bpEnableAction = createAction(i18n("&Enable/Disable breakpoint"), Qt::CTRL+Qt::Key_F9,
			m_filesWindow, SLOT(slotBrkptEnable()), "breakpoint_enable");

    // only in popup menus
    createAction(i18n("Watch Expression"), 0,
			this, SLOT(slotLocalsToWatch()), "watch_expression");
    m_editValueAction = createAction(i18n("Edit Value"), Qt::Key_F2,
			this, SLOT(slotEditValue()), "edit_value");

    // all actions force an UI update
    QList<QAction*> actions = actionCollection()->actions();
    foreach(QAction* action, actions) {
	connect(action, SIGNAL(activated()), this, SLOT(updateUI()));
    }
}

void DebuggerMainWnd::initAnimation()
{
    KToolBar* toolbar = toolBar("mainToolBar");
    m_animation = new KAnimatedButton(toolbar);
    toolbar->addWidget(m_animation);
    m_animation->setIcons("pulse");
    connect(m_animation, SIGNAL(clicked(bool)), m_debugger, SLOT(programBreak()));
    m_animRunning = false;
}

void DebuggerMainWnd::initStatusBar()
{
    KStatusBar* statusbar = statusBar();
    statusbar->insertItem(m_statusActive, ID_STATUS_ACTIVE);
    m_lastActiveStatusText = m_statusActive;
    statusbar->insertItem("", ID_STATUS_MSG);	/* message pane */

    // reserve some translations
    i18n("Restart");
    i18n("Core dump");
}

bool DebuggerMainWnd::queryClose()
{
    if (m_debugger != 0) {
	m_debugger->shutdown();
    }
    return true;
}


// instance properties
void DebuggerMainWnd::saveProperties(KConfigGroup& cg)
{
    // session management
    QString executable = "";
    if (m_debugger != 0) {
	executable = m_debugger->executable();
    }
    cg.writeEntry("executable", executable);
}

void DebuggerMainWnd::readProperties(const KConfigGroup& cg)
{
    // session management
    QString execName = cg.readEntry("executable");

    TRACE("readProperties: executable=" + execName);
    if (!execName.isEmpty()) {
	debugProgram(execName, "");
    }
}

static const char RecentExecutables[] = "RecentExecutables";
static const char LastSession[] = "LastSession";
static const char OutputWindowGroup[] = "OutputWindow";
static const char TermCmdStr[] = "TermCmdStr";
static const char KeepScript[] = "KeepScript";
static const char DebuggerGroup[] = "Debugger";
static const char DebuggerCmdStr[] = "DebuggerCmdStr";
static const char PreferencesGroup[] = "Preferences";
static const char PopForeground[] = "PopForeground";
static const char BackTimeout[] = "BackTimeout";
static const char TabWidth[] = "TabWidth";
static const char SourceFileFilter[] = "SourceFileFilter";
static const char HeaderFileFilter[] = "HeaderFileFilter";

void DebuggerMainWnd::saveSettings(KSharedConfigPtr config)
{
    m_recentExecAction->saveEntries(config->group(RecentExecutables));

    KConfigGroup lg = config->group(LastSession);
    lg.writeEntry("Width0Locals", m_localVariables->columnWidth(0));
    lg.writeEntry("Width0Watches", m_watches->columnWidth(0));

    if (m_debugger != 0) {
	m_debugger->saveSettings(config.data());
    }

    config->group(OutputWindowGroup).writeEntry(TermCmdStr, m_outputTermCmdStr);
    config->group(DebuggerGroup).writeEntry(DebuggerCmdStr, m_debuggerCmdStr);

    KConfigGroup pg(config->group(PreferencesGroup));
    pg.writeEntry(PopForeground, m_popForeground);
    pg.writeEntry(BackTimeout, m_backTimeout);
    pg.writeEntry(TabWidth, m_tabWidth);
    pg.writeEntry(SourceFileFilter, m_sourceFilter);
    pg.writeEntry(HeaderFileFilter, m_headerFilter);
}

void DebuggerMainWnd::restoreSettings(KSharedConfigPtr config)
{
    m_recentExecAction->loadEntries(config->group(RecentExecutables));

    KConfigGroup lg = config->group(LastSession);
    int w;
    w = lg.readEntry("Width0Locals", -1);
    if (w >= 0 && w < 30000)
	m_localVariables->setColumnWidth(0, w);
    w = lg.readEntry("Width0Watches", -1);
    if (w >= 0 && w < 30000)
	m_watches->setColumnWidth(0, w);

    if (m_debugger != 0) {
	m_debugger->restoreSettings(config.data());
    }

    KConfigGroup og(config->group(OutputWindowGroup));
    /*
     * For debugging and emergency purposes, let the config file override
     * the shell script that is used to keep the output window open. This
     * string must have EXACTLY 1 %s sequence in it.
     */
    setTerminalCmd(og.readEntry(TermCmdStr, defaultTermCmdStr));
    m_outputTermKeepScript = og.readEntry(KeepScript);

    setDebuggerCmdStr(config->group(DebuggerGroup).readEntry(DebuggerCmdStr));

    KConfigGroup pg(config->group(PreferencesGroup));
    m_popForeground = pg.readEntry(PopForeground, false);
    m_backTimeout = pg.readEntry(BackTimeout, 1000);
    m_tabWidth = pg.readEntry(TabWidth, 0);
    m_sourceFilter = pg.readEntry(SourceFileFilter, m_sourceFilter);
    m_headerFilter = pg.readEntry(HeaderFileFilter, m_headerFilter);

    emit setTabWidth(m_tabWidth);
}

void DebuggerMainWnd::updateUI()
{
    m_findAction->setChecked(m_filesWindow->m_findDlg.isVisible());
    m_findAction->setEnabled(m_filesWindow->hasWindows());
    m_bpSetAction->setEnabled(m_debugger->canChangeBreakpoints());
    m_bpSetTempAction->setEnabled(m_debugger->canChangeBreakpoints());
    m_bpEnableAction->setEnabled(m_debugger->canChangeBreakpoints());
    m_bpTableAction->setChecked(isDockVisible(m_bpTable));
    m_btWindowAction->setChecked(isDockVisible(m_btWindow));
    m_localVariablesAction->setChecked(isDockVisible(m_localVariables));
    m_watchesAction->setChecked(isDockVisible(m_watches));
    m_registersAction->setChecked(isDockVisible(m_registers));
    m_threadsAction->setChecked(isDockVisible(m_threads));
    m_memoryWindowAction->setChecked(isDockVisible(m_memoryWindow));
    m_ttyWindowAction->setChecked(isDockVisible(m_ttyWindow));

    m_fileExecAction->setEnabled(m_debugger->isIdle());
    m_settingsAction->setEnabled(m_debugger->haveExecutable());
    m_coreDumpAction->setEnabled(m_debugger->canStart());
    m_closeAction->setEnabled(m_filesWindow->hasWindows());
    m_reloadAction->setEnabled(m_filesWindow->hasWindows());
    m_stepIntoAction->setEnabled(m_debugger->canSingleStep());
    m_stepIntoIAction->setEnabled(m_debugger->canSingleStep());
    m_stepOverAction->setEnabled(m_debugger->canSingleStep());
    m_stepOverIAction->setEnabled(m_debugger->canSingleStep());
    m_stepOutAction->setEnabled(m_debugger->canSingleStep());
    m_toCursorAction->setEnabled(m_debugger->canSingleStep());
    m_execMovePCAction->setEnabled(m_debugger->canSingleStep());
    m_restartAction->setEnabled(m_debugger->canSingleStep());
    m_attachAction->setEnabled(m_debugger->isReady());
    m_runAction->setEnabled(m_debugger->canStart() || m_debugger->canSingleStep());
    m_killAction->setEnabled(m_debugger->haveExecutable() && m_debugger->isProgramActive());
    m_breakAction->setEnabled(m_debugger->isProgramRunning());
    m_argumentsAction->setEnabled(m_debugger->haveExecutable());
    m_editValueAction->setEnabled(m_debugger->canSingleStep());

    // animation
    if (m_debugger->isIdle()) {
	if (m_animRunning && m_animation) {
	    m_animation->stop();
	    m_animRunning = false;
	}
    } else {
	if (!m_animRunning && m_animation) {
	    m_animation->start();
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

void DebuggerMainWnd::slotAddWatch(const QString& text)
{
    if (m_debugger != 0) {
	m_debugger->addWatch(text);
    }
}

void DebuggerMainWnd::slotNewFileLoaded()
{
    // updates program counter in the new file
    if (m_debugger != 0)
	m_filesWindow->updateLineItems(m_debugger);
}

QDockWidget* DebuggerMainWnd::dockParent(QWidget* w)
{
    while ((w = w->parentWidget()) != 0) {
	if (QDockWidget* dock = qobject_cast<QDockWidget*>(w))
	    return dock;
    }
    return 0;
}

bool DebuggerMainWnd::isDockVisible(QWidget* w)
{
    QDockWidget* d = dockParent(w);
    return d != 0 && d->isVisible();
}

void DebuggerMainWnd::makeDefaultLayout()
{
    // +---------------+---------+
    // | Source        | Locals  |
    // |               |         |
    // |---------------+---------+
    // |Stack, Brkpts, | Watches |
    // |Output,...     |         |
    // +---------------+---------+

    addDockWidget(Qt::RightDockWidgetArea, dockParent(m_localVariables));
    addDockWidget(Qt::BottomDockWidgetArea, dockParent(m_memoryWindow));
    splitDockWidget(dockParent(m_memoryWindow), dockParent(m_threads), Qt::Horizontal);
    tabifyDockWidget(dockParent(m_memoryWindow), dockParent(m_registers));
    tabifyDockWidget(dockParent(m_registers), dockParent(m_bpTable));
    tabifyDockWidget(dockParent(m_bpTable), dockParent(m_ttyWindow));
    tabifyDockWidget(dockParent(m_ttyWindow), dockParent(m_btWindow));
    tabifyDockWidget(dockParent(m_threads), dockParent(m_watches));
    dockParent(m_localVariables)->setVisible(true);
    dockParent(m_ttyWindow)->setVisible(true);
    dockParent(m_watches)->setVisible(true);
    dockParent(m_btWindow)->setVisible(true);
    dockParent(m_bpTable)->setVisible(true);
}

bool DebuggerMainWnd::debugProgram(const QString& exe, const QString& lang)
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
	success = startDriver(fi.absoluteFilePath(), lang);
    }

    if (success)
    {
	m_recentExecAction->addUrl(KUrl(fi.absoluteFilePath()));

	// keep the directory
	m_lastDirectory = fi.absolutePath();
	m_filesWindow->setExtraDirectory(m_lastDirectory);

	// set caption to basename part of executable
	QString caption = fi.fileName();
	setCaption(caption);
    }
    else
    {
	m_recentExecAction->removeUrl(KUrl(fi.absoluteFilePath()));
    }

    return success;
}

static const char GeneralGroup[] = "General";

bool DebuggerMainWnd::startDriver(const QString& executable, QString lang)
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
	    KConfig c(configName, KConfig::SimpleConfig);

	    // Using "GDB" as default here is for backwards compatibility:
	    // The config file exists but doesn't have an entry,
	    // so it must have been created by an old version of KDbg
	    // that had only the GDB driver.
	    lang = c.group(GeneralGroup)
		    .readEntry(KDebugger::DriverNameEntry, "GDB");

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
	KMessageBox::sorry(this, msg.arg(lang));
	return false;
    }

    driver->setLogFileName(m_transcriptFile);

    bool success = m_debugger->debugProgram(executable, driver);

    if (!success)
    {
	delete driver;

	QString msg = i18n("Could not start the debugger process.\n"
			   "Please shut down KDbg and resolve the problem.");
	KMessageBox::sorry(this, msg);
    }

    return success;
}

// derive driver from language
DebuggerDriver* DebuggerMainWnd::driverFromLang(QString lang)
{
    // lang is needed in all lowercase
    lang = lang.toLower();

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
QString DebuggerMainWnd::driverNameFromFile(const QString& exe)
{
    /* Inprecise but simple test to see if file is in XSLT language */
    if (exe.right(4).toLower() == ".xsl")
	return "XSLT";

    return "GDB";
}

void DebuggerMainWnd::setCoreFile(const QString& corefile)
{
    assert(m_debugger != 0);
    m_debugger->useCoreFile(corefile, true);
}

void DebuggerMainWnd::setRemoteDevice(const QString& remoteDevice)
{
    if (m_debugger != 0) {
	m_debugger->setRemoteDevice(remoteDevice);
    }
}

void DebuggerMainWnd::overrideProgramArguments(const QString& args)
{
    assert(m_debugger != 0);
    m_debugger->overrideProgramArguments(args);
}

void DebuggerMainWnd::setTranscript(const QString& name)
{
    m_transcriptFile = name;
    if (m_debugger != 0 && m_debugger->driver() != 0)
	m_debugger->driver()->setLogFileName(m_transcriptFile);
}

void DebuggerMainWnd::setAttachPid(const QString& pid)
{
    assert(m_debugger != 0);
    m_debugger->setAttachPid(pid);
}

void DebuggerMainWnd::slotNewStatusMsg()
{
    QString msg = m_debugger->statusMessage();
    statusBar()->changeItem(msg, ID_STATUS_MSG);
}

void DebuggerMainWnd::slotFileGlobalSettings()
{
    int oldTabWidth = m_tabWidth;

    KPageDialog dlg(this);
    QString title = KGlobal::caption();
    title += i18n(": Global options");
    dlg.setCaption(title);

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

    dlg.addPage(&prefDebugger, i18n("Debugger"));
    dlg.addPage(&prefMisc, i18n("Miscellaneous"));
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

    if (m_tabWidth != oldTabWidth) {
	emit setTabWidth(m_tabWidth);
    }
}

void DebuggerMainWnd::setTerminalCmd(const QString& cmd)
{
    m_outputTermCmdStr = cmd;
    // revert to default if empty
    if (m_outputTermCmdStr.isEmpty()) {
	m_outputTermCmdStr = defaultTermCmdStr;
    }
}

void DebuggerMainWnd::setDebuggerCmdStr(const QString& cmd)
{
    m_debuggerCmdStr = cmd;
    // make empty if it is the default
    if (m_debuggerCmdStr == GdbDriver::defaultGdb()) {
	m_debuggerCmdStr = QString();
    }
}

void DebuggerMainWnd::slotDebuggerStarting()
{
    if (m_debugger == 0)		/* paranoia check */
	return;

    if (m_ttyLevel == m_debugger->ttyLevel())
	return;

    // shut down terminal emulations we will not need
    switch (m_ttyLevel) {
    case KDebugger::ttySimpleOutputOnly:
	m_ttyWindow->deactivate();
	break;
    case KDebugger::ttyFull:
	m_outputTermProc->kill();
	break;
    default: break;
    }

    m_ttyLevel = m_debugger->ttyLevel();

    QString ttyName;
    switch (m_ttyLevel) {
    case KDebugger::ttySimpleOutputOnly:
	ttyName = m_ttyWindow->activate();
	break;
    case KDebugger::ttyFull:
	// create an output window
	ttyName = createOutputWindow();
	TRACE(ttyName.isEmpty() ?
	      "createOuputWindow failed" : "successfully created output window");
	break;
    default: break;
    }

    m_debugger->setTerminal(ttyName);
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
    // create a name for a fifo
    QString fifoName;
    fifoName.sprintf("/tmp/kdbgttywin%05d", ::getpid());

    // create a fifo that will pass in the tty name
    QFile::remove(fifoName);		// remove remnants
#ifdef HAVE_MKFIFO
    if (::mkfifo(fifoName.toLocal8Bit(), S_IRUSR|S_IWUSR) < 0) {
	// failed
	TRACE("mkfifo " + fifoName + " failed");
	return QString();
    }
#else
    if (::mknod(fifoName.toLocal8Bit(), S_IFIFO | S_IRUSR|S_IWUSR, 0) < 0) {
	// failed
	TRACE("mknod " + fifoName + " failed");
	return QString();
    }
#endif

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

    QString title = KGlobal::caption();
    title += i18n(": Program output");

    // parse the command line specified in the preferences
    QStringList cmdParts = m_outputTermCmdStr.split(' ');

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
	    int pos = str.indexOf(substitute[j].seq);
	    if (pos >= 0) {
		str.replace(pos, 2, substitute[j].replace);
		break;		/* substitute only one sequence */
	    }
	}
    }

    QString tty, pgm = cmdParts.takeFirst();

    m_outputTermProc->start(pgm, cmdParts);
    if (m_outputTermProc->waitForStarted())
    {
	// read the ttyname from the fifo
	QFile f(fifoName);
	if (f.open(QIODevice::ReadOnly))
	{
	    QByteArray t = f.readAll();
	    tty = QString::fromLocal8Bit(t, t.size());
	    f.close();
	}
	f.remove();

	// remove whitespace
	tty = tty.trimmed();
	TRACE("tty=" + tty);
    }
    else
    {
	// error, could not start xterm
	TRACE("fork failed for fifo " + fifoName);
	QFile::remove(fifoName);
    }

    return tty;
}

void DebuggerMainWnd::slotProgramStopped()
{
    // when the program stopped, move the window to the foreground
    if (m_popForeground) {
	// unfortunately, this requires quite some force to work :-(
	KWindowSystem::raiseWindow(winId());
	KWindowSystem::forceActiveWindow(winId());
    }
    m_backTimer.stop();
}

void DebuggerMainWnd::intoBackground()
{
    if (m_popForeground) {
	m_backTimer.setSingleShot(true);
	m_backTimer.start(m_backTimeout);
    }
}

void DebuggerMainWnd::slotBackTimer()
{
    lower();
}

void DebuggerMainWnd::slotRecentExec(const KUrl& url)
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
void DebuggerMainWnd::slotLocalsPopup(const QPoint& pt)
{
    QMenu* popup = static_cast<QMenu*>(factory()->container("popup_locals", this));
    if (popup == 0) {
        return;
    }
    if (popup->isVisible()) {
	popup->hide();
    } else {
	popup->popup(m_localVariables->viewport()->mapToGlobal(pt));
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

    VarTree* expr = wnd->selectedItem();
    if (expr != 0 && m_debugger != 0 && m_debugger->canSingleStep())
    {
	TRACE("edit value");
	// determine the text to edit
	QString text = m_debugger->driver()->editableValue(expr);
	wnd->editValue(expr, text);
    }
}

// helper that gets a file name (it only differs in the caption of the dialog)
static QString myGetFileName(QString caption,
			     QString dir, QString filter,
	 		     QWidget* parent)
{
    QString filename;
    KFileDialog dlg(dir, filter, parent);

    dlg.setCaption(caption);

    if (dlg.exec() == QDialog::Accepted)
	filename = dlg.selectedFile();

    return filename;
}

void DebuggerMainWnd::slotFileOpen()
{
    // start browsing in the active file's directory
    // fall back to last used directory (executable)
    QString dir = m_lastDirectory;
    QString fileName = m_filesWindow->activeFileName();
    if (!fileName.isEmpty()) {
	QFileInfo fi(fileName);
	dir = fi.path();
    }

    fileName = myGetFileName(i18n("Open"),
			     dir,
			     makeSourceFilter(), this);

    if (!fileName.isEmpty())
    {
	QFileInfo fi(fileName);
	m_lastDirectory = fi.path();
	m_filesWindow->setExtraDirectory(m_lastDirectory);
	m_filesWindow->activateFile(fileName);
    }
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

void DebuggerMainWnd::slotViewStatusbar()
{
    if (statusBar()->isVisible())
	statusBar()->hide();
    else
	statusBar()->show();
    setSettingsDirty();
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
    dlg.setFilterText(fi.fileName());
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
    KShortcutsDialog::configure(actionCollection());
}

#include "dbgmainwnd.moc"
