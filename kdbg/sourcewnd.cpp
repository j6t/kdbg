// $Id$

// Copyright by Johannes Sixt
// This file is under GPL, the GNU General Public Licence

#include "debugger.h"
#include "sourcewnd.h"
#include <qtextstream.h>
#include <qpainter.h>
#include <qbrush.h>
#include <qfile.h>
#include <qkeycode.h>
#include <kapp.h>
#include <kiconloader.h>
#if QT_VERSION >= 200
#include <kglobal.h>
#else
#include <ctype.h>
#endif
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include "mydebug.h"


SourceWindow::SourceWindow(const char* fileName, QWidget* parent, const char* name) :
	KTextView(parent, name),
	m_fileName(fileName)
{
    setNumCols(3);

    // load pixmaps
#if QT_VERSION < 200
    KIconLoader* loader = kapp->getIconLoader();
    m_pcinner = loader->loadIcon("pcinner.xpm");
    m_pcup = loader->loadIcon("pcup.xpm");
    m_brkena = loader->loadIcon("brkena.xpm");
    m_brkdis = loader->loadIcon("brkdis.xpm");
    m_brktmp = loader->loadIcon("brktmp.xpm");
    m_brkcond = loader->loadIcon("brkcond.xpm");
    setFont(kapp->fixedFont);
#else
    m_pcinner = BarIcon("pcinner");
    m_pcup = BarIcon("pcup");
    m_brkena = BarIcon("brkena");
    m_brkdis = BarIcon("brkdis");
    m_brktmp = BarIcon("brktmp");
    m_brkcond = BarIcon("brkcond");
    setFont(KGlobal::fixedFont());
#endif
}

SourceWindow::~SourceWindow()
{
}

void SourceWindow::loadFile()
{
    // first we load the code into KTextView::m_texts
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

    // then we copy it into our own m_sourceCode
    m_sourceCode.setSize(m_texts.size());
    m_rowToLine.setSize(m_texts.size());
    m_lineItems.setSize(m_texts.size());
    for (int i = 0; i < m_texts.size(); i++) {
	m_sourceCode[i].code = m_texts[i];
	m_rowToLine[i] = i;
	m_lineItems[i] = 0;
    }
}

void SourceWindow::reloadFile()
{
    QFile f(m_fileName);
    if (!f.open(IO_ReadOnly)) {
	// open failed; leave alone
	return;
    }

    // read text into m_sourceCode
    m_sourceCode.setSize(0);		/* clear old text */

    QTextStream t(&f);
    SourceLine s;
    while (!t.eof()) {
	s.code = t.readLine();
	m_sourceCode.append(s);
    }
    f.close();

    bool autoU = autoUpdate();
    setAutoUpdate(false);

    int lineNo = QMIN(m_texts.size(), m_sourceCode.size());
    for (int i = 0; i < lineNo; i++) {
	replaceLine(i, m_sourceCode[i].code);
    }
    if (m_sourceCode.size() > m_texts.size()) {
	// the new file has more lines than the old one
	// here lineNo is the number of lines of the old file
	for (int i = lineNo; i < m_sourceCode.size(); i++) {
	    insertLine(m_sourceCode[i].code);
	}
	// allocate line items
	m_lineItems.setSize(m_texts.size());
	for (int i = m_texts.size()-1; i >= lineNo; i--) {
	    m_lineItems[i] = 0;
	}
    } else {
	// the new file has fewer lines
	// here lineNo is the number of lines of the new file
	// remove the excessive lines
	m_texts.setSize(lineNo);
	m_lineItems.setSize(lineNo);
	// if the cursor is in the deleted lines, move it to the last line
	if (m_curRow >= lineNo) {
	    m_curRow = -1;		/* at this point don't have an active row */
	    activateLine(lineNo-1);	/* now we have */
	}
    }
    f.close();

    setNumRows(m_texts.size());

    m_rowToLine.setSize(m_texts.size());
    for (int i = 0; i < m_texts.size(); i++)
	m_rowToLine[i] = i;

    setAutoUpdate(autoU);
    if (autoU && isVisible()) {
	repaint();
    }
}

void SourceWindow::scrollTo(int lineNo, const DbgAddr& address)
{
    if (lineNo < 0 || lineNo >= m_sourceCode.size())
	return;

    int row = lineToRow(lineNo, address);
    scrollToRow(row);
}

void SourceWindow::scrollToRow(int row)
{
    if (row >= numRows())
	row = numRows();

    // scroll to lineNo
    if (row >= topCell() && row <= lastRowVisible()) {
	// line is already visible
	setCursorPosition(row, 0);
	return;
    }

    // setCursorPosition does only a half-hearted job of making the
    // cursor line visible, so we do it by hand...
    setCursorPosition(row, 0);
    row -= 5;
    if (row < 0)
	row = 0;

    setTopCell(row);
}

int SourceWindow::textCol() const
{
    // text is in column 2
    return 2;
}

int SourceWindow::cellWidth(int col)
{
    if (col == 0) {
	return 15;
    } else if (col == 1) {
	return 12;
    } else {
	return KTextView::cellWidth(col);
    }
}

void SourceWindow::paintCell(QPainter* p, int row, int col)
{
    if (col == textCol()) {
	p->save();
	if (isRowDisassCode(row)) {
	    p->setPen(blue);
	}
	KTextView::paintCell(p, row, col);
	p->restore();
	return;
    }
    if (col == 0) {
	uchar item = m_lineItems[row];
	if (item == 0)			/* shortcut out */
	    return;
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
	if (item & liBPtemporary) {
	    // temporary breakpoint marker
	    int y = (h - m_brktmp.height())/2;
	    if (y < 0) y = 0;
	    p->drawPixmap(0,y,m_brktmp);
	}
	if (item & liBPconditional) {
	    // conditional breakpoint marker
	    int y = (h - m_brkcond.height())/2;
	    if (y < 0) y = 0;
	    p->drawPixmap(0,y,m_brkcond);
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
	return;
    }
    if (col == 1) {
	if (!isRowDisassCode(row) && m_sourceCode[rowToLine(row)].canDisass) {
	    int h = cellHeight(row);
	    int w = cellWidth(col);
	    int x = w/2;
	    int y = h/2;
	    p->drawLine(x-2, y, x+2, y);
	    if (!isRowExpanded(row)) {
		p->drawLine(x, y-2, x, y+2);
	    }
	}
    }
}

void SourceWindow::updateLineItems(const KDebugger* dbg)
{
    // clear outdated breakpoints
    for (int i = m_lineItems.size()-1; i >= 0; i--) {
	if (m_lineItems[i] & liBPany) {
	    // check if this breakpoint still exists
	    int line = rowToLine(i);
	    TRACE(QString().sprintf("checking for bp at %d", line));
	    int j;
	    for (j = dbg->numBreakpoints()-1; j >= 0; j--) {
		const Breakpoint* bp = dbg->breakpoint(j);
		if (bp->lineNo == line &&
		    fileNameMatches(bp->fileName) &&
		    lineToRow(line, bp->address) == i)
		{
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
    for (int j = dbg->numBreakpoints()-1; j >= 0; j--) {
	const Breakpoint* bp = dbg->breakpoint(j);
	if (fileNameMatches(bp->fileName)) {
	    TRACE(QString().sprintf("updating %s:%d", bp->fileName.data(), bp->lineNo));
	    int i = bp->lineNo;
	    if (i < 0 || i >= m_sourceCode.size())
		continue;
	    // compute new line item flags for breakpoint
	    uchar flags = bp->enabled ? liBP : liBPdisabled;
	    if (bp->temporary)
		flags |= liBPtemporary;
	    if (!bp->condition.isEmpty() || bp->ignoreCount != 0)
		flags |= liBPconditional;
	    // update if changed
	    int row = lineToRow(i, bp->address);
	    if ((m_lineItems[row] & liBPany) != flags) {
		m_lineItems[row] &= ~liBPany;
		m_lineItems[row] |= flags;
		updateLineItem(row);
	    }
	}
    }
}

void SourceWindow::updateLineItem(int row)
{
    updateCell(row, 0);
}

void SourceWindow::setPC(bool set, int lineNo, const DbgAddr& address, int frameNo)
{
    if (lineNo < 0 || lineNo >= int(m_sourceCode.size())) {
	return;
    }

    int row = lineToRow(lineNo, address);

    uchar flag = frameNo == 0  ?  liPC  :  liPCup;
    if (set) {
	// set only if not already set
	if ((m_lineItems[row] & flag) == 0) {
	    m_lineItems[row] |= flag;
	    updateLineItem(row);
	}
    } else {
	// clear only if not set
	if ((m_lineItems[row] & flag) != 0) {
	    m_lineItems[row] &= ~flag;
	    updateLineItem(row);
	}
    }
}

void SourceWindow::find(const QString& text, bool caseSensitive, FindDirection dir)
{
    ASSERT(dir == 1 || dir == -1);
    if (m_sourceCode.size() == 0 || text.isEmpty())
	return;

    int line;
    DbgAddr dummyAddr;
    activeLine(line, dummyAddr);
    if (line < 0)
	line = 0;
    int curLine = line;			/* remember where we started */
    bool found = false;
    do {
	// advance and wrap around
	line += dir;
	if (line < 0)
	    line = m_sourceCode.size()-1;
	else if (line >= int(m_sourceCode.size()))
	    line = 0;
	// search text
	found = m_sourceCode[line].code.find(text, 0, caseSensitive) >= 0;
    } while (!found && line != curLine);

    scrollTo(line, DbgAddr());
}

void SourceWindow::mousePressEvent(QMouseEvent* ev)
{
    // Check if right button was clicked.
    if (ev->button() == RightButton)
    {
	emit clickedRight(ev->pos());
	return;
    }

    int col = findCol(ev->x());

    // check if event is in line item column
    if (col == textCol()) {
	KTextView::mousePressEvent(ev);
	return;
    }

    // get row
    int row = findRow(ev->y());
    if (row < 0 || col < 0)
	return;

    if (col == 1) {
	if (isRowExpanded(row)) {
	    actionCollapseRow(row);
	} else {
	    actionExpandRow(row);
	}
	return;
    }

    int sourceRow;
    int line = rowToLine(row, &sourceRow);

    // find address if row is disassembled code
    DbgAddr address;
    if (row > sourceRow) {
	// get offset from source code line
	int off = row - sourceRow;
	address = m_sourceCode[line].disassAddr[off-1];
    }

    switch (ev->button()) {
    case LeftButton:
	TRACE(QString().sprintf("left-clicked line %d", line));
	emit clickedLeft(m_fileName, line, address,
			 (ev->state() & ShiftButton) != 0);
	break;
    case MidButton:
	TRACE(QString().sprintf("mid-clicked row %d", line));
	emit clickedMid(m_fileName, line, address);
	break;
    default:;
    }
}

void SourceWindow::keyPressEvent(QKeyEvent* ev)
{
    switch (ev->key()) {
    case Key_Plus:
	actionExpandRow(m_curRow);
	return;
    case Key_Minus:
	actionCollapseRow(m_curRow);
	return;
    }

    KTextView::keyPressEvent(ev);
}

#if QT_VERSION < 200
#define ISIDENT(c) (isalnum((c)) || (c) == '_')
#else
#define ISIDENT(c) ((c).isLetterOrNumber() || (c) == '_')
#endif

bool SourceWindow::wordAtPoint(const QPoint& p, QString& word, QRect& r)
{
    if (findCol(p.x()) != textCol())
	return false;
    int row = findRow(p.y());
    if (row < 0)
	return false;

    // find top-left corner of text cell
    int top, left;
    if (!colXPos(textCol(), &left))
	return false;
    if (!rowYPos(row, &top))
	return false;

    // get the bounding rect of the text
    QPainter painter(this);
    const QString& text = m_texts[row];
    QRect bound =
	painter.boundingRect(left+2, top, 0,0,
			     AlignLeft | SingleLine | DontClip | ExpandTabs,
			     text, text.length());
    if (!bound.contains(p))
	return false;			/* p is outside text */

    /*
     * We split the line into words and check each whether it contains
     * the point p. We must do it the hard way (measuring substrings)
     * because we cannot rely on that the font is mono-spaced.
     */
    uint start = 0;
    while (start < text.length()) {
	while (start < text.length() && !ISIDENT(text[start]))
	    start++;
	if (start >= text.length())
	    return false;
	/*
	 * If p is in the rectangle that ends at 'start', it is in the
	 * last non-word part that we have just skipped.
	 */
	bound = 
	    painter.boundingRect(left+2, top, 0,0,
				 AlignLeft | SingleLine | DontClip | ExpandTabs,
				 text, start);
	if (bound.contains(p))
	    return false;
	// a word starts now
	int startWidth = bound.width();
	uint end = start;
	while (end < text.length() && ISIDENT(text[end]))
	    end++;
	bound =
	    painter.boundingRect(left+2, top, 0,0,
				 AlignLeft | SingleLine | DontClip | ExpandTabs,
				 text, end);
	if (bound.contains(p)) {
	    // we found a word!
	    // extract the word
	    word = text.mid(start, end-start);
	    // and the rectangle
	    r = QRect(bound.x()+startWidth,bound.y(),
		      bound.width()-startWidth, bound.height());
	    return true;
	}
	start = end;
    }
    return false;
}

void SourceWindow::paletteChange(const QPalette& oldPal)
{
#if QT_VERSION < 200
    setFont(kapp->fixedFont);
#else
    setFont(KGlobal::fixedFont());
#endif
    KTextView::paletteChange(oldPal);
}

/*
 * Two file names (possibly full paths) match if the last parts - the file
 * names - match.
 */
bool SourceWindow::fileNameMatches(const QString& other)
{
    const QString& me = fileName();

    // check for null file names first
    if (me.isNull() || other.isNull()) {
	return me.isNull() && other.isNull();
    }

    /*
     * Get file names.  Note: Either there is a slash, then skip it, or
     * there is no slash, then -1 + 1 = 0!
     */
    int sme = me.findRev('/') + 1;
    int sother = other.findRev('/') + 1;
    return strcmp(me.data() + sme, other.data() + sother) == 0;
}

void SourceWindow::disassembled(int lineNo, const QList<DisassembledCode>& disass)
{
    TRACE("disassembled line " + QString().setNum(lineNo));
    if (lineNo < 0 || lineNo >= m_sourceCode.size())
	return;

    SourceLine& sl = m_sourceCode[lineNo];

    // copy disassembled code and its addresses
    sl.disass.setSize(disass.count());
    sl.disassAddr.setSize(disass.count());
    sl.canDisass = disass.count() > 0;
    for (uint i = 0; i < disass.count(); i++) {
	const DisassembledCode* c =
	    const_cast<QList<DisassembledCode>&>(disass).at(i);
	sl.disass[i] = c->address.asString() + ' ' + c->code;
	sl.disassAddr[i] = c->address;
    }

    int row = lineToRow(lineNo);
    if (sl.canDisass) {
	expandRow(row);
    } else {
	// clear expansion marker
	updateCell(row, 1);
    }
}

int SourceWindow::rowToLine(int row, int* sourceRow)
{
    int line = m_rowToLine[row];
    if (sourceRow != 0) {
	// search back until we hit the first entry with the current line number
	while (row > 0 && m_rowToLine[row-1] == line)
	    row--;
	*sourceRow = row;
    }
    return line;
}

/*
 * Rows showing diassembled code have the same line number as the
 * corresponding source code line number. Therefore, the line numbers in
 * m_rowToLine are monotonically increasing with blocks of equal line
 * numbers for a source line and its disassembled code that follows it.
 *
 * Hence, m_rowToLine always obeys the following condition:
 *
 *    m_rowToLine[i] <= i
 */

int SourceWindow::lineToRow(int line)
{
    // line is zero-based!

    assert(line < m_rowToLine.size());

    // quick test for common case
    if (line < 0 || m_rowToLine[line] == line)
	return line;

    assert(m_rowToLine[line] < line);

    /*
     * Binary search between row == line and end of list. In the loop below
     * we use the fact that the line numbers m_rowToLine do not contain
     * holes.
     */
    int l = line;
    int h = m_rowToLine.size();
    while (l < h && m_rowToLine[l] != line)
    {
	assert(h == m_rowToLine.size() || m_rowToLine[l] < m_rowToLine[h]);

	/*
	 * We want to round down the midpoint so that we find the
	 * lowest row that belongs to the line we seek.
	 */
	int mid = (l+h)/2;
	if (m_rowToLine[mid] <= line)
	    l = mid;
	else
	    h = mid;
    }
    // Found! Result is in l:
    assert(m_rowToLine[l] == line);

    /*
     * We might not have hit the lowest index for the line.
     */
    while (l > 0 && m_rowToLine[l-1] == line)
	--l;

    return l;
}

int SourceWindow::lineToRow(int line, const DbgAddr& address)
{
    int row = lineToRow(line);
    if (isRowExpanded(row)) {
	row += m_sourceCode[line].findAddressRowOffset(address);
    }
    return row;
}

bool SourceWindow::isRowExpanded(int row)
{
    assert(row >= 0);
    return row < m_rowToLine.size()-1 &&
	m_rowToLine[row] == m_rowToLine[row+1];
}

bool SourceWindow::isRowDisassCode(int row)
{
    return row > 0 &&
	m_rowToLine[row] == m_rowToLine[row-1];
}

void SourceWindow::expandRow(int row)
{
    TRACE("expanding row " + QString().setNum(row));
    // get disassembled code
    int line = rowToLine(row);
    const ValArray<QString>& disass = m_sourceCode[line].disass;

    // remove PC (must be set again in slot of signal expanded())
    m_lineItems[row] &= ~(liPC|liPCup);

    // insert new lines
    ++row;
    m_rowToLine.insertAt(row, line, disass.size());
    m_lineItems.insertAt(row, 0, disass.size());
    m_texts.insertAt(row, disass);

    bool autoU = autoUpdate();
    setAutoUpdate(false);

    // update line widths
    for (int i = 0; i < disass.size(); i++) {
	updateCellSize(disass[i]);
    }

    setNumRows(m_texts.size());

    emit expanded(line);		/* must set PC */

    setAutoUpdate(autoU);
    if (autoU && isVisible())
	update();
}

void SourceWindow::collapseRow(int row)
{
    TRACE("collapsing row " + QString().setNum(row));
    int line = rowToLine(row);

    // find end of this block
    int end = row+1;
    while (end < m_rowToLine.size() && m_rowToLine[end] == m_rowToLine[row]) {
	end++;
    }
    ++row;
    m_rowToLine.removeAt(row, end-row);
    m_lineItems.removeAt(row, end-row);
    m_texts.removeAt(row, end-row);

    bool autoU = autoUpdate();
    setAutoUpdate(false);

    setNumRows(m_texts.size());

    emit collapsed(line);

    setAutoUpdate(autoU);
    if (autoU && isVisible())
	update();
}

void SourceWindow::activeLine(int& line, DbgAddr& address)
{
    int row, col;
    cursorPosition(&row, &col);

    int sourceRow;
    line = rowToLine(row, &sourceRow);
    if (row > sourceRow) {
	int off = row - sourceRow;	/* offset from source line */
	address = m_sourceCode[line].disassAddr[off-1];
    }
}

/**
 * Returns the offset from the line displaying the source code to
 * the line containing the specified address. If the address is not
 * found, 0 is returned.
 */
int SourceWindow::SourceLine::findAddressRowOffset(const DbgAddr& address) const
{
    if (address.isEmpty())
	return 0;

    for (int i = 0; i < disassAddr.size(); i++) {
	if (disassAddr[i] == address) {
	    // found exact address
	    return i+1;
	}
	if (disassAddr[i] > address) {
	    /*
	     * We have already advanced too far; the address is before this
	     * index, but obviously we haven't found an exact match
	     * earlier. address is somewhere between the displayed
	     * addresses. We return the previous line.
	     */
	    return i;
	}
    }
    // not found
    return 0;
}

void SourceWindow::actionExpandRow(int row)
{
    if (isRowExpanded(row) || isRowDisassCode(row)) {
	// do nothing
	return;
    }

    // disassemble
    int line = rowToLine(row);
    const SourceLine& sl = m_sourceCode[line];
    if (!sl.canDisass)
	return;
    if (sl.disass.size() == 0) {
	emit disassemble(m_fileName, line);
    } else {
	expandRow(row);
    }
}

void SourceWindow::actionCollapseRow(int row)
{
    if (!isRowExpanded(row) || isRowDisassCode(row)) {
	// do nothing
	return;
    }

    collapseRow(row);
}


#include "sourcewnd.moc"