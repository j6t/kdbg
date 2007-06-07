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
#include <qpopupmenu.h>
#include <kapp.h>
#include <kiconloader.h>
#include <kglobalsettings.h>
#include <kmainwindow.h>
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include "mydebug.h"


SourceWindow::SourceWindow(const char* fileName, QWidget* parent, const char* name) :
	QTextEdit(parent, name),
	m_fileName(fileName),
	m_curRow(-1),
	m_widthItems(16),
	m_widthPlus(12)
{
    // load pixmaps
    m_pcinner = UserIcon("pcinner");
    m_pcup = UserIcon("pcup");
    m_brkena = UserIcon("brkena");
    m_brkdis = UserIcon("brkdis");
    m_brktmp = UserIcon("brktmp");
    m_brkcond = UserIcon("brkcond");
    m_brkorph = UserIcon("brkorph");
    setFont(KGlobalSettings::fixedFont());
    setReadOnly(true);
    setMargins(m_widthItems+m_widthPlus, 0, 0 ,0);
    setAutoFormatting(AutoNone);
    setTextFormat(PlainText);
    setWordWrap(NoWrap);
    connect(verticalScrollBar(), SIGNAL(valueChanged(int)),
	    this, SLOT(update()));
    connect(this, SIGNAL(cursorPositionChanged(int,int)), this, SLOT(cursorChanged(int)));
    viewport()->installEventFilter(this);

    // add a syntax highlighter
    if (QRegExp("\\.(c(pp|c|\\+\\+)?|CC?|h(\\+\\+|h)?|HH?)$").search(m_fileName))
    {
	new HighlightCpp(this);
    }
}

SourceWindow::~SourceWindow()
{
    delete syntaxHighlighter();
}

bool SourceWindow::loadFile()
{
    // first we load the code into QTextEdit
    QFile f(m_fileName);
    if (!f.open(IO_ReadOnly)) {
	return false;
    }

    QTextStream t(&f);
    setText(t.read());
    f.close();

    // then we copy it into our own m_sourceCode
    int n = paragraphs();
    m_sourceCode.resize(n);
    m_rowToLine.resize(n);
    for (int i = 0; i < n; i++) {
	m_sourceCode[i].code = text(i);
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
    setText(t.read());
    f.close();

    m_sourceCode.resize(paragraphs());
    for (size_t i = 0; i < m_sourceCode.size(); i++) {
	m_sourceCode[i].code = text(i);
    }
    // allocate line items
    m_lineItems.resize(m_sourceCode.size(), 0);

    m_rowToLine.resize(m_sourceCode.size());
    for (size_t i = 0; i < m_sourceCode.size(); i++)
	m_rowToLine[i] = i;
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
    setCursorPosition(row, 0);
    ensureCursorVisible();
}

void SourceWindow::drawFrame(QPainter* p)
{
    QTextEdit::drawFrame(p);

    // and paragraph at the top is...
    int top = paragraphAt(QPoint(0,contentsY()));
    int bot = paragraphAt(QPoint(0,contentsY()+visibleHeight()-1));
    if (bot < 0)
	bot = paragraphs()-1;

    p->save();

    // set a clip rectangle
    int fw = frameWidth();
    QRect inside = rect();
    inside.addCoords(fw,fw,-fw,-fw);
    QRegion clip = p->clipRegion();
    clip &= QRegion(inside);
    p->setClipRegion(clip);

    p->setPen(colorGroup().text());
    p->eraseRect(inside);

    for (int row = top; row <= bot; row++)
    {
	uchar item = m_lineItems[row];
	p->save();

	QRect r = paragraphRect(row);
	QPoint pt = contentsToViewport(r.topLeft());
	int h = r.height();
	p->translate(fw, pt.y()+viewport()->y());

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
	if (!isRowDisassCode(row) && m_sourceCode[rowToLine(row)].canDisass) {
	    int w = m_widthPlus;
	    p->translate(m_widthItems, 0);
	    int x = w/2;
	    int y = h/2;
	    p->drawLine(x-2, y, x+2, y);
	    if (!isRowExpanded(row)) {
		p->drawLine(x, y-2, x, y+2);
	    }
	}
	p->restore();
    }
    p->restore();
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
		update();
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
		update();
	    }
	}
    }
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
	    update();
	}
    } else {
	// clear only if not set
	if ((m_lineItems[row] & flag) != 0) {
	    m_lineItems[row] &= ~flag;
	    update();
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
    // we handle left and middle button
    if (ev->button() != LeftButton && ev->button() != MidButton)
    {
	QTextEdit::mousePressEvent(ev);
	return;
    }

    // get row
    QPoint p = viewportToContents(QPoint(0, ev->y() - viewport()->y()));
    int row = paragraphAt(p);
    if (row < 0)
	return;

    if (ev->x() > m_widthItems+frameWidth())
    {
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
    int top1, top2;
    QPoint top;
    switch (ev->key()) {
    case Key_Plus:
	actionExpandRow(m_curRow);
	return;
    case Key_Minus:
	actionCollapseRow(m_curRow);
	return;
    case Key_Up:
	if (m_curRow > 0) {
	    setCursorPosition(m_curRow-1, 0);
	}
	return;
    case Key_Down:
	if (m_curRow < paragraphs()-1) {
	    setCursorPosition(m_curRow+1, 0);
	}
	return;
    case Key_Home:
	setCursorPosition(0, 0);
	return;
    case Key_End:
	setCursorPosition(paragraphs()-1, 0);
	return;
    case Key_Next:
    case Key_Prior:
	top = viewportToContents(QPoint(0,0));
	top1 = paragraphAt(top);
    }

    QTextEdit::keyPressEvent(ev);

    switch (ev->key()) {
    case Key_Next:
    case Key_Prior:
	top = viewportToContents(QPoint(0,0));
	top2 = paragraphAt(top);
	setCursorPosition(m_curRow+(top2-top1), 0);
    }
}

static inline bool isident(QChar c)
{
    return c.isLetterOrNumber() || c.latin1() == '_';
}

bool SourceWindow::wordAtPoint(const QPoint& p, QString& word, QRect& r)
{
    QPoint pv = viewportToContents(p - viewport()->pos());
    int row, col  = charAt(pv, &row);
    if (row < 0 || col < 0)
	return false;

    // isolate the word at row, col
    QString line = text(row);
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
    QTextEdit::paletteChange(oldPal);
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
	update();
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
    return row > 0 && row < int(m_rowToLine.size()) &&
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

    // adjust current row
    if (m_curRow > row) {
	m_curRow += disass.size();
	// highlight is moved automatically
    }

    // insert new lines
    ++row;
    for (size_t i = 0; i < disass.size(); i++) {
	m_rowToLine.insert(m_rowToLine.begin()+row, line);
	m_lineItems.insert(m_lineItems.begin()+row, 0);
	insertParagraph(disass[i], row++);
    }
    update();		// line items

    emit expanded(line);		/* must set PC */
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
    // adjust current row
    if (m_curRow >= row) {
	m_curRow -= end-row;
	if (m_curRow < row)	// was m_curRow in disassembled code?
	    m_curRow = -1;
    }
    while (--end >= row) {
	m_rowToLine.erase(m_rowToLine.begin()+end);
	m_lineItems.erase(m_lineItems.begin()+end);
	removeParagraph(end);
    }
    update();		// line items

    emit collapsed(line);
}

void SourceWindow::activeLine(int& line, DbgAddr& address)
{
    int row = m_curRow;

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
    if (row < 0 || isRowExpanded(row) || isRowDisassCode(row))
	return;

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
    if (row < 0 || !isRowExpanded(row) || isRowDisassCode(row))
	return;

    collapseRow(row);
}

void SourceWindow::setTabWidth(int numChars)
{
    if (numChars <= 0)
	numChars = 8;
    QFontMetrics fm(currentFont());
    QString s;
    int w = fm.width(s.fill('x', numChars));
    setTabStopWidth(w);
}

void SourceWindow::cursorChanged(int row)
{
    if (row == m_curRow)
	return;

    if (m_curRow >= 0 && m_curRow < paragraphs())
	clearParagraphBackground(m_curRow);
    m_curRow = row;
    setParagraphBackgroundColor(row, colorGroup().background());

    emit lineChanged();
}

/*
 * We must override the context menu handling because QTextEdit's handling
 * requires that it receives ownership of the popup menu; but the popup menu
 * returned from the GUI factory is owned by the factory.
 */

void SourceWindow::contextMenuEvent(QContextMenuEvent* e)
{
    // get the context menu from the GUI factory
    QWidget* top = this;
    do
	top = top->parentWidget();
    while (!top->isTopLevel());
    KMainWindow* mw = static_cast<KMainWindow*>(top);
    QPopupMenu* m =
	static_cast<QPopupMenu*>(mw->factory()->container("popup_files", mw));
    m->exec(e->globalPos());
}

bool SourceWindow::eventFilter(QObject* watched, QEvent* e)
{
    if (e->type() == QEvent::ContextMenu && watched == viewport())
    {
	contextMenuEvent(static_cast<QContextMenuEvent*>(e));
	return true;
    }
    return QTextEdit::eventFilter(watched, e);
}

HighlightCpp::HighlightCpp(SourceWindow* srcWnd) :
	QSyntaxHighlighter(srcWnd),
	m_srcWnd(srcWnd)
{
}

enum HLState {
    hlCommentLine = 1,
    hlCommentBlock,
    hlString
};

int HighlightCpp::highlightParagraph(const QString& text, int state)
{
    int row = currentParagraph();
    // highlight assembly lines
    if (m_srcWnd->isRowDisassCode(row))
    {
	setFormat(0, text.length(), blue);
	return state;
    }

    if (state == -2)		// initial state
	state = 0;

    // check for preprocessor line
    if (state == 0 && text.stripWhiteSpace().startsWith("#"))
    {
	setFormat(0, text.length(), QColor("dark green"));
	return 0;
    }

    unsigned start = 0;
    while (start < text.length())
    {
	int end;
	switch (state) {
	case hlCommentLine:
	    end = text.length();
	    state = 0;
	    setFormat(start, end-start, QColor("gray50"));
	    break;
	case hlCommentBlock:
	    end = text.find("*/", start);
	    if (end >= 0)
		end += 2, state = 0;
	    else
		end = text.length();
	    setFormat(start, end-start, QColor("gray50"));
	    break;
	case hlString:
	    for (end = start+1; end < int(text.length()); end++) {
		if (text[end] == '\\') {
		    if (end < int(text.length()))
			++end;
		} else if (text[end] == text[start]) {
		    ++end;
		    break;
		}
	    }
	    state = 0;
	    setFormat(start, end-start, QColor("dark red"));
	    break;
	default:
	    for (end = start; end < int(text.length()); end++)
	    {
		if (text[end] == '/')
		{
		    if (end+1 < int(text.length())) {
			if (text[end+1] == '/') {
			    state = hlCommentLine;
			    break;
			} else if (text[end+1] == '*') {
			    state = hlCommentBlock;
			    break;
			}
		    }
		}
		else if (text[end] == '"' || text[end] == '\'')
		{
		    state = hlString;
		    break;
		}
	    }
	    setFormat(start, end-start, m_srcWnd->colorGroup().text());
	}
	start = end;
    }
    return state;
}

#include "sourcewnd.moc"
