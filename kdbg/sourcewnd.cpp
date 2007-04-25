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
#include <kglobalsettings.h>
#include <iterator>
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
    m_pcinner = UserIcon("pcinner");
    m_pcup = UserIcon("pcup");
    m_brkena = UserIcon("brkena");
    m_brkdis = UserIcon("brkdis");
    m_brktmp = UserIcon("brktmp");
    m_brkcond = UserIcon("brkcond");
    m_brkorph = UserIcon("brkorph");
    setFont(KGlobalSettings::fixedFont());
}

SourceWindow::~SourceWindow()
{
}

bool SourceWindow::loadFile()
{
    // first we load the code into KTextView::m_texts
    QFile f(m_fileName);
    if (!f.open(IO_ReadOnly)) {
	return false;
    }

    bool upd = autoUpdate();
    setAutoUpdate(false);

    QTextStream t(&f);
    QString s;
    while (!t.eof()) {
	s = t.readLine();
	insertLine(s);
    }
    f.close();

    setAutoUpdate(upd);
    if (upd) {
	updateTableSize();
    }

    // then we copy it into our own m_sourceCode
    int n = m_texts.size();
    m_sourceCode.resize(n);
    m_rowToLine.resize(n);
    for (int i = 0; i < n; i++) {
	m_sourceCode[i].code = m_texts[i];
	m_rowToLine[i] = i;
    }
    m_lineItems.resize(n, 0);

    return true;
}

void SourceWindow::reloadFile()
{
    QFile f(m_fileName);
    if (!f.open(IO_ReadOnly)) {
	// open failed; leave alone
	return;
    }

    // read text into m_sourceCode
    m_sourceCode.clear();		/* clear old text */

    QTextStream t(&f);
    std::back_insert_iterator<std::vector<SourceLine> > it(m_sourceCode);
    SourceLine s;
    while (!t.eof()) {
	s.code = t.readLine();
	*it = s;
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
	for (size_t i = lineNo; i < m_sourceCode.size(); i++) {
	    insertLine(m_sourceCode[i].code);
	}
    } else {
	// the new file has fewer lines
	// here lineNo is the number of lines of the new file
	// remove the excessive lines
	m_texts.resize(lineNo);
    }
    // allocate line items
    m_lineItems.resize(m_sourceCode.size(), 0);

    setNumRows(m_sourceCode.size());

    m_rowToLine.resize(m_sourceCode.size());
    for (size_t i = 0; i < m_sourceCode.size(); i++)
	m_rowToLine[i] = i;

    setAutoUpdate(autoU);
    if (autoU && isVisible()) {
	updateTableSize();
	repaint();
    }

    // if the cursor is in the deleted lines, move it to the last line
    if (m_curRow >= int(m_texts.size())) {
	m_curRow = -1;			/* at this point don't have an active row */
	activateLine(m_texts.size()-1);	/* now we have */
    }
}

void SourceWindow::scrollTo(int lineNo, const DbgAddr& address)
{
    if (lineNo < 0 || lineNo >= int(m_sourceCode.size()))
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

int SourceWindow::cellWidth(int col) const
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
	if (item & liBPorphan) {
	    // orphaned breakpoint marker
	    int y = (h - m_brkcond.height())/2;
	    if (y < 0) y = 0;
	    p->drawPixmap(0,y,m_brkorph);
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
	    if (i < 0 || i >= int(m_sourceCode.size()))
		continue;
	    // compute new line item flags for breakpoint
	    uchar flags = bp->enabled ? liBP : liBPdisabled;
	    if (bp->temporary)
		flags |= liBPtemporary;
	    if (!bp->condition.isEmpty() || bp->ignoreCount != 0)
		flags |= liBPconditional;
	    if (bp->isOrphaned())
		flags |= liBPorphan;
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

static inline bool isident(QChar c)
{
    return c.isLetterOrNumber() || c.latin1() == '_';
}

bool SourceWindow::wordAtPoint(const QPoint& p, QString& word, QRect& r)
{
    int row, col  = charAt(p, &row);
    if (row < 0 || col < 0)
	return false;

    // isolate the word at row, col
    QString line = m_texts[row];
    if (!isident(line[col]))
	return false;

    int begin = col;
    while (begin > 0 && isident(line[begin-1]))
	--begin;
    do
	++col;
    while (col < int(line.length()) && isident(line[col]));

    r = QRect(p, p);
    r.addCoords(-5,-5,5,5);
    word = line.mid(begin, col-begin);
    return true;
}

void SourceWindow::paletteChange(const QPalette& oldPal)
{
    setFont(KGlobalSettings::fixedFont());
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
    if (lineNo < 0 || lineNo >= int(m_sourceCode.size()))
	return;

    SourceLine& sl = m_sourceCode[lineNo];

    // copy disassembled code and its addresses
    sl.disass.resize(disass.count());
    sl.disassAddr.resize(disass.count());
    sl.canDisass = disass.count() > 0;
    for (uint i = 0; i < disass.count(); i++) {
	const DisassembledCode* c =
	    const_cast<QList<DisassembledCode>&>(disass).at(i);
	QString code = c->code;
	while (code.endsWith("\n"))
	    code.truncate(code.length()-1);
	sl.disass[i] = c->address.asString() + ' ' + code;
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
    int line = row >= 0  ?  m_rowToLine[row]  :  -1;
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

    assert(line < int(m_rowToLine.size()));

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
	assert(h == int(m_rowToLine.size()) || m_rowToLine[l] < m_rowToLine[h]);

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
    return row < int(m_rowToLine.size())-1 &&
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
    const std::vector<QString>& disass = m_sourceCode[line].disass;

    // remove PC (must be set again in slot of signal expanded())
    m_lineItems[row] &= ~(liPC|liPCup);

    // insert new lines
    ++row;
    m_rowToLine.insert(m_rowToLine.begin()+row, disass.size(), line);
    m_lineItems.insert(m_lineItems.begin()+row, disass.size(), 0);
    m_texts.insert(m_texts.begin()+row, disass.begin(), disass.end());

    bool autoU = autoUpdate();
    setAutoUpdate(false);

    // update line widths
    for (size_t i = 0; i < disass.size(); i++) {
	updateCellSize(disass[i]);
    }

    setNumRows(m_texts.size());

    emit expanded(line);		/* must set PC */

    setAutoUpdate(autoU);
    if (autoU && isVisible()) {
	updateTableSize();
	update();
    }
}

void SourceWindow::collapseRow(int row)
{
    TRACE("collapsing row " + QString().setNum(row));
    int line = rowToLine(row);

    // find end of this block
    int end = row+1;
    while (end < int(m_rowToLine.size()) && m_rowToLine[end] == m_rowToLine[row]) {
	end++;
    }
    ++row;
    m_rowToLine.erase(m_rowToLine.begin()+row, m_rowToLine.begin()+end);
    m_lineItems.erase(m_lineItems.begin()+row, m_lineItems.begin()+end);
    m_texts.erase(m_texts.begin()+row, m_texts.begin()+end);

    bool autoU = autoUpdate();
    setAutoUpdate(false);

    setNumRows(m_texts.size());

    emit collapsed(line);

    setAutoUpdate(autoU);
    if (autoU && isVisible()) {
	updateTableSize();
	update();
    }
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

    for (size_t i = 0; i < disassAddr.size(); i++) {
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
