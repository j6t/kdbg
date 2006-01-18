// $Id$

// Copyright by Johannes Sixt
// This file is under GPL, the GNU General Public Licence

#include "tableview.h"

#include <qapplication.h>
#include <qpainter.h>
#include <qscrollbar.h>

#if 0
class CornerSquare : public QWidget	// internal class
{
public:
    CornerSquare(QWidget* parent, const char* name = 0);
    void paintEvent(QPaintEvent*);
};

CornerSquare::CornerSquare(QWidget* parent, const char* name) :
	QWidget(parent, name)
{
}

void CornerSquare::paintEvent(QPaintEvent*)
{
}
#endif

/*
 * A simplified version of QTableView, which works only for rows of uniform
 * height: cellHeight() is only invoked for row 0.
 */
TableView::TableView(QWidget* parent, const char* name, WFlags f) :
	QWidget(parent, name, f),
	m_numRows(0),
	m_numCols(0),
	m_xOffset(0),
	m_yOffset(0),
	m_totalSize(0, 0),
	m_autoUpdate(true)
{
    m_sbV = new QScrollBar(QScrollBar::Vertical, this, "table_sbV");
    m_sbH = new QScrollBar(QScrollBar::Horizontal, this, "table_sbH");
    m_sbV->resize(m_sbV->sizeHint());
    m_sbH->resize(m_sbH->sizeHint());
    m_sbV->hide();
    m_sbH->hide();
    connect(m_sbV, SIGNAL(valueChanged(int)), this, SLOT(sbVer(int)));
    connect(m_sbH, SIGNAL(valueChanged(int)), this, SLOT(sbHor(int)));

    m_sbCorner = new QWidget(this, "table_corner");
    m_sbCorner->hide();

    setFocusPolicy(WheelFocus);
}

TableView::~TableView()
{
}

void TableView::setNumCols(int cols)
{
    m_numCols = cols;
}

void TableView::setNumRows(int rows)
{
    m_numRows = rows;
}

void TableView::updateTableSize()
{
    // don't call cellHeight if there are now rows
    m_totalSize.setHeight(m_numRows > 0 ? cellHeight(0) * m_numRows : 0);
    int w = 0;
    for (int i = 0; i < m_numCols; i++) {
	w += cellWidth(i);
    }
    m_totalSize.setWidth(w);
    int maxX = maxXOffset();
    int maxY = maxYOffset();
    if (xOffset() > maxX || yOffset() > maxY) {
	setOffset(QMIN(xOffset(),maxX), QMIN(yOffset(),maxY));
    } else {
	updateScrollBars();
    }
}

int TableView::lastRowVisible() const
{
    if (numRows() == 0)
	return -1;
    int r = (viewHeight() + m_yOffset) / cellHeight(0) - 1;
    if (r < 0) {
	return 0;
    } else if (r >= numRows()) {
	return numRows()-1;
    } else {
	return r;
    }
}

int TableView::maxXOffset() const
{
    int o = m_totalSize.width()-viewWidth();
    return QMAX(0, o);
}

int TableView::maxYOffset() const
{
    int o = m_totalSize.height()-viewHeight();
    return QMAX(0, o);
}

void TableView::setOffset(int x, int y)
{
    int oldX = m_xOffset;
    int oldY = m_yOffset;
    int maxX = maxXOffset();
    int maxY = maxYOffset();
    m_xOffset = QMIN(x, maxX);
    m_yOffset = QMIN(y, maxY);
    if (m_autoUpdate) {
	QRect r(0,0, viewWidth(), viewHeight());
	scroll(oldX-m_xOffset, oldY-m_yOffset, r);
    }
    updateScrollBars();
}

void TableView::setTopCell(int row)
{
    if (numRows() > 0)
	setYOffset(row * cellHeight(0));
}

int TableView::topCell() const
{
    if (numRows() > 0)
	return yOffset() / cellHeight(0);
    else
	return -1;
}

int TableView::leftCell() const
{
    int x = -xOffset();
    int c = 0;
    while (x < 0) {
	x += cellWidth(c);
	c++;
    }
    // special-casing x == 0 saves one call to cellWidth()
    return x == 0  ?  c  :  c-1;
}

int TableView::viewWidth() const
{
    // takes into account whether scrollbars are visible
    int w = width();
    if (m_sbV->isVisible())
	w -= m_sbV->width();
    return w;
}

int TableView::viewHeight() const
{
    // takes into account whether scrollbars are visible
    int h = height();
    if (m_sbH->isVisible())
	h -= m_sbH->height();
    return h;
}

bool TableView::rowIsVisible(int row) const
{
    return row >= topCell() && row <= lastRowVisible();
}

int TableView::findRow(int y) const
{
    if (numRows() == 0)
	return -1;
    int r = (yOffset() + y) / cellHeight(0);
    if (r < 0 || r >= numRows())
	return -1;
    else
	return r;
}

bool TableView::rowYPos(int row, int* top) const
{
    if (numRows() == 0)
	return false;		// now rows => nothing visible
    int y = row * cellHeight(0) - yOffset();
    if (y <= -cellHeight(0))
	return false;		// row is above view
    if (y > viewHeight())
	return false;		// row is below view
    *top = y;
    return true;
}

int TableView::findCol(int x) const
{
    x += xOffset();
    if (x < 0)
	return -1;
    for (int col = 0; col < numCols(); col++) {
	x -= cellWidth(col);
	if (x < 0)
	    return col;
    }
    return -1;
}

bool TableView::colXPos(int col, int* left) const
{
    int x = -xOffset();
    int viewW = viewWidth();
    int c = 0;
    while (c < col) {
	x += cellWidth(c);
	if (x >= viewW) {
	    // col is completely to the right of view
	    return false;
	}
	c++;
    }
    if (x <= 0) {
	if (x + cellWidth(col) > 0) {
	    // col is partially visible at left of view
	    *left = x;
	    return true;
	} else {
	    // col is completely to the left of view
	    return false;
	}
    } else {
	// left edge of col is in the view
	*left = x;
	return true;
    }
}

void TableView::updateCell(int row, int col, bool erase)
{
    int x, y;
    if (!colXPos(col, &x))
	return;
    if (!rowYPos(row, &y))
	return;
    QRect r(x, y, cellWidth(col), cellHeight(0/*row*/));
    repaint(r, erase);
}

void TableView::setupPainter(QPainter* /*p*/)
{
    // does nothing special
}

void TableView::resizeEvent(QResizeEvent* /*ev*/)
{
    updateScrollBars();
}

void TableView::paintEvent(QPaintEvent* /*ev*/)
{
    if (numRows() == 0 || numCols() == 0)
	return;

    QPainter p(this);
    setupPainter(&p);

    int viewH = viewHeight();
    int viewW = viewWidth();
    int leftCol = leftCell();
    int leftX = 0;
    colXPos(leftCol, &leftX);
    int row = topCell();
    int y = 0;
    rowYPos(row, &y);
    QWMatrix matrix;
    while (y < viewH && row < numRows()) {
	int col = leftCol;
	int x = leftX;
	while (x < viewW && col < numCols()) {
	    matrix.translate(x, y);
	    p.setWorldMatrix(matrix);
	    paintCell(&p, row, col);
	    matrix.reset();
	    p.setWorldMatrix(matrix);
	    x += cellWidth(col);
	    col++;
	}
	row++;
	y += cellHeight(0/*row*/);
    }
}

void TableView::wheelEvent(QWheelEvent* ev)
{
#if QT_VERSION >= 300
    if (ev->orientation() == Horizontal && m_sbH != 0)
    {
	QApplication::sendEvent(m_sbH, ev);
    }
    else
#endif
    {
	if (m_sbV != 0)
	{
	    QApplication::sendEvent(m_sbV, ev);
	}
    }
}

void TableView::updateScrollBars()
{
    // see which scrollbars we absolutely need
    int w = width();
    int h = height();
    bool needV = m_totalSize.height() > h;
    bool needH = m_totalSize.width() > w;

    // if we need neither, remove the scrollbars and we're done
    if (!needV && !needH)
    {
	m_sbH->hide();
	m_sbV->hide();
	m_sbCorner->hide();
	return;
    }
    // if we need both, reduce view
    if (needV && needH) {
	w -= m_sbV->width();
	h -= m_sbH->height();
    } else {
	/*
	 * If we need the vertical bar, but not the horizontal, the
	 * presence of the vertical bar might reduce the space so that we
	 * also need the horizontal bar. Likewise, if we need the
	 * horizontal but not necessarily the vertical bar.
	 */
	if (needV) {
	    w -= m_sbV->width();
	    if (w < 0)
		w = 0;
	    needH = m_totalSize.width() > w;
	    if (needH) {
		h -= m_sbH->height();
	    }
	} else {
	    // assert(needH)
	    h -= m_sbH->height();
	    if (h < 0)
		h = 0;
	    needV = m_totalSize.height() > h;
	    if (needV) {
		w -= m_sbV->width();
	    }
	}
    }
    // set scrollbars
    // note: must show() early because max?Offset() depends on visibility
    if (needH) {
	m_sbH->setGeometry(0, h, w, m_sbH->height());
	m_sbH->show();
	m_sbH->setRange(0, maxXOffset());
	m_sbH->setValue(xOffset());
	m_sbH->setSteps(32, w);
    } else {
	m_sbH->hide();
    }
    if (needV) {
	m_sbV->setGeometry(w, 0, m_sbV->width(), h);
	m_sbV->show();
	m_sbV->setRange(0, maxYOffset());
	m_sbV->setValue(yOffset());
	m_sbV->setSteps(cellHeight(0), h);
    } else {
	m_sbV->hide();
    }
    // corner square: only if both scrollbars are there
    if (needH && needV) {
	m_sbCorner->setGeometry(w, h, m_sbV->width(), m_sbH->height());
	m_sbCorner->show();
    } else {
	m_sbCorner->hide();
    }
}

void TableView::sbVer(int value)
{
    setYOffset(value);
}

void TableView::sbHor(int value)
{
    setXOffset(value);
}

#include "tableview.moc"
