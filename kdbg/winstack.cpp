// $Id$

// Copyright by Johannes Sixt
// This file is under GPL, the GNU General Public Licence

#include "winstack.h"
#include "sourcewnd.h"
#include <qbrush.h>
#include <qfileinfo.h>
#include <qlistbox.h>
#include <kapp.h>
#include <kmainwindow.h>
#include <klocale.h>			/* i18n */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include "mydebug.h"



WinStack::WinStack(QWidget* parent, const char* name) :
	QWidget(parent, name),
	m_activeWindow(0),
	m_windowMenu(0),
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

void WinStack::setWindowMenu(QPopupMenu* menu)
{
    m_windowMenu = menu;
    if (menu == 0) {
	return;
    }
    
    // hook up for menu activations
    connect(menu, SIGNAL(activated(int)), this, SLOT(selectWindow(int)));
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
    for (int i = 0; i < m_fileList.size(); i++) {
	m_fileList[i]->reloadFile();
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
    for (int i = 0; i < m_fileList.size(); i++) {
	if (m_fileList[i]->fileName() == pathName) {
	    fw = m_fileList[i];
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

	m_fileList.insertAt(0, fw);
	connect(fw, SIGNAL(lineChanged()),SIGNAL(lineChanged()));
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

	changeWindowMenu();
	
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
    // lookup fw
    int index = m_fileList.size()-1;
    while (index >= 0 && m_fileList[index] != fw)
	--index;
    ASSERT(index >= 0);
    if (index < 0) {
	return false;
    }
    /*
     * If the file is not in the list of those that would appear in the
     * window menu, move it to the first position.
     */
    if (index >= 9) {
	m_fileList.removeAt(index);
	m_fileList.insertAt(0, fw);
	changeWindowMenu();
    }

    // make the line visible
    if (lineNo >= 0) {
	fw->scrollTo(lineNo, address);
    }

    // first resize the window, then lift it to the top
    fw->setGeometry(0,0, width(),height());
    fw->raise();
    fw->show();

    // set the focus to the new active window
    QWidget* oldActive = m_activeWindow;
    fw->setFocusPolicy(QWidget::WheelFocus);
    m_activeWindow = fw;
    if (oldActive != 0 && oldActive != fw) {
	// disable focus on non-active windows
	oldActive->setFocusPolicy(QWidget::NoFocus);
    }
    fw->setFocus();

    emit fileChanged();

    return true;
}

bool WinStack::activeLine(QString& fileName, int& lineNo)
{
    DbgAddr dummy;
    return activeLine(fileName, lineNo, dummy);
}

bool WinStack::activeLine(QString& fileName, int& lineNo, DbgAddr& address)
{
    if (m_activeWindow == 0) {
	return false;
    }
    
    fileName = m_activeWindow->fileName();
    m_activeWindow->activeLine(lineNo, address);
    return true;
}

void WinStack::changeWindowMenu()
{
    if (m_windowMenu == 0) {
	return;
    }

    const int N = 9;
    // Delete entries at most the N window entries that we insert below.
    // When we get here if the More entry was selected, we must make sure
    // that we don't delete it (or we crash).
    bool haveMore = m_windowMenu->count() > uint(N);
    int k = haveMore ? N : m_windowMenu->count();

    for (int i = 0; i < k; i++) {
	m_windowMenu->removeItemAt(0);
    }

    // insert current windows
    QString text;
    for (int i = 0; i < m_fileList.size() && i < N; i++) {
	text.sprintf("&%d ", i+1);
	text += m_fileList[i]->fileName();
	m_windowMenu->insertItem(text, WindowMore+i+1, i);
    }
    // add More entry if we have none yet
    if (!haveMore && m_fileList.size() > N) {
	m_windowMenu->insertItem(i18n("&More..."), WindowMore, N);
    }
}

void WinStack::updateLineItems(const KDebugger* dbg)
{
    for (int i = 0; i < m_fileList.size(); i++) {
	m_fileList[i]->updateLineItems(dbg);
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

void WinStack::setPC(bool set, const QString& fileName, int lineNo,
		     const DbgAddr& address, int frameNo)
{
    TRACE((set ? "set PC: " : "clear PC: ") + fileName +
	  QString().sprintf(":%d#%d ", lineNo, frameNo) + address.asString());
    // find file
    for (int i = 0; i < m_fileList.size(); i++) {
	if (m_fileList[i]->fileNameMatches(fileName)) {
	    m_fileList[i]->setPC(set, lineNo, address, frameNo);
	    break;
	}
    }
}

void WinStack::resizeEvent(QResizeEvent*)
{
    if (m_activeWindow != 0) {
	m_activeWindow->resize(width(), height());
    }
}

QString WinStack::activeFileName() const
{
    QString f;
    if (m_activeWindow != 0)
	f = m_activeWindow->fileName();
    return f;
}

void WinStack::slotFindForward()
{
    if (m_activeWindow != 0)
	m_activeWindow->find(m_findDlg.searchText(), m_findDlg.caseSensitive(),
			     SourceWindow::findForward);
}

void WinStack::slotFindBackward()
{
    if (m_activeWindow != 0)
	m_activeWindow->find(m_findDlg.searchText(), m_findDlg.caseSensitive(),
			     SourceWindow::findBackward);
}

void WinStack::maybeTip(const QPoint& p)
{
    if (m_activeWindow == 0)
	return;

    // get the word at the point
    QString word;
    QRect r;
    if (!m_activeWindow->wordAtPoint(p, word, r))
	return;

    // must be valid
    assert(!word.isEmpty());
    assert(r.isValid());

    // remember the location
    m_tipLocation = r;

    emit initiateValuePopup(word);
}

void WinStack::slotShowValueTip(const QString& tipText)
{
    m_valueTip.tip(m_tipLocation, tipText);
}

void WinStack::slotDisassembled(const QString& fileName, int lineNo,
				const QList<DisassembledCode>& disass)
{
    // lookup the file
    SourceWindow* fw = 0;
    for (int i = 0; i < m_fileList.size(); i++) {
	if (m_fileList[i]->fileNameMatches(fileName)) {
	    fw = m_fileList[i];
	    break;
	}
    }
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
    if (m_activeWindow != 0) {
	TRACE("reloading one file");
	m_activeWindow->reloadFile();
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


ValueTip::ValueTip(WinStack* parent) :
	QToolTip(parent)
{
}

void ValueTip::maybeTip(const QPoint& p)
{
    WinStack* w = static_cast<WinStack*>(parentWidget());
    w->maybeTip(p);
}


class MoreWindowsDialog : public QDialog
{
public:
    MoreWindowsDialog(QWidget* parent);
    virtual ~MoreWindowsDialog();

    void insertString(const char* text) { m_list.insertItem(text); }
    void setListIndex(int i) { m_list.setCurrentItem(i); }
    int listIndex() const { return m_list.currentItem(); }

protected:
    QListBox m_list;
    QPushButton m_buttonOK;
    QPushButton m_buttonCancel;
    QVBoxLayout m_layout;
    QHBoxLayout m_buttons;
};

MoreWindowsDialog::MoreWindowsDialog(QWidget* parent) :
	QDialog(parent, "morewindows", true),
	m_list(this, "windows"),
	m_buttonOK(this, "show"),
	m_buttonCancel(this, "cancel"),
	m_layout(this, 8),
	m_buttons(4)
{
    QString title = kapp->caption();
    title += i18n(": Open Windows");
    setCaption(title);

    m_list.setMinimumSize(250, 100);
    connect(&m_list, SIGNAL(selected(int)), SLOT(accept()));

    m_buttonOK.setMinimumSize(100, 30);
    connect(&m_buttonOK, SIGNAL(clicked()), SLOT(accept()));
    m_buttonOK.setText(i18n("Show"));
    m_buttonOK.setDefault(true);

    m_buttonCancel.setMinimumSize(100, 30);
    connect(&m_buttonCancel, SIGNAL(clicked()), SLOT(reject()));
    m_buttonCancel.setText(i18n("Cancel"));

    m_layout.addWidget(&m_list, 10);
    m_layout.addLayout(&m_buttons);
    m_buttons.addStretch(10);
    m_buttons.addWidget(&m_buttonOK);
    m_buttons.addSpacing(40);
    m_buttons.addWidget(&m_buttonCancel);
    m_buttons.addStretch(10);

    m_layout.activate();

    m_list.setFocus();
    resize(320, 320);
}

MoreWindowsDialog::~MoreWindowsDialog()
{
}

void WinStack::selectWindow(int id)
{
    // react only on menu entries concerning windows
    if ((id & ~WindowMask) != WindowMore) {
	return;
    }

    id &= WindowMask;

    int index = 0;

    if (id == 0) {
	// more windows selected: show windows in a list
	MoreWindowsDialog dlg(this);
	for (int i = 0; i < m_fileList.size(); i++)
	{
	    dlg.insertString(m_fileList[i]->fileName());
	    if (m_activeWindow == m_fileList[i]) {
		index = i;
	    }
	}
	dlg.setListIndex(index);
	if (dlg.exec() == QDialog::Rejected)
	    return;
	index = dlg.listIndex();
    } else {
	index = (id & WindowMask)-1;
    }

    SourceWindow* fw = m_fileList[index];
    ASSERT(fw != 0);
	
    activateWindow(fw, -1, DbgAddr());
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
