// $Id$

// Copyright by Johannes Sixt
// This file is under GPL, the GNU General Public Licence

#include <kapp.h>
#if QT_VERSION >= 200
#include <klocale.h>			/* i18n */
#include <klibglobal.h>
#endif
#include <kiconloader.h>
#include <kstdaccel.h>
#include <qpainter.h>
#include <qtabdialog.h>
#include "mainwndbase.h"
#include "debugger.h"
#include "prefdebugger.h"
#include "updateui.h"
#include "commandids.h"
#include "mydebug.h"


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


DebuggerMainWndBase::DebuggerMainWndBase(const char* name) :
	KTMainWindow(name),
	m_animationCounter(0),
	m_debugger(0)
{
    m_statusActive = i18n("active");
}

DebuggerMainWndBase::~DebuggerMainWndBase()
{
    delete m_debugger;
}

void DebuggerMainWndBase::setupDebugger(ExprWnd* localVars,
					ExprWnd* watchVars,
					QListBox* backtrace)
{
    m_debugger = new KDebugger(this, localVars, watchVars, backtrace);

    connect(m_debugger, SIGNAL(updateStatusMessage()), SLOT(slotNewStatusMsg()));
    connect(m_debugger, SIGNAL(updateUI()), SLOT(updateUI()));

    connect(m_debugger, SIGNAL(lineItemsChanged()),
	    SLOT(updateLineItems()));
    
    connect(m_debugger, SIGNAL(animationTimeout()), SLOT(slotAnimationTimeout()));
}


void DebuggerMainWndBase::setCoreFile(const QString& corefile)
{
    m_debugger->setCoreFile(corefile);
}


void DebuggerMainWndBase::saveSettings(KConfig* config)
{
    if (m_debugger != 0) {
	m_debugger->saveSettings(config);
    }
}

void DebuggerMainWndBase::restoreSettings(KConfig* config)
{
    if (m_debugger != 0) {
	m_debugger->restoreSettings(config);
    }
}

bool DebuggerMainWndBase::debugProgram(const QString& executable)
{
    assert(m_debugger != 0);
    return m_debugger->debugProgram(executable);
}

void DebuggerMainWndBase::menuCallback(int item)
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
    case ID_FILE_GLOBAL_OPTIONS:
	slotGlobalOptions();
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
    case ID_PROGRAM_BREAK:
	if (m_debugger != 0)
	    m_debugger->programBreak();
	break;
    case ID_PROGRAM_ARGS:
	if (m_debugger != 0)
	    m_debugger->programArgs();
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

void DebuggerMainWndBase::updateUI()
{
    // toolbar
    static const int toolIds[] = {
	ID_PROGRAM_RUN, ID_PROGRAM_STEP, ID_PROGRAM_NEXT, ID_PROGRAM_FINISH,
	ID_BRKPT_SET
    };
    UpdateToolbarUI updateToolbar(toolBar(), this, SLOT(updateUIItem(UpdateUI*)),
				  toolIds, sizeof(toolIds)/sizeof(toolIds[0]));
    updateToolbar.iterateToolbar();
}

void DebuggerMainWndBase::updateUIItem(UpdateUI* item)
{
    switch (item->id) {
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
	break;
    case ID_BRKPT_LIST:
	item->setCheck(m_debugger->isBreakListVisible());
	break;
    }
    
    // update statusbar
    statusBar()->changeItem(m_debugger->isProgramActive() ?
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
    KLibGlobal libglobal("kdbg");
    QPixmap pixmap = BarIcon("kde1", &libglobal);
#endif

    KToolBar* toolbar = toolBar();
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
	QPixmap* p = new QPixmap(BarIcon(n,&libglobal));
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
	p.fillRect(0,0,2,2,QBrush(white));
	m_animation.append(pix);
    }
}

void DebuggerMainWndBase::slotAnimationTimeout()
{
    assert(m_animation.count() > 0);	/* must have been initialized */
    m_animationCounter++;
    if (m_animationCounter == m_animation.count())
	m_animationCounter = 0;
    toolBar()->setButtonPixmap(ID_STATUS_BUSY,
			       *m_animation.at(m_animationCounter));
}

void DebuggerMainWndBase::slotNewStatusMsg()
{
    QString msg = m_debugger->statusMessage();
    statusBar()->changeItem(msg, ID_STATUS_MSG);
}

void DebuggerMainWndBase::slotGlobalOptions()
{
    QTabDialog dlg(this, "global_options", true);
    QString title = kapp->getCaption();
    title += i18n(": Global options");
    dlg.setCaption(title);
    dlg.setCancelButton(i18n("Cancel"));
    dlg.setOKButton(i18n("OK"));

    PrefDebugger prefDebugger(&dlg);
    prefDebugger.setDebuggerCmd(m_debugger->debuggerCmd());
    prefDebugger.setTerminal(m_debugger->terminalCmd());
    
    dlg.addTab(&prefDebugger, "&Debugger");
    if (dlg.exec() == QDialog::Accepted) {
	m_debugger->setDebuggerCmd(prefDebugger.debuggerCmd());
	m_debugger->setTerminalCmd(prefDebugger.terminal());
    }
}

#include "mainwndbase.moc"
