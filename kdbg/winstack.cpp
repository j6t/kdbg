// $Id$

// Copyright by Johannes Sixt
// This file is under GPL, the GNU General Public Licence

#include "winstack.h"
#include "winstack.moc"
#include "commandids.h"
#include "brkpt.h"
#include <kfiledialog.h>
#include <qpopmenu.h>
#include <qtstream.h>
#include <qpainter.h>
#include <qbrush.h>
#include <qfile.h>
#include <qfileinf.h>
#include <kapp.h>
#include <kiconloader.h>
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include "mydebug.h"


FileWindow::FileWindow(const char* fileName, QWidget* parent, const char* name) :
	KTextView(parent, name),
	m_fileName(fileName)
{
    setNumCols(2);
    setFont(QFont("courier"));

    // load pixmaps
    KIconLoader* loader = kapp->getIconLoader();
    m_pcinner = loader->loadIcon("pcinner.xpm");
    m_pcup = loader->loadIcon("pcup.xpm");
    m_brkena = loader->loadIcon("brkena.xpm");
    m_brkdis = loader->loadIcon("brkdis.xpm");
}

FileWindow::~FileWindow()
{
}

void FileWindow::loadFile()
{
    QFile f(m_fileName);
    if (f.open(IO_ReadOnly)) {
	QTextStream t(&f);
	QString s;
	while (!t.eof()) {
	    s = t.readLine();
	    insertLine(s);
	}
	f.close();
    }
    
    // allocate line items
    m_lineItems.fill(0, numRows());
}

void FileWindow::scrollTo(int lineNo)
{
    if (lineNo >= numRows())
	lineNo = numRows();

    // scroll to lineNo
    if (lineNo >= topCell() && lineNo <= lastRowVisible()) {
	// line is already visible
	setCursorPosition(lineNo, 0);
	return;
    }

    // setCursorPosition does only a half-hearted job of making the
    // cursor line visible, so we do it by hand...
    setCursorPosition(lineNo, 0);
    lineNo -= 5;
    if (lineNo < 0)
	lineNo = 0;

    setTopCell(lineNo);
}

int FileWindow::textCol() const
{
    // text is in column 1
    return 1;
}

int FileWindow::cellWidth(int col)
{
    if (col == 0) {
	return 15;
    } else {
	return KTextView::cellWidth(col);
    }
}

void FileWindow::paintCell(QPainter* p, int row, int col)
{
    if (col == textCol()) {
	KTextView::paintCell(p, row, col);
	return;
    }
    if (col == 0) {
	uchar item = m_lineItems[row];
	if (item == 0)			/* shortcut out */
	    return;
//	p->save();
//	int w = 15;
	int h = cellHeight(row);
	if (item & liBP) {
	    // enabled breakpoint
	    int y = (h - m_brkena.height())/2;
	    if (y < 0) y = 0;
	    p->drawPixmap(0,y,m_brkena);
	}
	if (item & liBPdisabled) {
	    // disabled breakpoint
	    int y = (h - m_brkdis.height())/2;
	    if (y < 0) y = 0;
	    p->drawPixmap(0,y,m_brkdis);
	}
	if (item & liPC) {
	    // program counter in innermost frame
	    int y = (h - m_pcinner.height())/2;
	    if (y < 0) y = 0;
	    p->drawPixmap(0,y,m_pcinner);
	}
	if (item & liPCup) {
	    // program counter somewhere up the stack
	    int y = (h - m_pcup.height())/2;
	    if (y < 0) y = 0;
	    p->drawPixmap(0,y,m_pcup);
	}
//	p->restore();
	return;
    }
}

void FileWindow::updateLineItems(const BreakpointTable& bpt)
{
    // get the simple name for this file
    QString name = fileName();
    name.detach();
    int slash = name.findRev('/');
    if (slash >= 0) {
	name.remove(0, slash+1);
    }

    // clear outdated breakpoints
    for (int i = m_lineItems.size()-1; i >= 0; i--) {
	if (m_lineItems[i] & liBPany) {
	    // check if this breakpoint still exists
	    TRACE(QString().sprintf("checking for bp at %d", i));
	    int j;
	    for (j = bpt.numBreakpoints()-1; j >= 0; j--) {
		const Breakpoint& bp = bpt.breakpoint(j);
		if (bp.lineNo == i && bp.fileName == name) {
		    // yes it exists; mode is changed below
		    break;
		}
	    }
	    if (j < 0) {
		/* doesn't exist anymore, remove it */
		m_lineItems[i] &= ~liBPany;
		updateLineItem(i);
	    }
	}
    }

    // add new breakpoints
    for (int j = bpt.numBreakpoints()-1; j >= 0; j--) {
	const Breakpoint& bp = bpt.breakpoint(j);
	if (bp.fileName == name) {
	    TRACE(QString().sprintf("updating %s:%d", bp.fileName.data(), bp.lineNo));
	    int i = bp.lineNo;
	    if (i < 0 || uint(i) >= m_lineItems.size())
		continue;
	    if (bp.enabled) {
		if (!(m_lineItems[i] & liBP)) {
		    m_lineItems[i] &= ~liBPany;
		    m_lineItems[i] |= liBP;
		    updateLineItem(i);
		}
	    } else {
		if (!(m_lineItems[i] & liBPdisabled)) {
		    m_lineItems[i] &= ~liBPany;
		    m_lineItems[i] |= liBPdisabled;
		    updateLineItem(i);
		}
	    }
	}
    }
}

void FileWindow::updateLineItem(int row)
{
    updateCell(row, 0);
}

void FileWindow::setPC(bool set, int lineNo, int frameNo)
{
    if (lineNo < 0 || lineNo >= int(m_lineItems.size())) {
	return;
    }
    uchar flag = frameNo == 0  ?  liPC  :  liPCup;
    if (set) {
	// set only if not already set
	if ((m_lineItems[lineNo] & flag) == 0) {
	    m_lineItems[lineNo] |= flag;
	    updateLineItem(lineNo);
	}
    } else {
	// clear only if not set
	if ((m_lineItems[lineNo] & flag) != 0) {
	    m_lineItems[lineNo] &= ~flag;
	    updateLineItem(lineNo);
	}
    }
}



WinStack::WinStack(QWidget* parent, const char* name) :
	QWidget(parent, name),
	m_activeWindow(0),
	m_windowMenu(0),
	m_itemMore(0),
	m_pcLine(-1)
{
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
    
    // find entry More...
    m_itemMore = menu->indexOf(ID_WINDOW_MORE);
    // must contain item More...
    ASSERT(m_itemMore >= 0);

    m_textMore = menu->text(ID_WINDOW_MORE);
    menu->removeItemAt(m_itemMore);
}

void WinStack::menuCallback(int item)
{
    TRACE(QString().sprintf("menu item=0x%x", item));
    // check for window
    if ((item & ~ID_WINDOW_INDEX_MASK) == ID_WINDOW_MORE) {
	selectWindow(item & ID_WINDOW_INDEX_MASK);
	return;
    }
    
    switch (item) {
    case ID_FILE_OPEN:
	openFile();
	break;
    }
}

void WinStack::openFile()
{
    QString fileName = KFileDialog::getOpenFileName();
    TRACE("openFile: " + fileName);
    if (fileName.isEmpty()) {
	return;
    }

    activate(fileName, 0);
}

bool WinStack::activate(QString fileName, int lineNo)
{
    // if this is not an absolute path name, make it one
    QFileInfo fi(fileName);
    if (!fi.isFile()) {
	return false;
    }
    return activatePath(fi.absFilePath(), lineNo);
}

bool WinStack::activatePath(QString pathName, int lineNo)
{
    // check whether the file is already open
    FileWindow* fw;
    for (fw = m_fileList.first(); fw != 0; fw = m_fileList.next()) {
	if (fw->fileName() == pathName) {
	    break;
	}
    }
    if (fw == 0) {
	// not found, load it
	fw = new FileWindow(pathName, this, "fileWindow");
	m_fileList.insert(0, fw);

	changeWindowMenu();
	
	// slurp the file in
	fw->loadFile();

	// set PC if there is one
	if (m_pcLine >= 0) {
	    setPC(true, m_pcFile, m_pcLine, m_pcFrame);
	}
    }
    return activateWindow(fw, lineNo);
}

bool WinStack::activateWindow(FileWindow* fw, int lineNo)
{
    int index = m_fileList.findRef(fw);
    ASSERT(index >= 0);
    if (index < 0) {
	return false;
    }
    /*
     * If the file is not in the list of those that would appear in the
     * window menu, move it to the first position.
     */
    if (index >= 9) {
	m_fileList.remove();
	m_fileList.insert(0, fw);
	changeWindowMenu();
    }

    // make the line visible
    if (lineNo >= 0) {
	fw->scrollTo(lineNo);
    }

    // first resize the window, then lift it to the top
    fw->setGeometry(0,0, width(),height());
    fw->raise();
    fw->show();

    // set the focus to the new active window
    QWidget* oldActive = m_activeWindow;
    fw->setFocusPolicy(QWidget::StrongFocus);
    m_activeWindow = fw;
    if (oldActive != 0 && oldActive != fw) {
	// disable focus on non-active windows
	oldActive->setFocusPolicy(QWidget::NoFocus);
    }
    fw->setFocus();
    return true;
}

bool WinStack::activeLine(QString& fileName, int& lineNo)
{
    if (m_activeWindow == 0) {
	return false;
    }
    
    fileName = m_activeWindow->fileName();
    int dummyCol;
    m_activeWindow->cursorPosition(&lineNo, &dummyCol);
    return true;
}

void WinStack::changeWindowMenu()
{
    if (m_windowMenu == 0) {
	return;
    }

    // delete window entries
    while ((m_windowMenu->idAt(m_itemMore) & ~ID_WINDOW_INDEX_MASK) == ID_WINDOW_MORE) {
	m_windowMenu->removeItemAt(m_itemMore);
    }
    
    // insert current windows
    QString text;
    int index = 1;
    FileWindow* fw = 0;
    for (fw = m_fileList.first(); fw != 0 && index < 10; fw = m_fileList.next()) {
	text.sprintf("&%d ", index);
	text += fw->fileName();
	m_windowMenu->insertItem(text, ID_WINDOW_MORE+index, m_itemMore+index-1);
	index++;
    }
    if (fw != 0) {
	// there are still windows
	m_windowMenu->insertItem(m_textMore, ID_WINDOW_MORE, m_itemMore+9);
    }
}

void WinStack::selectWindow(int id)
{
    if (id == 0) {
	TRACE("Window->More... not yet implemented");
    } else {
	FileWindow* fw = m_fileList.first();
	for (int index = (id & ID_WINDOW_INDEX_MASK)-1; index > 0; index--) {
	    fw = m_fileList.next();
	}
	KASSERT(fw != 0, KDEBUG_INFO, 0, "");
	
	activateWindow(fw, -1);
    }
}

void WinStack::updateLineItems(const BreakpointTable& bpt)
{
    FileWindow* fw = 0;
    for (fw = m_fileList.first(); fw != 0; fw = m_fileList.next()) {
	fw->updateLineItems(bpt);
    }
}

void WinStack::updatePC(const QString& fileName, int lineNo, int frameNo)
{
    if (m_pcLine >= 0) {
	setPC(false, m_pcFile, m_pcLine, m_pcFrame);
    }
    m_pcFile = fileName;
    m_pcLine = lineNo;
    m_pcFrame = frameNo;
    if (lineNo >= 0) {
	setPC(true, fileName, lineNo, frameNo);
    }
}

void WinStack::setPC(bool set, const QString& fileName, int lineNo, int frameNo)
{
    TRACE(QString(fileName.length()+60).sprintf("%s PC: %s:%d#%d", set ? "set" : "clear",
						fileName.data(), lineNo, frameNo));
    // find file
    FileWindow* fw = 0;
    for (fw = m_fileList.first(); fw != 0; fw = m_fileList.next()) {
	// get the simple name for this file
	const QString& name = fw->fileName();
	// either there is a slash, then skip it, or there is no slash,
	// then -1 + 1 = 0!
	int slash = name.findRev('/') + 1;
	if (name.data() + slash == fileName) {
	    fw->setPC(set, lineNo, frameNo);
	    break;
	}
    }
}

void WinStack::resizeEvent(QResizeEvent*)
{
    ASSERT(m_activeWindow == 0 || m_fileList.findRef(m_activeWindow) >= 0);
    if (m_activeWindow != 0) {
	m_activeWindow->resize(width(), height());
    }
}
