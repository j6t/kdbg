// $Id$

// Copyright by Johannes Sixt
// This file is under GPL, the GNU General Public Licence

#ifndef WINSTACK_H
#define WINSTACK_H

#include <qmlined.h>
#include <qlist.h>
#include <qpixmap.h>
#include <qdialog.h>
#include <qlined.h>
#include <qlayout.h>
#include <qchkbox.h>
#include <qpushbt.h>
#include <qpopupmenu.h>
#include "textvw.h"

// forward declarations
class QPopupMenu;
class QFileInfo;
class BreakpointTable;

//class FileWindow : public QMultiLineEdit
class FileWindow : public KTextView
{
    Q_OBJECT
public:
    FileWindow(const char* fileName, QWidget* parent, const char* name);
    ~FileWindow();
    
    void loadFile();
    void reloadFile();
    void scrollTo(int lineNo);
    const QString& fileName() const { return m_fileName; }
    void updateLineItems(const BreakpointTable& bpt);
    void setPC(bool set, int lineNo, int frameNo);
    enum FindDirection { findForward = 1, findBackward = -1 };
    void find(const char* text, bool caseSensitive, FindDirection dir);

protected:
    virtual int textCol() const;
    virtual int cellWidth(int col);
    virtual void paintCell(QPainter* p, int row, int col);
    virtual void mousePressEvent(QMouseEvent* ev);
    void updateLineItem(int i);

signals:
    void clickedLeft(const QString&, int);
    void clickedMid(const QString&, int);
    void clickedRight(const QPoint &);

protected:
    QString m_fileName;
    enum LineItem { liPC = 1, liPCup = 2,
	liBP = 4, liBPdisabled = 8, liBPtemporary = 16,
	liBPconditional = 32,
	liBPany = liBP|liBPdisabled|liBPtemporary|liBPconditional
    };
    QArray<uchar> m_lineItems;
    QPixmap m_pcinner;			/* PC at innermost frame */
    QPixmap m_pcup;			/* PC at frame up the stack */
    QPixmap m_brkena;			/* enabled breakpoint */
    QPixmap m_brkdis;			/* disabled breakpoint */
    QPixmap m_brktmp;			/* temporary breakpoint marker */
    QPixmap m_brkcond;			/* conditional breakpoint marker */
};

class FindDialog : public QDialog
{
    Q_OBJECT
public:
    FindDialog();
    ~FindDialog();

    bool caseSensitive() const { return m_caseCheck.isChecked(); }
    const char* searchText() const { return m_searchText.text(); }
    virtual void done(int result);

    QLineEdit m_searchText;
    QCheckBox m_caseCheck;
    QPushButton m_buttonForward;
    QPushButton m_buttonBackward;
    QPushButton m_buttonClose;

signals:
    void closed();

protected:
    virtual void closeEvent(QCloseEvent* ev);
    QVBoxLayout m_layout;
    QHBoxLayout m_buttons;
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
    bool activeLine(QString& filename, int& lineNo);

    virtual void resizeEvent(QResizeEvent*);

signals:
    void fileChanged();
    void lineChanged();
    void toggleBreak(const QString&, int);
    void enadisBreak(const QString&, int);
    void clickedRight(const QPoint&);
    void newFileLoaded();

public slots:
    virtual void menuCallback(int item);
    virtual void slotFindForward();
    virtual void slotFindBackward();
    virtual void slotFileWindowRightClick(const QPoint &);
    virtual void activate(const QString& filename, int lineNo);
    void updatePC(const QString& filename, int lineNo, int frameNo);
    void reloadAllFiles();
    void updateLineItems(const BreakpointTable& bpt);

    // Right click on file panner when no file is loaded.
    virtual void slotWidgetRightClick(const QPoint &);

protected:
    void initMenu();
    bool activateFI(QFileInfo& fi, int lineNo);
    bool activatePath(QString pathname, int lineNo);
    virtual bool activateWindow(FileWindow* fw, int lineNo = -1);	/* -1 doesnt change line */
    virtual void changeWindowMenu();
    virtual void openFile();
    virtual void mousePressEvent(QMouseEvent*);
    void setPC(bool set, const QString& fileName, int lineNo, int frameNo);
    QList<FileWindow> m_fileList;
    FileWindow* m_activeWindow;
    QString m_lastOpenDir;		/* where user opened last file */
    QPopupMenu* m_windowMenu;
    int m_itemMore;
    QString m_textMore;
    
    // program counter
    QString m_pcFile;
    int m_pcLine;			/* -1 if no PC */
    int m_pcFrame;

public:
    // find dialog
    FindDialog m_findDlg;

    // Popup menu
    QPopupMenu m_menuFloat;
    QPopupMenu m_menuFileFloat;
};

#endif // WINSTACK_H
