// $Id$

// Copyright by Johannes Sixt
// This file is under GPL, the GNU General Public Licence

#ifndef WINSTACK_H
#define WINSTACK_H

#include <qmlined.h>
#include <qlist.h>
#include <qpixmap.h>
#include "textvw.h"

// forward declarations
class QPopupMenu;
class BreakpointTable;

//class FileWindow : public QMultiLineEdit
class FileWindow : public KTextView
{
    Q_OBJECT
public:
    FileWindow(const char* fileName, QWidget* parent, const char* name);
    ~FileWindow();
    
    void loadFile();
    void scrollTo(int lineNo);
    const QString& fileName() const { return m_fileName; }
    void updateLineItems(const BreakpointTable& bpt);
    void setPC(bool set, int lineNo, int frameNo);

protected:
    virtual int textCol() const;
    virtual int cellWidth(int col);
    virtual void paintCell(QPainter* p, int row, int col);
    void updateLineItem(int i);

protected:
    QString m_fileName;
    enum LineItem { liPC = 1, liPCup = 2,
	liBP = 4, liBPdisabled = 8, liBPany = liBP|liBPdisabled
    };
    QArray<uchar> m_lineItems;
    QPixmap m_pcinner;			/* PC at innermost frame */
    QPixmap m_pcup;			/* PC at frame up the stack */
    QPixmap m_brkena;			/* enabled breakpoint */
    QPixmap m_brkdis;			/* disabled breakpoint */
};

class WinStack : public QWidget
{
    Q_OBJECT
public:
    WinStack(QWidget* parent, const char* name);
    virtual ~WinStack();
    
    /**
     * The menu set with setWindowMenu will be modified by this widget to
     * list the available windows. The specified popup menu must be set up
     * to contain an entry with ID_WINDOW_MORE. The windows will be inserted
     * before this entry.
     */
    void setWindowMenu(QPopupMenu* menu);
    void selectWindow(int index);	/* 1-based index, 0 means dialog More... */
    bool activate(QString filename, int lineNo);
    bool activeLine(QString& filename, int& lineNo);
    void updateLineItems(const BreakpointTable& bpt);
    void updatePC(const QString& filename, int lineNo, int frameNo);
    
    virtual void resizeEvent(QResizeEvent*);

public slots:
    virtual void menuCallback(int item);

protected:
    bool activatePath(QString pathname, int lineNo);
    virtual bool activateWindow(FileWindow* fw, int lineNo = -1);	/* -1 doesnt change line */
    virtual void changeWindowMenu();
    virtual void openFile();
    void setPC(bool set, const QString& fileName, int lineNo, int frameNo);
    QList<FileWindow> m_fileList;
    FileWindow* m_activeWindow;
    QPopupMenu* m_windowMenu;
    int m_itemMore;
    QString m_textMore;
    
    // program counter
    QString m_pcFile;
    int m_pcLine;			/* -1 if no PC */
    int m_pcFrame;
};

#endif // WINSTACK_H
