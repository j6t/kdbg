// $Id$

// Copyright by Johannes Sixt
// This file is under GPL, the GNU General Public Licence

#include <qpainter.h>
#include <qkeycode.h>
#include "textvw.h"
#include "textvw.moc"
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include "mydebug.h"

#define DEFAULT_WIDTH 100
#define DEFAULT_LINEHEIGHT 1

KTextView::KTextView(QWidget* parent, const char* name, WFlags f) :
	TableView(parent, name, f),
	m_width(DEFAULT_WIDTH),
	m_height(DEFAULT_LINEHEIGHT),
	m_tabWidth(0),
	m_curRow(-1)
{
    setNumCols(1);
    setBackgroundColor(colorGroup().base());
}

KTextView::~KTextView()
{
}

/*
 * Update cell width and hight; returns whether there is a change
 * Cell geometry: There are 2 pixels to the left and to the right
 * and 1 pixel _below_ the line
 */
bool KTextView::updateCellSize(const QString& text)
{
    QPainter p(this);
    setupPainter(&p);
    QRect r = p.boundingRect(0,0, 0,0,
			     AlignLeft | SingleLine | DontClip | ExpandTabs,
			     text, text.length());

    bool update = false;
    int w = r.width() + 4;
    if (w > m_width) {
	m_width = w;
	update = true;
    }
    int h = r.height() + 1;
    if (h > m_height) {
	m_height = h;
	update = true;
    }
    return update;
}

void KTextView::insertLine(const QString& text)
{
    m_texts.append(text);
    setNumRows(m_texts.size());

    updateCellSize(text);

    if (autoUpdate()) {
	updateTableSize();
	repaint();
    }
}

/*
 * TODO: This function doesn't shrink the line length if the longest line
 * is replaced by a shorter one.
 */
void KTextView::replaceLine(int line, const QString& text)
{
    if (line < 0 || line >= int(m_texts.size()))
	return;

    m_texts[line] = text;

    bool update = updateCellSize(text);

    if (autoUpdate()) {
	if (update) {
	    updateTableSize();
	}
	repaint();
    }
}

void KTextView::setCursorPosition(int row, int)
{
    activateLine(row);
}

void KTextView::cursorPosition(int* row, int* col)
{
    *row = m_curRow;
    *col = 0;
}

int KTextView::cellWidth(int /*col*/) const 
{
    return m_width;
}

int KTextView::cellHeight(int /*row*/) const
{
    return m_height;
}

int KTextView::textCol() const
{
    // by default, the text is in column 0
    return 0;
}

void KTextView::paintCell(QPainter* p, int row, int /*col*/)
{
    if (row >= m_texts.size()) {
	return;
    }
    if (row == m_curRow) {
	// paint background
	p->fillRect(0,0, m_width,m_height, QBrush(colorGroup().background()));
    }
    const QString& text = m_texts[row];
    p->drawText(2,0, m_width-4, m_height,
		AlignLeft | SingleLine | DontClip | ExpandTabs,
		text, text.length());
}

void KTextView::keyPressEvent(QKeyEvent* ev)
{
    int oldRow = m_curRow;

    // go to line 0 if no current line
    if (m_curRow < 0) {
	activateLine(0);
    }
    else
    {
	int numVisibleRows, newRow, newX;

	switch (ev->key()) {
	case Key_Up:
	    if (m_curRow > 0) {
		activateLine(m_curRow-1);
	    }
	    break;
	case Key_Down:
	    if (m_curRow < numRows()-1) {
		activateLine(m_curRow+1);
	    }
	    break;
	case Key_PageUp:
	    /* this method doesn't work for variable height lines */
	    numVisibleRows = lastRowVisible()-topCell();
	    newRow = m_curRow - QMAX(numVisibleRows,1);
	    if (newRow < 0)
		newRow = 0;
	    activateLine(newRow);
	    break;
	case Key_PageDown:
	    /* this method doesn't work for variable height lines */
	    numVisibleRows = lastRowVisible()-topCell();
	    newRow = m_curRow + QMAX(numVisibleRows,1);
	    if (newRow >= numRows())
		newRow = numRows()-1;
	    activateLine(newRow);
	    break;

	// scroll left and right by a few pixels
	case Key_Left:
	    newX = xOffset() - viewWidth()/10;
	    setXOffset(QMAX(newX, 0));
	    break;
	case Key_Right:
	    newX = xOffset() + viewWidth()/10;
	    setXOffset(QMIN(newX, maxXOffset()));
	    break;

	default:
	    QWidget::keyPressEvent(ev);
	    return;
	}
    }
    // make row visible
    if (m_curRow != oldRow && !rowIsVisible(m_curRow)) {
	// if the old row was visible, we scroll the view by some pixels
	// if the old row was not visible, we place active row at the top
	if (oldRow >= 0 && rowIsVisible(oldRow)) {
	    int diff = m_curRow - oldRow;
	    int newTop = topCell() + diff;
	    setTopCell(QMAX(newTop,0));
	} else {
	    setTopCell(m_curRow);
	}
    }
    ev->accept();
}

void KTextView::mousePressEvent(QMouseEvent* ev)
{
    int row = findRow(ev->y());
    activateLine(row);
}

void KTextView::focusInEvent(QFocusEvent*)
{
    /*
     * The base class does a repaint(), which causes flicker. So we do
     * nothing here.
     */
}

void KTextView::focusOutEvent(QFocusEvent*)
{
    /*
     * The base class does a repaint(), which causes flicker. So we do
     * nothing here.
     */
}

void KTextView::activateLine(int row)
{
    if (row == m_curRow)
	return;

    int col = textCol();

    int oldRow = m_curRow;
    // note that row may be < 0
    m_curRow = row;
    // must change m_curRow first, so that updateCell(oldRow) erases the old line!
    if (oldRow >= 0) {
	updateCell(oldRow, col);
    }
    if (row >= 0) {
	updateCell(row, col);
    }
    emit lineChanged();
}

/* This is needed to make the kcontrol's color setup working */
void KTextView::paletteChange(const QPalette& oldPal)
{
    setBackgroundColor(colorGroup().base());
    TableView::paletteChange(oldPal);

    // recompute window size
    m_width = DEFAULT_WIDTH;
    m_height = DEFAULT_LINEHEIGHT;
    for (int i = 0; i < m_texts.size(); i++) {
	updateCellSize(m_texts[i]);
    }
    updateTableSize();
}

void KTextView::setTabWidth(int numChars)
{
    QFontMetrics fm = font();
    int newTabWidth = fm.width('x') * numChars;
    if (newTabWidth == m_tabWidth)
	return;
    m_tabWidth = newTabWidth;

    // recompute window width
    m_width = DEFAULT_WIDTH;
    for (int i = 0; i < m_texts.size(); i++) {
	updateCellSize(m_texts[i]);
    }

    updateTableSize();
    if (autoUpdate()) {
	repaint();
    }
}

void KTextView::setupPainter(QPainter* p)
{
    p->setTabStops(m_tabWidth);
}
