/*
 * Copyright Johannes Sixt
 * This file is licensed under the GNU General Public License Version 2.
 * See the file COPYING in the toplevel directory of the source directory.
 */

#ifndef SOURCEWND_H
#define SOURCEWND_H

#include <qpixmap.h>
#include <qtextedit.h>
#include <qsyntaxhighlighter.h>
#include <vector>
#include "dbgdriver.h"

// forward declarations
class KDebugger;
struct DbgAddr;

class SourceWindow : public QTextEdit
{
    Q_OBJECT
public:
    SourceWindow(const QString& fileName, QWidget* parent, const char* name);
    ~SourceWindow();
    
    bool loadFile();
    void reloadFile();
    bool fileNameMatches(const QString& other);
    void scrollTo(int lineNo, const DbgAddr& address);
    const QString& fileName() const { return m_fileName; }
    void updateLineItems(const KDebugger* dbg);
    void setPC(bool set, int lineNo, const DbgAddr& address, int frameNo);
    enum FindDirection { findForward = 1, findBackward = -1 };
    void find(const QString& text, bool caseSensitive, FindDirection dir);
    bool wordAtPoint(const QPoint& p, QString& word, QRect& r);
    /**
     * Translates row number (zero-based) to zero-based source line number.
     * If sourceRow is non-zero, it is filled with the source code row
     * belonging to the line number.
     */
    int rowToLine(int row, int* sourceRow = 0);
    /** Translates zero-based source line number to row number (zero-based) */
    int lineToRow(int line);
    /** Is the row disassembled? */
    bool isRowExpanded(int row);
    /** Does the row show disassembled code? */
    bool isRowDisassCode(int row);

    /** lineNo is zero-based */
    void disassembled(int lineNo, const std::list<DisassembledCode>& disass);

    void activeLine(int& lineNo, DbgAddr& address);

protected:
    virtual void drawFrame(QPainter* p);
    virtual bool eventFilter(QObject* watched, QEvent* e);
    virtual void contextMenuEvent(QContextMenuEvent* e);
    virtual void mousePressEvent(QMouseEvent* ev);
    virtual void keyPressEvent(QKeyEvent* ev);
    virtual void paletteChange(const QPalette&);
    void expandRow(int row);
    void collapseRow(int row);
    void scrollToRow(int row);
    /** translates (0-based) line number plus a code address into a row number */
    int lineToRow(int row, const DbgAddr& address);

    void actionExpandRow(int row);
    void actionCollapseRow(int row);

signals:
    void clickedLeft(const QString&, int, const DbgAddr& address, bool);
    void clickedMid(const QString&, int, const DbgAddr& address);
    void disassemble(const QString&, int);
    void expanded(int lineNo);		/* source lineNo has been expanded */
    void collapsed(int lineNo);		/* source lineNo has been collapsed */
public slots:
    void setTabWidth(int numChars);
    void cursorChanged(int row);

protected:
    QString m_fileName;
    enum LineItem { liPC = 1, liPCup = 2,
	liBP = 4, liBPdisabled = 8, liBPtemporary = 16,
	liBPconditional = 32, liBPorphan = 64,
	liBPany = liBP|liBPdisabled|liBPtemporary|liBPconditional|liBPorphan
    };

    struct SourceLine {
	QString code;			/* a line of text */
	std::vector<QString> disass;		/* its disassembled code */
	std::vector<DbgAddr> disassAddr;	/* the addresses thereof */
	bool canDisass;			/* if line can be disassembled */
	SourceLine() : canDisass(true) { }
	int findAddressRowOffset(const DbgAddr& address) const;
    };
    std::vector<SourceLine> m_sourceCode;

    std::vector<int> m_rowToLine;	//!< The source line number for each row
    std::vector<uchar> m_lineItems;	//!< Icons displayed on the line
    QPixmap m_pcinner;			/* PC at innermost frame */
    QPixmap m_pcup;			/* PC at frame up the stack */
    QPixmap m_brkena;			/* enabled breakpoint */
    QPixmap m_brkdis;			/* disabled breakpoint */
    QPixmap m_brktmp;			/* temporary breakpoint marker */
    QPixmap m_brkcond;			/* conditional breakpoint marker */
    QPixmap m_brkorph;			/* orphaned breakpoint marker */
    QFont m_lineNoFont;			//!< The font used to draw line numbers
    int m_curRow;			//!< The highlighted row
    int m_widthItems;			//!< The width of the item column
    int m_widthPlus;			//!< The width of the expander column
    int m_widthLineNo;			//!< The width of the line number columns
};

class HighlightCpp : public QSyntaxHighlighter
{
    SourceWindow* m_srcWnd;

public:
    HighlightCpp(SourceWindow* srcWnd);
    virtual int highlightParagraph(const QString& text, int state);
};

#endif // SOURCEWND_H
