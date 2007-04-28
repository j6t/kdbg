// $Id$

// Copyright by Johannes Sixt
// This file is under GPL, the GNU General Public Licence

#ifndef TEXTVW_H
#define TEXTVW_H

#include "tableview.h"
#include <vector>

class KTextView : public TableView
{
    Q_OBJECT
public:
    KTextView(QWidget* parent = 0, const char* name = 0, WFlags f = 0);
    ~KTextView();
    void insertLine(const QString& text);
    void replaceLine(int line, const QString& text);
    virtual void setCursorPosition(int row, int col);
    virtual void cursorPosition(int* row, int* col);
    void setText(const QString& t);
    const QString& text(int i) const { return m_texts[i]; }
    int paragraphs() const { return m_texts.size(); }
    void insertParagraph(const QString& text, int row);
    void removeParagraph(int row);
    int charAt(const QPoint& p, int* para);
protected:
    virtual int cellWidth(int col) const;
    virtual int cellHeight(int row) const;
    virtual void paintCell(QPainter* p, int row, int col);
    virtual void activateLine(int row);
    virtual int textCol() const;
    virtual bool updateCellSize(const QString& text);
    virtual void setupPainter(QPainter* p);

signals:
    void lineChanged();

public slots:
    void setTabWidth(int numChars);

    // event handling
protected:
    virtual void keyPressEvent(QKeyEvent* ev);
    virtual void mousePressEvent(QMouseEvent* ev);
    virtual void focusInEvent(QFocusEvent* ev);
    virtual void focusOutEvent(QFocusEvent* ev);

    void paletteChange(const QPalette& oldPal);

    int m_width;
    int m_height;			/* line height */
    int m_tabWidth;			/* in pixels */

    int m_curRow;			/* cursor position */
private:
    std::vector<QString> m_texts;
};

#endif // TEXTVW_H
