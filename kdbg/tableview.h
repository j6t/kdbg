// $Id$

// Copyright by Johannes Sixt
// This file is under GPL, the GNU General Public Licence

#ifndef TABLEVIEW_H
#define TABLEVIEW_H

#include <qwidget.h>

class QScrollBar;

class TableView : public QWidget
{
    Q_OBJECT
public:
    TableView(QWidget* parent, const char* name, WFlags f);
    ~TableView();

    void setNumCols(int cols);
    int numCols() const { return m_numCols; }
    void setNumRows(int rows);
    int numRows() const { return m_numRows; }
    void setAutoUpdate(bool update) { m_autoUpdate = update; }
    bool autoUpdate() const { return m_autoUpdate; }
    void updateTableSize();
    int totalWidth() const { return m_totalSize.width(); }
    int totalHeight() const { return m_totalSize.height(); }
    int lastRowVisible() const;
    void setTopCell(int row);
    int topCell() const;
    int leftCell() const;
    void setXOffset(int x) { setOffset(x, yOffset()); }
    int xOffset() const { return m_xOffset; }
    int maxXOffset() const;
    void setYOffset(int y) { setOffset(xOffset(), y); }
    int yOffset() const { return m_yOffset; }
    int maxYOffset() const;
    void setOffset(int x, int y);
    int viewWidth() const;
    int viewHeight() const;
    bool rowIsVisible(int row) const;
    int findRow(int y) const;
    bool rowYPos(int row, int* top) const;
    int findCol(int x) const;
    bool colXPos(int col, int* left) const;
    void updateCell(int row, int col, bool erase = true);

    // override in derived classes
    virtual int cellWidth(int col) const = 0;
    virtual int cellHeight(int row) const = 0;
    virtual void paintCell(QPainter* p, int row, int col) = 0;
    virtual void setupPainter(QPainter* p);

    // overrides
    virtual void resizeEvent(QResizeEvent* ev);
    virtual void paintEvent(QPaintEvent* ev);

public slots:
    void sbVer(int value);
    void sbHor(int value);

private:
    int m_numRows, m_numCols;
    int m_xOffset, m_yOffset;
    QSize m_totalSize;
    bool m_autoUpdate;
    QScrollBar* m_sbV;
    QScrollBar* m_sbH;
    QWidget* m_sbCorner;
    void updateScrollBars();
};

#endif // TABLEVIEW_H
