// $Id$

// Copyright by Johannes Sixt
// This file is under GPL, the GNU General Public Licence

#include <kapp.h>
#include <kiconloader.h>
#include <kstdaccel.h>
#include "dbgmainwnd.h"
#include "debugger.h"
#include "updateui.h"
#include "commandids.h"
#include "mydebug.h"


KStdAccel* keys = 0;

DebuggerMainWnd::DebuggerMainWnd(const char* name) :
	KTopLevelWidget(name),
	m_menu(this, "menu"),
	m_toolbar(this, "toolbar"),
	m_statusbar(this, "statusbar"),
	m_mainPanner(this, "main_pane", KNewPanner::Vertical, KNewPanner::Percent, 60),
	m_leftPanner(&m_mainPanner, "left_pane", KNewPanner::Horizontal, KNewPanner::Percent, 70),
	m_rightPanner(&m_mainPanner, "right_pane", KNewPanner::Horizontal, KNewPanner::Percent, 50),
	m_filesWindow(&m_leftPanner, "files"),
	m_btWindow(&m_leftPanner, "backtrace"),
	m_localVariables(&m_rightPanner, "locals"),
	m_watches(&m_rightPanner, "watches"),
	m_watchEdit(&m_watches, "watch_edit"),
	m_watchAdd(i18n(" Add "), &m_watches, "watch_add"),
	m_watchDelete(i18n(" Del "), &m_watches, "watch_delete"),
	m_watchVariables(&m_watches, "watch_variables"),
	m_watchV(&m_watches, 0),
	m_watchH(0),
	m_animationCounter(0),
	m_debugger(new KDebugger(this,
				 &m_localVariables,
				 &m_watchVariables,
				 &m_btWindow))
{
    m_statusActive = i18n("active");

    initMenu();
    setMenu(&m_menu);
    
    initToolbar();
    addToolBar(&m_toolbar);
    
    setStatusBar(&m_statusbar);
    connect(m_debugger, SIGNAL(updateStatusMessage()), SLOT(newStatusMsg()));
    connect(m_debugger, SIGNAL(updateUI()), SLOT(updateUI()));

    m_mainPanner.activate(&m_leftPanner, &m_rightPanner);
    setView(&m_mainPanner, true);	/* show frame */

    m_leftPanner.activate(&m_filesWindow, &m_btWindow);
    m_rightPanner.activate(&m_localVariables, &m_watches);

    connect(&m_watchEdit, SIGNAL(returnPressed()), SLOT(slotAddWatch()));
    connect(&m_watchAdd, SIGNAL(clicked()), SLOT(slotAddWatch()));
    connect(&m_watchDelete, SIGNAL(clicked()),
	    m_debugger, SLOT(slotDeleteWatch()));
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
    connect(&m_filesWindow, SIGNAL(newFileLoaded()),
	    SLOT(slotNewFileLoaded()));
    connect(&m_filesWindow, SIGNAL(toggleBreak(const QString&, int)),
	    m_debugger, SLOT(slotToggleBreak(const QString&,int)));
    connect(&m_filesWindow, SIGNAL(enadisBreak(const QString&, int)),
	    m_debugger, SLOT(slotEnaDisBreak(const QString&,int)));
    connect(m_debugger, SIGNAL(activateFileLine(const QString&,int)),
	    &m_filesWindow, SLOT(activate(const QString&,int)));
    connect(m_debugger, SIGNAL(executableUpdated()),
	    &m_filesWindow, SLOT(reloadAllFiles()));
    connect(m_debugger, SIGNAL(updatePC(const QString&,int,int)),
	    &m_filesWindow, SLOT(updatePC(const QString&,int,int)));
    connect(m_debugger, SIGNAL(lineItemsChanged()),
	    SLOT(updateLineItems()));

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

    restoreSettings(kapp->getConfig());
    resize(700, 700);

    emit updateUI();
    slotFileChanged();
}

DebuggerMainWnd::~DebuggerMainWnd()
{
    saveSettings(kapp->getConfig());

    delete m_debugger;
}

void DebuggerMainWnd::initMenu()
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

void DebuggerMainWnd::initToolbar()
{
    KIconLoader* loader = kapp->getIconLoader();

    m_toolbar.insertButton(loader->loadIcon("execopen.xpm"),ID_FILE_EXECUTABLE, true,
			   i18n("Executable"));
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
    
    initAnimation();

    m_statusbar.insertItem(m_statusActive, ID_STATUS_ACTIVE);
    m_statusbar.insertItem(i18n("Line 00000"), ID_STATUS_LINENO);
    m_statusbar.insertItem("", ID_STATUS_MSG);	/* message pane */

    // reserve some translations
    i18n("Restart");
    i18n("Core dump");
}


void DebuggerMainWnd::setCoreFile(const QString& corefile)
{
    m_debugger->setCoreFile(corefile);
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
const char MainPane[] = "MainPane";
const char LeftPane[] = "LeftPane";
const char RightPane[] = "RightPane";

void DebuggerMainWnd::saveSettings(KConfig* config)
{
    KConfigGroupSaver g(config, WindowGroup);
    // panner positions
    int vsep = m_mainPanner.separatorPos();
    int lsep = m_leftPanner.separatorPos();
    int rsep = m_rightPanner.separatorPos();
    config->writeEntry(MainPane, vsep);
    config->writeEntry(LeftPane, lsep);
    config->writeEntry(RightPane, rsep);

    if (m_debugger != 0) {
	m_debugger->saveSettings(config);
    }
}

void DebuggerMainWnd::restoreSettings(KConfig* config)
{
    KConfigGroupSaver g(config, WindowGroup);
    // panner positions
    int vsep = config->readNumEntry(MainPane, 60);
    int lsep = config->readNumEntry(LeftPane, 70);
    int rsep = config->readNumEntry(RightPane, 50);
    m_mainPanner.setSeparatorPos(vsep);
    m_leftPanner.setSeparatorPos(lsep);
    m_rightPanner.setSeparatorPos(rsep);

    if (m_debugger != 0) {
	m_debugger->restoreSettings(config);
    }
}

bool DebuggerMainWnd::debugProgram(const QString& executable)
{
    assert(m_debugger != 0);
    return m_debugger->debugProgram(executable);
}

void DebuggerMainWnd::menuCallback(int item)
{
    switch (item) {
    case ID_FILE_EXECUTABLE:
	if (m_debugger != 0)
	    m_debugger->fileExecutable();
	break;
    case ID_FILE_COREFILE:
	if (m_debugger != 0)
	    m_debugger->fileCoreFile();
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
	if (m_debugger != 0)
	    m_debugger->programRun();
	break;
    case ID_PROGRAM_ATTACH:
	if (m_debugger != 0)
	    m_debugger->programAttach();
	break;
    case ID_PROGRAM_RUN_AGAIN:
	if (m_debugger != 0)
	    m_debugger->programRunAgain();
	break;
    case ID_PROGRAM_STEP:
	if (m_debugger != 0)
	    m_debugger->programStep();
	break;
    case ID_PROGRAM_NEXT:
	if (m_debugger != 0)
	    m_debugger->programNext();
	break;
    case ID_PROGRAM_FINISH:
	if (m_debugger != 0)
	    m_debugger->programFinish();
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
    case ID_PROGRAM_BREAK:
	if (m_debugger != 0)
	    m_debugger->programBreak();
	break;
    case ID_PROGRAM_ARGS:
	if (m_debugger != 0)
	    m_debugger->programArgs();
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
	if (m_debugger != 0)
	    m_debugger->breakListToggleVisible();
	break;
    default:
	// forward all others
	emit forwardMenuCallback(item);
    }
    emit updateUI();
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
    UpdateToolbarUI updateToolbar(&m_toolbar, this, SLOT(updateUIItem(UpdateUI*)),
				  toolIds, sizeof(toolIds)/sizeof(toolIds[0]));
    updateToolbar.iterateToolbar();
}

void DebuggerMainWnd::updateUIItem(UpdateUI* item)
{
    switch (item->id) {
    case ID_VIEW_FINDDLG:
	item->setCheck(m_filesWindow.m_findDlg.isVisible());
	break;
//    case ID_FILE_EXECUTABLE:
//	item->enable(m_state == DSidle);
//	break;
    case ID_FILE_COREFILE:
	item->enable(m_debugger->canChangeExecutable());
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
    case ID_PROGRAM_BREAK:
	item->enable(m_debugger->isProgramRunning());
	break;
    case ID_PROGRAM_ARGS:
	item->enable(m_debugger->haveExecutable());
    case ID_BRKPT_SET:
    case ID_BRKPT_TEMP:
	item->enable(m_debugger->canChangeBreakpoints());
	break;
    case ID_BRKPT_ENABLE:
	item->enable(m_debugger->canChangeBreakpoints());
	break;
    case ID_BRKPT_LIST:
	item->setCheck(m_debugger->isBreakListVisible());
	break;
    }
    
    // update statusbar
    m_statusbar.changeItem(m_debugger->isProgramActive() ?
			   static_cast<const char*>(m_statusActive) : "",
			   ID_STATUS_ACTIVE);
    // line number is updated in slotLineChanged
}

void DebuggerMainWnd::updateLineItems()
{
    m_filesWindow.updateLineItems(m_debugger->breakList());
}

void DebuggerMainWnd::initAnimation()
{
    QPixmap pixmap;
    QString path = kapp->kde_datadir() + "/kfm/pics/";

    pixmap.load(path + "/kde1.xpm");
    
    m_toolbar.insertButton(pixmap, ID_STATUS_BUSY);
    m_toolbar.alignItemRight(ID_STATUS_BUSY, true);
    
    // Load animated logo
    m_animation.setAutoDelete(true);
    QString n;
    for (int i = 1; i <= 9; i++) {
	n.sprintf("/kde%d.xpm", i);
	QPixmap* p = new QPixmap();
	p->load(path + n);
	if (!p->isNull()) {
	    m_animation.append(p);
	}
    }
    connect(m_debugger, SIGNAL(animationTimeout()), SLOT(slotAnimationTimeout()));
}

void DebuggerMainWnd::slotAnimationTimeout()
{
    m_animationCounter++;
    if (m_animationCounter == m_animation.count())
	m_animationCounter = 0;
    m_toolbar.setButtonPixmap(ID_STATUS_BUSY,
			      *m_animation.at(m_animationCounter));
}

void DebuggerMainWnd::newStatusMsg()
{
    QString msg = m_debugger->statusMessage();
    m_statusbar.changeItem(msg, ID_STATUS_MSG);
}

void DebuggerMainWnd::slotAddWatch()
{
    if (m_debugger != 0) {
	QString t = m_watchEdit.text();
	m_debugger->addWatch(t);
    }
}

// place the text of the hightlighted watch expr in the edit field
void DebuggerMainWnd::slotWatchHighlighted(int index)
{
    QString text = m_watchVariables.exprStringAt(index);
    m_watchEdit.setText(text);
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
    m_filesWindow.updateLineItems(m_debugger->breakList());
}

void DebuggerMainWnd::updateLineStatus(int lineNo)
{
    if (lineNo < 0) {
	m_statusbar.changeItem("", ID_STATUS_LINENO);
    } else {
	QString strLine;
	strLine.sprintf(i18n("Line %d"), lineNo + 1);
	m_statusbar.changeItem(strLine, ID_STATUS_LINENO);
    }
}

#include "dbgmainwnd.moc"
