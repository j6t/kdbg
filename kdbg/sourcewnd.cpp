/*
 * Copyright Johannes Sixt
 * This file is licensed under the GNU General Public License Version 2.
 * See the file COPYING in the toplevel directory of the source directory.
 */

#include "debugger.h"
#include "sourcewnd.h"
#include "dbgdriver.h"
#include <QTextStream>
#include <QPainter>
#include <QFile>
#include <QFileInfo>
#include <QFontDatabase>
#include <QMenu>
#include <QContextMenuEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QRegularExpression>
#include <QTimer>
#include <kiconloader.h>
#include <kxmlguiwindow.h>
#include <kxmlguifactory.h>
#include <algorithm>
#include "mydebug.h"


SourceWindow::SourceWindow(const QString& fileName, QWidget* parent) :
	QPlainTextEdit(parent),
	m_fileName(fileName),
	m_widthItems(16),
	m_widthPlus(12),
	m_widthLineNo(30),
	m_lineInfoArea(new LineInfoArea(this))
{
    // Center the cursor when moving it into view
    setCenterOnScroll(true);

    // load pixmaps
    m_pcinner = KIconLoader::global()->loadIcon("pcinner", KIconLoader::User);
    m_pcup = KIconLoader::global()->loadIcon("pcup", KIconLoader::User);
    m_brkena = KIconLoader::global()->loadIcon("brkena", KIconLoader::User);
    m_brkdis = KIconLoader::global()->loadIcon("brkdis", KIconLoader::User);
    m_brktmp = KIconLoader::global()->loadIcon("brktmp", KIconLoader::User);
    m_brkcond = KIconLoader::global()->loadIcon("brkcond", KIconLoader::User);
    m_brkorph = KIconLoader::global()->loadIcon("brkorph", KIconLoader::User);
    setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    setReadOnly(true);
    setViewportMargins(lineInfoAreaWidth(), 0, 0 ,0);
    setWordWrapMode(QTextOption::NoWrap);
    connect(this, SIGNAL(updateRequest(const QRect&, int)),
	    m_lineInfoArea, SLOT(update()));
    connect(this, SIGNAL(cursorPositionChanged()), this, SLOT(cursorChanged()));

    // add a syntax highlighter
    if (QRegularExpression("\\.(c(pp|c|\\+\\+)?|CC?|h(\\+\\+|h|pp)?|HH?)$").match(m_fileName).hasMatch())
    {
	m_highlighter = new HighlightCpp(this);
    }
}

SourceWindow::~SourceWindow()
{
    delete m_highlighter;
}

int SourceWindow::lineInfoAreaWidth() const
{
    return 3 + m_widthItems + m_widthPlus + m_widthLineNo;
}

bool SourceWindow::loadFile()
{
    QFile f(m_fileName);
    if (!f.open(QIODevice::ReadOnly)) {
	return false;
    }

    QTextStream t(&f);
    setPlainText(t.readAll());
    f.close();

    int n = blockCount();
    m_sourceCode.resize(n);
    m_rowToLine.resize(n);
    for (int i = 0; i < n; i++) {
	m_rowToLine[i] = i;
    }
    m_lineItems.resize(n, 0);

    // set a font for line numbers
    m_lineNoFont = currentCharFormat().font();
    m_lineNoFont.setPixelSize(11);

    return true;
}

void SourceWindow::reloadFile()
{
    QFile f(m_fileName);
    if (!f.open(QIODevice::ReadOnly)) {
	// open failed; leave alone
	return;
    }

    m_sourceCode.clear();		/* clear old text */

    QTextStream t(&f);
    setPlainText(t.readAll());
    f.close();

    m_sourceCode.resize(blockCount());
    // expanded lines are collapsed: move existing line items up
    for (size_t i = 0; i < m_lineItems.size(); i++) {
	if (m_rowToLine[i] != int(i)) {
	    m_lineItems[m_rowToLine[i]] |= m_lineItems[i];
	    m_lineItems[i] = 0;
	}
    }
    // allocate line items
    m_lineItems.resize(m_sourceCode.size(), 0);

    m_rowToLine.resize(m_sourceCode.size());
    for (size_t i = 0; i < m_sourceCode.size(); i++)
	m_rowToLine[i] = i;

    // Highlighting was applied above when the text was inserted into widget,
    // but at that time m_rowToLine was not corrected, yet, so that lines
    // that previously were assembly were painted incorrectly.
    if (m_highlighter)
	m_highlighter->rehighlight();

    restorePrevDisass();
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
    QTextCursor cursor(document()->findBlockByNumber(row));
    setTextCursor(cursor);
    ensureCursorVisible();
}

void SourceWindow::resizeEvent(QResizeEvent* e)
{
    QPlainTextEdit::resizeEvent(e);

    QRect cr = contentsRect();
    cr.setRight(lineInfoAreaWidth());
    m_lineInfoArea->setGeometry(cr);
}

void SourceWindow::drawLineInfoArea(QPainter* p, QPaintEvent* event)
{
    QTextBlock block = firstVisibleBlock();

    p->setFont(m_lineNoFont);

    for (; block.isValid(); block = block.next())
    {
	if (!block.isVisible())
	    continue;

	QRect r = blockBoundingGeometry(block).translated(contentOffset()).toRect();
	if (r.bottom() < event->rect().top())
	    continue; // skip blocks that are higher than the region being updated
	else if (r.top() > event->rect().bottom())
	    break;    // all the following blocks are lower then the region being updated

	int row = block.blockNumber();
	uchar item = m_lineItems[row];

	int h = r.height();
	p->save();
	p->translate(0, r.top());

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
	p->translate(m_widthItems, 0);
	if (!isRowDisassCode(row) && m_sourceCode[rowToLine(row)].canDisass) {
	    int w = m_widthPlus;
	    int x = w/2;
	    int y = h/2;
	    p->drawLine(x-2, y, x+2, y);
	    if (!isRowExpanded(row)) {
		p->drawLine(x, y-2, x, y+2);
	    }
	}
	p->translate(m_widthPlus, 0);
	if (!isRowDisassCode(row)) {
	    p->drawText(0, 0, m_widthLineNo, h, Qt::AlignRight|Qt::AlignVCenter,
			QString().setNum(rowToLine(row)+1));
	}
	p->restore();
    }
}

void SourceWindow::updateLineItems(const KDebugger* dbg)
{
    // clear outdated breakpoints
    for (int i = m_lineItems.size()-1; i >= 0; i--) {
	if (m_lineItems[i] & liBPany) {
	    // check if this breakpoint still exists
	    int line = rowToLine(i);
	    TRACE(QString::asprintf("checking for bp at %d", line));
	    KDebugger::BrkptROIterator bp = dbg->breakpointsBegin();
	    for (; bp != dbg->breakpointsEnd(); ++bp)
	    {
		if (bp->lineNo == line &&
		    fileNameMatches(bp->fileName) &&
		    lineToRow(line, bp->address) == i)
		{
		    // yes it exists; mode is changed below
		    break;
		}
	    }
	    if (bp == dbg->breakpointsEnd()) {
		/* doesn't exist anymore, remove it */
		m_lineItems[i] &= ~liBPany;
	    }
	}
    }

    // add new breakpoints
    for (KDebugger::BrkptROIterator bp = dbg->breakpointsBegin(); bp != dbg->breakpointsEnd(); ++bp)
    {
	if (fileNameMatches(bp->fileName)) {
	    TRACE(QString("updating %2:%1").arg(bp->lineNo).arg(bp->fileName));
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
	    }
	}
    }
    m_lineInfoArea->update();
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
	    m_lineInfoArea->update();
	}
    } else {
	// clear only if not set
	if ((m_lineItems[row] & flag) != 0) {
	    m_lineItems[row] &= ~flag;
	    m_lineInfoArea->update();
	}
    }
}

void SourceWindow::find(const QString& text, bool caseSensitive, FindDirection dir)
{
    ASSERT(dir == 1 || dir == -1);
    QTextDocument::FindFlags flags;
    if (caseSensitive)
	flags |= QTextDocument::FindCaseSensitively;
    if (dir < 0)
	flags |= QTextDocument::FindBackward;
    if (QPlainTextEdit::find(text, flags))
	return;
    // not found; wrap around
    QTextCursor cursor(document());
    if (dir < 0)
	cursor.movePosition(QTextCursor::End);
    cursor = document()->find(text, cursor, flags);
    if (!cursor.isNull())
	setTextCursor(cursor);
}

void SourceWindow::infoMousePress(QMouseEvent* ev)
{
    // we handle left and middle button
    if (ev->button() != Qt::LeftButton && ev->button() != Qt::MiddleButton)
    {
	return;
    }

    // get row
    int row = cursorForPosition(QPoint(0, ev->y())).blockNumber();
    if (row < 0)
	return;

    if (ev->x() > m_widthItems)
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
    case Qt::LeftButton:
	TRACE(QString::asprintf("left-clicked line %d", line));
	Q_EMIT clickedLeft(m_fileName, line, address,
 			 (ev->modifiers() & Qt::ShiftModifier) != 0);
	break;
    case Qt::MiddleButton:
	TRACE(QString::asprintf("mid-clicked row %d", line));
	Q_EMIT clickedMid(m_fileName, line, address);
	break;
    default:;
    }
}

void SourceWindow::keyPressEvent(QKeyEvent* ev)
{
    int top1 = 0, top2;
    switch (ev->key()) {
    case Qt::Key_Plus:
	actionExpandRow(textCursor().blockNumber());
	return;
    case Qt::Key_Minus:
	actionCollapseRow(textCursor().blockNumber());
	return;
    case Qt::Key_Up:
    case Qt::Key_K:
	moveCursor(QTextCursor::PreviousBlock);
	return;
    case Qt::Key_Down:
    case Qt::Key_J:
	moveCursor(QTextCursor::NextBlock);
	return;
    case Qt::Key_Home:
	moveCursor(QTextCursor::Start);
	return;
    case Qt::Key_End:
	moveCursor(QTextCursor::End);
	return;
    case Qt::Key_PageUp:
    case Qt::Key_PageDown:
	top1 = firstVisibleBlock().blockNumber();
    }

    QPlainTextEdit::keyPressEvent(ev);

    switch (ev->key()) {
    case Qt::Key_PageUp:
    case Qt::Key_PageDown:
	top2 = firstVisibleBlock().blockNumber();
	{
	    QTextCursor cursor = textCursor();
	    if (top1 < top2)
		cursor.movePosition(QTextCursor::NextBlock,
				    QTextCursor::MoveAnchor, top2-top1);
	    else
		cursor.movePosition(QTextCursor::PreviousBlock,
				    QTextCursor::MoveAnchor, top1-top2);
	    setTextCursor(cursor);
	}
    }
}

QString SourceWindow::extendExpr(const QString &plainText,
                                 int            wordStart,
                                 int            wordEnd)
{
    QString document = plainText.left(wordEnd);
    QString word     = plainText.mid(wordStart, wordEnd - wordStart);
    QRegularExpression regex("(::)?([A-Za-z_]{1}\\w*\\s*(->|\\.|::)\\s*)*" + word + "$");

    #define IDENTIFIER_MAX_SIZE 256
    // cut the document to reduce size of string to scan
    // because of this only identifiefs of length <= IDENTIFIER_MAX_SIZE are supported
    if (document.length() > IDENTIFIER_MAX_SIZE) {
	document = document.right(IDENTIFIER_MAX_SIZE);
    }

    auto res = regex.match(document);

    if (!res.hasMatch())
    {
        TRACE("No match, returning " + word);
    }
    else
    {
	word = res.captured();
        TRACE("Matched, returning " + word);
    }

    return word;
}

bool SourceWindow::wordAtPoint(const QPoint& p, QString& word, QRect& r)
{
    QTextCursor cursor = cursorForPosition(viewport()->mapFrom(this, p));
    if (cursor.isNull())
	return false;

    cursor.select(QTextCursor::WordUnderCursor);
    word = cursor.selectedText();

    if (word.isEmpty())
	return false;

    // keep only letters and digits
    QRegularExpression w("[A-Za-z_]{1}[\\dA-Za-z_]*");
    auto res = w.match(word);
    if (!res.hasMatch())
	return false;

    word = res.captured();

    if (m_highlighter)
    {
        // if cpp highlighter is enabled - c/c++ file is being displayed

        // check that word is not a c/c++ keyword
        if (m_highlighter->isCppKeyword(word))
            return false;

        // TODO check that cursor is on top of a string literal
        //      and don't display any tooltips in this case

        // try to extend selected word under the cursor to get a full variable name
        word = extendExpr(cursor.document()->toPlainText(),
                          cursor.selectionStart(),
                          cursor.selectionEnd());

        if (word.isEmpty())
            return false;
    }

    r = QRect(p, p);
    r.adjust(-5,-5,5,5);
    return true;
}

void SourceWindow::changeEvent(QEvent* ev)
{
    switch (ev->type()) {
    case QEvent::ApplicationFontChange:
    case QEvent::FontChange:
	setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
	break;
    default:
	break;
    }
    QPlainTextEdit::changeEvent(ev);
}

/*
 * Two file names (possibly full paths) match if the last parts - the file
 * names - match.
 */
bool SourceWindow::fileNameMatches(const QString& other)
{
    return QFileInfo(other).fileName() == QFileInfo(m_fileName).fileName();
}

void SourceWindow::disassembled(int lineNo, const std::list<DisassembledCode>& disass)
{
    TRACE("disassembled line " + QString().setNum(lineNo));
    if (lineNo < 0 || lineNo >= int(m_sourceCode.size()))
	return;

    SourceLine& sl = m_sourceCode[lineNo];

    // copy disassembled code and its addresses
    sl.disass.resize(disass.size());
    sl.disassAddr.resize(disass.size());
    sl.canDisass = !disass.empty();
    int i = 0;
    for (std::list<DisassembledCode>::const_iterator c = disass.begin(); c != disass.end(); ++c, ++i)
    {
	QString code = c->code;
	while (code.endsWith(QLatin1Char('\n')))
	    code.truncate(code.length()-1);
	sl.disass[i] = c->address.asString() + QLatin1Char(' ') + code;
	sl.disassAddr[i] = c->address;
    }

    int row = lineToRow(lineNo);
    if (sl.canDisass) {
	expandRow(row);
    } else {
	// clear expansion marker
	m_lineInfoArea->update();
    }
}

int SourceWindow::rowToLine(int row, int* sourceRow)
{
    int line = row >= 0  ?  m_rowToLine[row]  :  -1;
    if (sourceRow) {
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

    // insert new lines
    setUpdatesEnabled(false);
    ++row;
    m_rowToLine.insert(m_rowToLine.begin()+row, disass.size(), line);
    m_lineItems.insert(m_lineItems.begin()+row, disass.size(), 0);

    QTextCursor cursor(document()->findBlockByNumber(row));
    for (size_t i = 0; i < disass.size(); i++) {
	cursor.insertText(disass[i]);
	cursor.insertBlock();
    }
    setUpdatesEnabled(true);

    registerExpandedLine(line);

    Q_EMIT expanded(line);		/* must set PC */
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
    setUpdatesEnabled(false);
    QTextCursor cursor(document()->findBlockByNumber(end-1));
    while (--end >= row) {
	m_rowToLine.erase(m_rowToLine.begin()+end);
	m_lineItems.erase(m_lineItems.begin()+end);
	cursor.select(QTextCursor::BlockUnderCursor);
	cursor.removeSelectedText();
    }
    setUpdatesEnabled(true);

    unregisterExpandedLine(line);

    Q_EMIT collapsed(line);
}

void SourceWindow::activeLine(int& line, DbgAddr& address)
{
    int row = textCursor().blockNumber();

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
	Q_EMIT disassemble(m_fileName, line);
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
    QFontMetrics fm(document()->defaultFont());
    QString s;
    int w = fm.horizontalAdvance(s.fill(QLatin1Char('x'), numChars));
    setTabStopDistance(w);
}

void SourceWindow::registerExpandedLine(int line)
{
    m_expandedLines.push_back(line);
}

void SourceWindow::unregisterExpandedLine(int line)
{
    m_expandedLines.erase(std::remove(m_expandedLines.begin(), m_expandedLines.end(),line), m_expandedLines.end());
}

void SourceWindow::restorePrevDisass()
{
    for(auto lineNo : m_expandedLines)
    {
        QTimer::singleShot(500, [this, lineNo](){actionExpandRow(lineNo);});
    }
}

QColor SourceWindow::lineSelectionColor() const
{
    QColor c = palette().color(QPalette::Text);

    if (c.lightness() < 100)	// Breeze light 24, Breeze dark 240
	return "#E7E7E7";
    else
	return "#191919";
}

void SourceWindow::cursorChanged()
{
    QList<QTextEdit::ExtraSelection> extraSelections;
    QTextEdit::ExtraSelection selection;

    selection.format.setBackground(lineSelectionColor());
    selection.format.setProperty(QTextFormat::FullWidthSelection, true);
    selection.cursor = textCursor();
    selection.cursor.clearSelection();
    extraSelections.append(selection);
    setExtraSelections(extraSelections);
}

/*
 * Show our own context menu.
 */
void SourceWindow::contextMenuEvent(QContextMenuEvent* e)
{
    // get the context menu from the GUI factory
    QWidget* top = this;
    do
	top = top->parentWidget();
    while (!top->isWindow());
    KXmlGuiWindow* mw = static_cast<KXmlGuiWindow*>(top);
    QMenu* m = static_cast<QMenu*>(mw->factory()->container("popup_files", mw));
    if (m)
	m->exec(e->globalPos());
}

void LineInfoArea::paintEvent(QPaintEvent* e)
{
    QPainter p(this);
    static_cast<SourceWindow*>(parent())->drawLineInfoArea(&p, e);
}

void LineInfoArea::mousePressEvent(QMouseEvent* ev)
{
    static_cast<SourceWindow*>(parent())->infoMousePress(ev);
}

void LineInfoArea::contextMenuEvent(QContextMenuEvent* e)
{
    static_cast<SourceWindow*>(parent())->contextMenuEvent(e);
}

HighlightCpp::HighlightCpp(SourceWindow* srcWnd) :
	QSyntaxHighlighter(srcWnd->document()),
	m_srcWnd(srcWnd)
{
}

enum HLState {
    hlCommentLine = 1,
    hlCommentBlock,
    hlIdent,
    hlString
};

static const char* const ckw[] =
{
    "alignas",
    "alignof",
    "and",
    "and_eq",
    "asm",
    "auto",
    "bitand",
    "bitor",
    "bool",
    "break",
    "case",
    "catch",
    "char",
    "char16_t",
    "char32_t",
    "class",
    "compl",
    "const",
    "const_cast",
    "constexpr",
    "continue",
    "decltype",
    "default",
    "delete",
    "do",
    "double",
    "dynamic_cast",
    "else",
    "enum",
    "explicit",
    "export",
    "extern",
    "false",
    "float",
    "for",
    "friend",
    "goto",
    "if",
    "inline",
    "int",
    "long",
    "mutable",
    "namespace",
    "new",
    "noexcept",
    "not",
    "not_eq",
    "nullptr",
    "operator",
    "or",
    "or_eq",
    "private",
    "protected",
    "public",
    "register",
    "reinterpret_cast",
    "return",
    "short",
    "signed",
    "sizeof",
    "static",
    "static_assert",
    "static_cast",
    "struct",
    "switch",
    "template",
    "this",
    "thread_local",
    "throw",
    "true",
    "try",
    "typedef",
    "typeid",
    "typename",
    "union",
    "unsigned",
    "using",
    "virtual",
    "void",
    "volatile",
    "wchar_t",
    "while",
    "xor",
    "xor_eq"
};

void HighlightCpp::highlightBlock(const QString& text)
{
    int state = previousBlockState();
    state = highlight(text, state);
    setCurrentBlockState(state);
}

int HighlightCpp::highlight(const QString& text, int state)
{
    // highlight assembly lines
    if (m_srcWnd->isRowDisassCode(currentBlock().blockNumber()))
    {
	setFormat(0, text.length(), Qt::blue);
	return state;
    }

    if (state == -2)		// initial state
	state = 0;

    // check for preprocessor line
    if (state == 0 && text.trimmed().startsWith(QLatin1Char('#')))
    {
	setFormat(0, text.length(), QColor("darkgreen"));
	return 0;
    }

    // a font for keywords
    QTextCharFormat identFont;
    identFont.setFontWeight(QFont::Bold);

    int start = 0;
    while (start < text.length())
    {
	int end;
	switch (state) {
	case hlCommentLine:
	    end = text.length();
	    state = 0;
	    setFormat(start, end-start, QColor("gray"));
	    break;
	case hlCommentBlock:
	    end = text.indexOf("*/", start);
	    if (end >= 0)
		end += 2, state = 0;
	    else
		end = text.length();
	    setFormat(start, end-start, QColor("gray"));
	    break;
	case hlString:
	    for (end = start+1; end < int(text.length()); end++) {
		if (text[end] == QLatin1Char('\\')) {
		    if (end < int(text.length()))
			++end;
		} else if (text[end] == text[start]) {
		    ++end;
		    break;
		}
	    }
	    state = 0;
	    setFormat(start, end-start, QColor("darkred"));
	    break;
	case hlIdent:
	    for (end = start+1; end < int(text.length()); end++) {
		if (!text[end].isLetterOrNumber() && text[end] != QLatin1Char('_'))
		    break;
	    }
	    state = 0;
	    if (isCppKeyword(text.mid(start, end-start)))
	    {
		setFormat(start, end-start, identFont);
	    } else {
		setFormat(start, end-start, m_srcWnd->palette().color(QPalette::WindowText));
	    }
	    break;
	default:
	    for (end = start; end < int(text.length()); end++)
	    {
		if (text[end] == QLatin1Char('/'))
		{
		    if (end+1 < int(text.length())) {
			if (text[end+1] == QLatin1Char('/')) {
			    state = hlCommentLine;
			    break;
			} else if (text[end+1] == QLatin1Char('*')) {
			    state = hlCommentBlock;
			    break;
			}
		    }
		}
		else if (text[end] == QLatin1Char('"') || text[end] == QLatin1Char('\''))
		{
		    state = hlString;
		    break;
		}
		else if ((text[end] >= QLatin1Char('A') && text[end] <= QLatin1Char('Z')) ||
			 (text[end] >= QLatin1Char('a') && text[end] <= QLatin1Char('z')) ||
			 text[end] == QLatin1Char('_'))
		{
		    state = hlIdent;
		    break;
		}
	    }
	    setFormat(start, end-start, m_srcWnd->palette().color(QPalette::WindowText));
	}
	start = end;
    }
    return state;
}

bool HighlightCpp::isCppKeyword(const QString& word)
{
    struct StringCompare {
	bool operator()(const char* a, const char* b) const {
	    return strcmp(a, b) < 0;
	};
    };
    using std::begin;
    using std::end;
#ifndef NDEBUG
    // std::binary_search requires the search list to be sorted
    static bool keyword_order_verified = false;
    if (!keyword_order_verified) {
	assert(std::is_sorted(begin(ckw), end(ckw), StringCompare{}));
	keyword_order_verified = true;
    }
#endif

    return std::binary_search(begin(ckw), end(ckw), word.toLocal8Bit().data(), StringCompare{});
}
