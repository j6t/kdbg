// $Id$

// Copyright by Johannes Sixt
// This file is under GPL, the GNU General Public Licence

#ifndef TEXTVW_H
#define TEXTVW_H

#include <qtablevw.h>

class KTextView : public QTableView
{
    Q_OBJECT
public:
    KTextView(QWidget* parent = 0, const char* name = 0, WFlags f = 0);
    ~KTextView();
    void insertLine(const char*);
    virtual void setCursorPosition(int row, int col);
    virtual void cursorPosition(int* row, int* col);
protected:
    virtual int cellWidth(int col);
    virtual int cellHeight(int row);
    virtual void paintCell(QPainter* p, int row, int col);
    virtual void activateLine(int row);
    virtual int textCol() const;

signals:
    void lineChanged();

    // event handling
protected:
    virtual void keyPressEvent(QKeyEvent* ev);
    virtual void mousePressEvent(QMouseEvent* ev);
    virtual void focusInEvent(QFocusEvent* ev);
    virtual void focusOutEvent(QFocusEvent* ev);

    void paletteChange(const QPalette& oldPal);

    int m_width;
    int m_height;
    
    QArray<char*> m_texts;
    int m_curRow;			/* cursor position */
};

#endif // TEXTVW_H
