/*
 * Copyright Johannes Sixt
 * This file is licensed under the GNU General Public License Version 2.
 * See the file COPYING in the toplevel directory of the source directory.
 */

#include "winstack.h"
#include "sourcewnd.h"
#include <qbrush.h>
#include <qfileinfo.h>
#include <qpopupmenu.h>
#include <kapplication.h>
#include <kmainwindow.h>
#include <klocale.h>			/* i18n */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include "mydebug.h"



WinStack::WinStack(QWidget* parent, const char* name) :
	KTabWidget(parent, name),
	m_pcLine(-1),
	m_valueTip(this),
	m_tipLocation(1,1,10,10),
	m_tabWidth(0)
{
    connect(&m_findDlg.m_buttonForward,
	    SIGNAL(clicked()), SLOT(slotFindForward()));
    connect(&m_findDlg.m_buttonBackward,
	    SIGNAL(clicked()), SLOT(slotFindBackward()));

    connect(this, SIGNAL(setTabWidth(int)), this, SLOT(slotSetTabWidth(int)));
}

WinStack::~WinStack()
{
}

void WinStack::contextMenuEvent(QContextMenuEvent* e)
{
    // get the context menu from the GUI factory
    QWidget* top = this;
    do
	top = top->parentWidget();
    while (!top->isTopLevel());
    KMainWindow* mw = static_cast<KMainWindow*>(top);
    QPopupMenu* m =
	static_cast<QPopupMenu*>(mw->factory()->container("popup_files_empty", mw));
    m->exec(e->globalPos());
}


void WinStack::reloadAllFiles()
{
    for (int i = count()-1; i >= 0; i--) {
	windowAt(i)->reloadFile();
    }
}

QSize WinStack::sizeHint() const
{
    return QSize(640, 480);
}

void WinStack::activate(const QString& fileName, int lineNo, const DbgAddr& address)
{
    QFileInfo fi(fileName);

    if (!fi.isFile()) {
	/*
	 * We didn't find that file. Now check if it is a relative path and
	 * try m_lastOpenDir as prefix.
	 */
	TRACE(fi.filePath() + (" not found, looking in " + m_lastOpenDir));
	if (!fi.isRelative() || m_lastOpenDir.isEmpty()) {
	    return;
	}
	fi.setFile(m_lastOpenDir + "/" + fi.filePath());
	if (!fi.isFile()) {
	    return;
	}
    }
    // if this is not an absolute path name, make it one
    activatePath(fi.absFilePath(), lineNo, address);
}

void WinStack::activateFile(const QString& fileName)
{
    activatePath(fileName, 0, DbgAddr());
}

bool WinStack::activatePath(QString pathName, int lineNo, const DbgAddr& address)
{
    // check whether the file is already open
    SourceWindow* fw = 0;
    for (int i = count()-1; i >= 0; i--) {
	if (windowAt(i)->fileName() == pathName) {
	    fw = windowAt(i);
	    break;
	}
    }
    if (fw == 0) {
	// not found, load it
	fw = new SourceWindow(pathName, this, "fileWindow");

	// slurp the file in
	if (!fw->loadFile()) {
	    // read failed
	    delete fw;
	    return false;
	}

	addTab(fw, QFileInfo(pathName).fileName());
	setTabToolTip(fw, pathName);

	connect(fw, SIGNAL(clickedLeft(const QString&,int,const DbgAddr&,bool)),
		SIGNAL(toggleBreak(const QString&,int,const DbgAddr&,bool)));
	connect(fw, SIGNAL(clickedMid(const QString&,int,const DbgAddr&)),
		SIGNAL(enadisBreak(const QString&,int,const DbgAddr&)));

	// disassemble code
	connect(fw, SIGNAL(disassemble(const QString&, int)),
		SIGNAL(disassemble(const QString&, int)));
	connect(fw, SIGNAL(expanded(int)), SLOT(slotExpandCollapse(int)));
	connect(fw, SIGNAL(collapsed(int)), SLOT(slotExpandCollapse(int)));

	// tab width
	connect(this, SIGNAL(setTabWidth(int)), fw, SLOT(setTabWidth(int)));
	fw->setTabWidth(m_tabWidth);
	fw->setFocusPolicy(QWidget::WheelFocus);

	// set PC if there is one
	emit newFileLoaded();
	if (m_pcLine >= 0) {
	    setPC(true, m_pcFile, m_pcLine, DbgAddr(m_pcAddress), m_pcFrame);
	}
    }
    return activateWindow(fw, lineNo, address);
}

bool WinStack::activateWindow(SourceWindow* fw, int lineNo, const DbgAddr& address)
{
    // make the line visible
    if (lineNo >= 0) {
	fw->scrollTo(lineNo, address);
    }

    showPage(fw);
    fw->setFocus();

    return true;
}

bool WinStack::activeLine(QString& fileName, int& lineNo)
{
    DbgAddr dummy;
    return activeLine(fileName, lineNo, dummy);
}

bool WinStack::activeLine(QString& fileName, int& lineNo, DbgAddr& address)
{
    if (activeWindow() == 0) {
	return false;
    }
    
    fileName = activeFileName();
    activeWindow()->activeLine(lineNo, address);
    return true;
}

void WinStack::updateLineItems(const KDebugger* dbg)
{
    for (int i = count()-1; i >= 0; i--) {
	windowAt(i)->updateLineItems(dbg);
    }
}

void WinStack::updatePC(const QString& fileName, int lineNo, const DbgAddr& address, int frameNo)
{
    if (m_pcLine >= 0) {
	setPC(false, m_pcFile, m_pcLine, DbgAddr(m_pcAddress), m_pcFrame);
    }
    m_pcFile = fileName;
    m_pcLine = lineNo;
    m_pcAddress = address.asString();
    m_pcFrame = frameNo;
    if (lineNo >= 0) {
	setPC(true, fileName, lineNo, address, frameNo);
    }
}

SourceWindow* WinStack::findByFileName(const QString& fileName) const
{
    for (int i = count()-1; i >= 0; i--) {
	if (windowAt(i)->fileNameMatches(fileName)) {
	    return windowAt(i);
	}
    }
    return 0;
}

void WinStack::setPC(bool set, const QString& fileName, int lineNo,
		     const DbgAddr& address, int frameNo)
{
    TRACE((set ? "set PC: " : "clear PC: ") + fileName +
	  QString().sprintf(":%d#%d ", lineNo, frameNo) + address.asString());
    SourceWindow* fw = findByFileName(fileName);
    if (fw)
	fw->setPC(set, lineNo, address, frameNo);
}

SourceWindow* WinStack::windowAt(int i) const
{
    return static_cast<SourceWindow*>(page(i));
}

SourceWindow* WinStack::activeWindow() const
{
    return static_cast<SourceWindow*>(currentPage());
}

QString WinStack::activeFileName() const
{
    QString f;
    if (activeWindow() != 0)
	f = activeWindow()->fileName();
    return f;
}

void WinStack::slotFindForward()
{
    if (activeWindow() != 0)
	activeWindow()->find(m_findDlg.searchText(), m_findDlg.caseSensitive(),
			     SourceWindow::findForward);
}

void WinStack::slotFindBackward()
{
    if (activeWindow() != 0)
	activeWindow()->find(m_findDlg.searchText(), m_findDlg.caseSensitive(),
			     SourceWindow::findBackward);
}

void WinStack::maybeTip(const QPoint& p)
{
    SourceWindow* w = activeWindow();
    if (w == 0)
	return;

    // get the word at the point
    QString word;
    QRect r;
    if (!w->wordAtPoint(w->mapFrom(this, p), word, r))
	return;

    // must be valid
    assert(!word.isEmpty());
    assert(r.isValid());

    // remember the location
    m_tipLocation = QRect(w->mapTo(this, r.topLeft()), r.size());

    emit initiateValuePopup(word);
}

void WinStack::slotShowValueTip(const QString& tipText)
{
    m_valueTip.tip(m_tipLocation, tipText);
}

void WinStack::slotDisassembled(const QString& fileName, int lineNo,
				const std::list<DisassembledCode>& disass)
{
    SourceWindow* fw = findByFileName(fileName);
    if (fw == 0) {
	// not found: ignore
	return;
    }

    fw->disassembled(lineNo, disass);
}

void WinStack::slotExpandCollapse(int)
{
    // update line items after expanding or collapsing disassembled code

    // HACK: we know that this will result in updateLineItems
    // should be done more cleanly with a separate signal
    emit newFileLoaded();

    if (m_pcLine >= 0) {
	setPC(true, m_pcFile, m_pcLine, DbgAddr(m_pcAddress), m_pcFrame);
    }
}


void WinStack::slotSetTabWidth(int numChars)
{
    m_tabWidth = numChars;
}

void WinStack::slotFileReload()
{
    if (activeWindow() != 0) {
	TRACE("reloading one file");
	activeWindow()->reloadFile();
    }
}

void WinStack::slotViewFind()
{
    if (m_findDlg.isVisible()) {
	m_findDlg.done(0);
    } else {
	m_findDlg.show();
    }
}

void WinStack::slotBrkptSet()
{
    QString file;
    int lineNo;
    DbgAddr address;
    if (activeLine(file, lineNo, address))
	emit toggleBreak(file, lineNo, address, false);
}

void WinStack::slotBrkptSetTemp()
{
    QString file;
    int lineNo;
    DbgAddr address;
    if (activeLine(file, lineNo, address))
	emit toggleBreak(file, lineNo, address, true);
}

void WinStack::slotBrkptEnable()
{
    QString file;
    int lineNo;
    DbgAddr address;
    if (activeLine(file, lineNo, address))
	emit enadisBreak(file, lineNo, address);
}

void WinStack::slotMoveProgramCounter()
{
    QString file;
    int lineNo;
    DbgAddr address;
    if (activeLine(file, lineNo, address))
	emit moveProgramCounter(file, lineNo, address);
}

void WinStack::slotClose()
{
    QWidget* w = activeWindow();
    if (!w)
	return;

    removePage(w);
    delete w;
}


ValueTip::ValueTip(WinStack* parent) :
	QToolTip(parent)
{
}

void ValueTip::maybeTip(const QPoint& p)
{
    WinStack* w = static_cast<WinStack*>(parentWidget());
    w->maybeTip(p);
}


FindDialog::FindDialog() :
	QDialog(0, "find", false),
	m_searchText(this, "text"),
	m_caseCheck(this, "case"),
	m_buttonForward(this, "forward"),
	m_buttonBackward(this, "backward"),
	m_buttonClose(this, "close"),
	m_layout(this, 8),
	m_buttons(4)
{
    setCaption(QString(kapp->caption()) + i18n(": Search"));

    m_searchText.setMinimumSize(330, 24);
    m_searchText.setMaxLength(10000);
    m_searchText.setFrame(true);

    m_caseCheck.setText(i18n("&Case sensitive"));
    m_caseCheck.setChecked(true);
    m_buttonForward.setText(i18n("&Forward"));
    m_buttonForward.setDefault(true);
    m_buttonBackward.setText(i18n("&Backward"));
    m_buttonClose.setText(i18n("Close"));

    m_caseCheck.setMinimumSize(330, 24);

    // get maximum size of buttons
    QSize maxSize(80,30);
    maxSize.expandedTo(m_buttonForward.sizeHint());
    maxSize.expandedTo(m_buttonBackward.sizeHint());
    maxSize.expandedTo(m_buttonClose.sizeHint());

    m_buttonForward.setMinimumSize(maxSize);
    m_buttonBackward.setMinimumSize(maxSize);
    m_buttonClose.setMinimumSize(maxSize);

    connect(&m_buttonClose, SIGNAL(clicked()), SLOT(reject()));

    m_layout.addWidget(&m_searchText);
    m_layout.addWidget(&m_caseCheck);
    m_layout.addLayout(&m_buttons);
    m_layout.addStretch(10);
    m_buttons.addWidget(&m_buttonForward);
    m_buttons.addStretch(10);
    m_buttons.addWidget(&m_buttonBackward);
    m_buttons.addStretch(10);
    m_buttons.addWidget(&m_buttonClose);

    m_layout.activate();

    m_searchText.setFocus();
    resize( 350, 120 );
}

FindDialog::~FindDialog()
{
}

void FindDialog::closeEvent(QCloseEvent* ev)
{
    QDialog::closeEvent(ev);
    emit closed();
}

void FindDialog::done(int result)
{
    QDialog::done(result);
    emit closed();
}

#include "winstack.moc"
