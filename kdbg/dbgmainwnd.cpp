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
#include <qtabdialog.h>
#include <kwm.h>
#include "dbgmainwnd.h"
#include "debugger.h"
#include "prefdebugger.h"
#include "updateui.h"
#include "commandids.h"
#include "mydebug.h"


DebuggerMainWnd::DebuggerMainWnd(const char* name) :
	KTMainWindow(name),
	DebuggerMainWndBase(),
#if QT_VERSION < 200
	m_mainPanner(this, "main_pane", KNewPanner::Vertical, KNewPanner::Percent, 60),
	m_leftPanner(&m_mainPanner, "left_pane", KNewPanner::Horizontal, KNewPanner::Percent, 70),
	m_rightPanner(&m_mainPanner, "right_pane", KNewPanner::Horizontal, KNewPanner::Percent, 50),
#else
	m_mainPanner(QSplitter::Horizontal, this, "main_pane"),
	m_leftPanner(QSplitter::Vertical, &m_mainPanner, "left_pane"),
	m_rightPanner(QSplitter::Vertical, &m_mainPanner, "right_pane"),
#endif
	m_filesWindow(&m_leftPanner, "files"),
	m_btWindow(&m_leftPanner, "backtrace"),
	m_localVariables(&m_rightPanner, "locals"),
	m_watches(&m_rightPanner, "watches"),
	m_registers(0)
{
    initMenu();
    initToolbar();

    setupDebugger(&m_localVariables, m_watches.watchVariables(), &m_btWindow);
    m_bpTable.setDebugger(m_debugger);

#if QT_VERSION < 200
    m_mainPanner.activate(&m_leftPanner, &m_rightPanner);
    m_leftPanner.activate(&m_filesWindow, &m_btWindow);
    m_rightPanner.activate(&m_localVariables, &m_watches);
#endif
    setView(&m_mainPanner, true);	/* show frame */

    connect(&m_watches, SIGNAL(addWatch()), SLOT(slotAddWatch()));
    connect(&m_watches, SIGNAL(deleteWatch()), m_debugger, SLOT(slotDeleteWatch()));

    m_filesWindow.setWindowMenu(&m_menuWindow);
    connect(&m_filesWindow.m_findDlg, SIGNAL(closed()), SLOT(updateUI()));
    connect(&m_filesWindow, SIGNAL(newFileLoaded()),
	    SLOT(slotNewFileLoaded()));
    connect(&m_filesWindow, SIGNAL(toggleBreak(const QString&, int)),
	    this, SLOT(slotToggleBreak(const QString&,int)));
    connect(&m_filesWindow, SIGNAL(enadisBreak(const QString&, int)),
	    this, SLOT(slotEnaDisBreak(const QString&,int)));
    connect(m_debugger, SIGNAL(activateFileLine(const QString&,int)),
	    &m_filesWindow, SLOT(activate(const QString&,int)));
    connect(m_debugger, SIGNAL(executableUpdated()),
	    &m_filesWindow, SLOT(reloadAllFiles()));
    connect(m_debugger, SIGNAL(updatePC(const QString&,int,int)),
	    &m_filesWindow, SLOT(updatePC(const QString&,int,int)));

    // Establish communication when right clicked on file window.
    connect(&m_filesWindow.m_menuFloat, SIGNAL(activated(int)),
	    SLOT(menuCallback(int)));

    // Connection when right clicked on file window before any file is
    // loaded.
    connect(&m_filesWindow.m_menuFileFloat, SIGNAL(activated(int)),
	    SLOT(menuCallback(int)));

    // route unhandled menu items to winstack
    connect(this, SIGNAL(forwardMenuCallback(int)), &m_filesWindow, SLOT(menuCallback(int)));
    // file/line updates
    connect(&m_filesWindow, SIGNAL(fileChanged()), SLOT(slotFileChanged()));
    connect(&m_filesWindow, SIGNAL(lineChanged()), SLOT(slotLineChanged()));

    m_bpTable.setCaption(i18n("Breakpoints"));
    connect(&m_bpTable, SIGNAL(closed()), SLOT(updateUI()));
    connect(&m_bpTable, SIGNAL(activateFileLine(const QString&,int)),
	    &m_filesWindow, SLOT(activate(const QString&,int)));
    connect(m_debugger, SIGNAL(updateUI()), &m_bpTable, SLOT(updateUI()));
    connect(m_debugger, SIGNAL(breakpointsChanged()), &m_bpTable, SLOT(updateBreakList()));

    connect(m_debugger, SIGNAL(registersChanged(QList<RegisterInfo>&)),
	    &m_registers, SLOT(updateRegisters(QList<RegisterInfo>&)));
    m_registers.show();
    
    restoreSettings(kapp->getConfig());

    updateUI();
    slotFileChanged();
}

DebuggerMainWnd::~DebuggerMainWnd()
{
    saveSettings(kapp->getConfig());
    // must delete m_debugger early since it references our windows
    delete m_debugger;
    m_debugger = 0;
}

void DebuggerMainWnd::initMenu()
{
    m_menuFile.insertItem(i18n("&Open Source..."), ID_FILE_OPEN);
    m_menuFile.insertItem(i18n("&Reload Source"), ID_FILE_RELOAD);
    m_menuFile.insertSeparator();
    m_menuFile.insertItem(i18n("&Executable..."), ID_FILE_EXECUTABLE);
    m_menuFile.insertItem(i18n("&Core dump..."), ID_FILE_COREFILE);
    m_menuFile.insertSeparator();
    m_menuFile.insertItem(i18n("&Global Options..."), ID_FILE_GLOBAL_OPTIONS);
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
    m_menuProgram.insertItem(i18n("&Kill"), ID_PROGRAM_KILL);
    m_menuProgram.insertItem(i18n("Re&start"), ID_PROGRAM_RUN_AGAIN);
    m_menuProgram.insertItem(i18n("A&ttach..."), ID_PROGRAM_ATTACH);
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
  
    connect(&m_menuFile, SIGNAL(activated(int)), SLOT(menuCallback(int)));
    connect(&m_menuView, SIGNAL(activated(int)), SLOT(menuCallback(int)));
    connect(&m_menuProgram, SIGNAL(activated(int)), SLOT(menuCallback(int)));
    connect(&m_menuBrkpt, SIGNAL(activated(int)), SLOT(menuCallback(int)));
    connect(&m_menuWindow, SIGNAL(activated(int)), SLOT(menuCallback(int)));

    KMenuBar* menu = menuBar();
    menu->insertItem(i18n("&File"), &m_menuFile);
    menu->insertItem(i18n("&View"), &m_menuView);
    menu->insertItem(i18n("E&xecution"), &m_menuProgram);
    menu->insertItem(i18n("&Breakpoint"), &m_menuBrkpt);
    menu->insertItem(i18n("&Window"), &m_menuWindow);
    menu->insertSeparator();
    
    QString about = "KDbg " VERSION " - ";
    about += i18n("A Debugger\n"
		  "by Johannes Sixt <Johannes.Sixt@telecom.at>\n"
		  "with the help of many others");
    menu->insertItem(i18n("&Help"),
#if QT_VERSION < 200
		     kapp->getHelpMenu(false, about)
#else
		     helpMenu(about)
#endif
	);
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

#if QT_VERSION >= 200
static int getPercentSize(QSplitter* w)
{
    const QValueList<int> sizes = w->sizes();
    int total = *sizes.begin() + *sizes.at(1);
    int p = *sizes.begin()*100/total;
    return p;
}

static void restorePercentSize(QSplitter* w, int percent)
{
    QValueList<int> sizes = w->sizes();
    if (sizes.count() < 2) {
	w->setSizes(sizes);
	// only now we have 2 sizes!
	sizes = w->sizes();
    }
    int total = *sizes.begin() + *sizes.at(1);
    int p1 = total*percent/100;
    *sizes.begin() = p1;
    *sizes.at(1) = total-p1;
    w->setSizes(sizes);
}
#endif

const char WindowGroup[] = "Windows";
const char MainPane[] = "MainPane";
const char LeftPane[] = "LeftPane";
const char RightPane[] = "RightPane";
const char MainPosition[] = "MainPosition";
const char BreaklistVisible[] = "BreaklistVisible";
const char Breaklist[] = "Breaklist";


void DebuggerMainWnd::saveSettings(KConfig* config)
{
    KConfigGroupSaver g(config, WindowGroup);
    // panner positions
#if QT_VERSION < 200
    int vsep = m_mainPanner.separatorPos();
    int lsep = m_leftPanner.separatorPos();
    int rsep = m_rightPanner.separatorPos();
#else
    int vsep = getPercentSize(&m_mainPanner);
    int lsep = getPercentSize(&m_leftPanner);
    int rsep = getPercentSize(&m_rightPanner);
#endif
    config->writeEntry(MainPane, vsep);
    config->writeEntry(LeftPane, lsep);
    config->writeEntry(RightPane, rsep);
    // window position
    QRect r = KWM::geometry(winId());
    config->writeEntry(MainPosition, r);

    // breakpoint window
    bool visible = m_bpTable.isVisible();
    // ask window manager for position
    r = KWM::geometry(m_bpTable.winId());
    config->writeEntry(BreaklistVisible, visible);
    config->writeEntry(Breaklist, r);

    DebuggerMainWndBase::saveSettings(config);
}

void DebuggerMainWnd::restoreSettings(KConfig* config)
{
    KConfigGroupSaver g(config, WindowGroup);
    // window position
    QRect pos(0,0,600,600);
    pos = config->readRectEntry(MainPosition, &pos);
    resize(pos.width(), pos.height());	/* only restore size */
    // panner positions
    int vsep = config->readNumEntry(MainPane, 60);
    int lsep = config->readNumEntry(LeftPane, 70);
    int rsep = config->readNumEntry(RightPane, 50);
#if QT_VERSION < 200
    m_mainPanner.setSeparatorPos(vsep);
    m_leftPanner.setSeparatorPos(lsep);
    m_rightPanner.setSeparatorPos(rsep);
#else
    restorePercentSize(&m_mainPanner, vsep);
    restorePercentSize(&m_leftPanner, lsep);
    restorePercentSize(&m_rightPanner, rsep);
#endif

    // breakpoint window
    bool visible = config->readBoolEntry(BreaklistVisible, false);
    if (config->hasKey(Breaklist)) {
	QRect r = config->readRectEntry(Breaklist);
	m_bpTable.setGeometry(r);
    }
    visible ? m_bpTable.show() : m_bpTable.hide();

    DebuggerMainWndBase::restoreSettings(config);
}

void DebuggerMainWnd::menuCallback(int item)
{
    switch (item) {
    case ID_FILE_QUIT:
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
	    if (m_filesWindow.activeLine(file, lineNo))
		m_debugger->runUntil(file, lineNo);
	}
	break;
    case ID_BRKPT_SET:
    case ID_BRKPT_TEMP:
	if (m_debugger != 0)
	{
	    QString file;
	    int lineNo;
	    if (m_filesWindow.activeLine(file, lineNo))
		m_debugger->setBreakpoint(file, lineNo, item == ID_BRKPT_TEMP);
	}
	break;
    case ID_BRKPT_ENABLE:
	if (m_debugger != 0)
	{
	    QString file;
	    int lineNo;
	    if (m_filesWindow.activeLine(file, lineNo))
		m_debugger->enableDisableBreakpoint(file, lineNo);
	}
	break;
    case ID_BRKPT_LIST:
	// toggle visibility
	if (m_bpTable.isVisible()) {
	    m_bpTable.hide();
	} else {
	    m_bpTable.show();
	}
	break;
    default:
	// forward all others
	if (!handleCommand(item))
	    emit forwardMenuCallback(item);
    }
    updateUI();
}

void DebuggerMainWnd::updateUI()
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

    // Update winstack float popup items
    {
	UpdateMenuUI updateMenu(&m_filesWindow.m_menuFloat, this,
				SLOT(updateUIItem(UpdateUI*)));
	updateMenu.iterateMenu();
    }

    // Update winstack float file popup items
    {
	UpdateMenuUI updateMenu(&m_filesWindow.m_menuFileFloat, this,
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

void DebuggerMainWnd::updateUIItem(UpdateUI* item)
{
    switch (item->id) {
    case ID_VIEW_FINDDLG:
	item->setCheck(m_filesWindow.m_findDlg.isVisible());
	break;
    case ID_BRKPT_SET:
    case ID_BRKPT_TEMP:
	item->enable(m_debugger->canChangeBreakpoints());
	break;
    case ID_BRKPT_ENABLE:
	item->enable(m_debugger->canChangeBreakpoints());
	break;
    case ID_BRKPT_LIST:
	item->setCheck(m_bpTable.isVisible());
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
    m_filesWindow.updateLineItems(m_debugger);
}

void DebuggerMainWnd::slotAddWatch()
{
    if (m_debugger != 0) {
	QString t = m_watches.watchText();
	m_debugger->addWatch(t);
    }
}

void DebuggerMainWnd::slotFileChanged()
{
    // set caption
    QString caption = kapp->getCaption();
    if (m_debugger->haveExecutable()) {
	// basename part of executable
	QString executable = m_debugger->executable();
	const char* execBase = executable.data();
	int lastSlash = executable.findRev('/');
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

void DebuggerMainWnd::slotLineChanged()
{
    QString file;
    int line;
    bool anyWindows = m_filesWindow.activeLine(file, line);
    updateLineStatus(anyWindows ? line : -1);
}

void DebuggerMainWnd::slotNewFileLoaded()
{
    // updates program counter in the new file
    m_filesWindow.updateLineItems(m_debugger);
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

void DebuggerMainWnd::slotNewStatusMsg()
{
    DebuggerMainWndBase::slotNewStatusMsg();
}

void DebuggerMainWnd::slotAnimationTimeout()
{
    DebuggerMainWndBase::slotAnimationTimeout();
}

void DebuggerMainWnd::slotGlobalOptions()
{
    DebuggerMainWndBase::slotGlobalOptions();
}

void DebuggerMainWnd::slotDebuggerStarting()
{
    DebuggerMainWndBase::slotDebuggerStarting();
}

void DebuggerMainWnd::slotToggleBreak(const QString& fileName, int lineNo)
{
    // lineNo is zero-based
    if (m_debugger != 0) {
	m_debugger->setBreakpoint(fileName, lineNo, false);
    }
}

void DebuggerMainWnd::slotEnaDisBreak(const QString& fileName, int lineNo)
{
    // lineNo is zero-based
    if (m_debugger != 0) {
	m_debugger->enableDisableBreakpoint(fileName, lineNo);
    }
}

#include "dbgmainwnd.moc"
