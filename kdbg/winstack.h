// $Id$

// Copyright by Johannes Sixt
// This file is under GPL, the GNU General Public Licence

#ifndef WINSTACK_H
#define WINSTACK_H

#include <qlist.h>
#include <qdialog.h>
#include <qlineedit.h>
#include <qlayout.h>
#include <qcheckbox.h>
#include <qpushbutton.h>
#include <qpopupmenu.h>
#include <qtooltip.h>

// forward declarations
class KDebugger;
class WinStack;
class SourceWindow;

class FindDialog : public QDialog
{
    Q_OBJECT
public:
    FindDialog();
    ~FindDialog();

    bool caseSensitive() const { return m_caseCheck.isChecked(); }
    QString searchText() const { return m_searchText.text(); }
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


class ValueTip : public QToolTip
{
public:
    ValueTip(WinStack* parent);
    virtual void maybeTip(const QPoint& p);
    void tip(const QRect& r, const QString& s) { QToolTip::tip(r, s); }
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
    /**
     * Slot activate also looks in this directory when the specified file is
     * a relative path.
     */
    void setExtraDirectory(const QString& dir) { m_lastOpenDir = dir; }
    bool activeLine(QString& filename, int& lineNo);
    void maybeTip(const QPoint& p);

    virtual void resizeEvent(QResizeEvent*);

signals:
    void fileChanged();
    void lineChanged();
    void toggleBreak(const QString&, int);
    void enadisBreak(const QString&, int);
    void clickedRight(const QPoint&);
    void newFileLoaded();
    void initiateValuePopup(const QString&);
    void disassemble(const QString&, int);

public slots:
    virtual void menuCallback(int item);
    virtual void slotFindForward();
    virtual void slotFindBackward();
    virtual void slotFileWindowRightClick(const QPoint &);
    virtual void activate(const QString& filename, int lineNo);
    void updatePC(const QString& filename, int lineNo, int frameNo);
    void reloadAllFiles();
    void updateLineItems(const KDebugger* deb);

    // Right click on file panner when no file is loaded.
    virtual void slotWidgetRightClick(const QPoint &);

    // Displays the value tip at m_tipLocation
    void slotShowValueTip(const QString& tipText);

    // Shows the disassembled code at the location given by file and lineNo
    void slotDisassembled(const QString& fileName, int lineNo,
			  const QString& disass);

protected:
    void initMenu();
    bool activatePath(QString pathname, int lineNo);
    virtual bool activateWindow(SourceWindow* fw, int lineNo = -1);	/* -1 doesnt change line */
    virtual void changeWindowMenu();
    virtual void mousePressEvent(QMouseEvent*);
    void setPC(bool set, const QString& fileName, int lineNo, int frameNo);
    QList<SourceWindow> m_fileList;
    SourceWindow* m_activeWindow;
    QString m_lastOpenDir;		/* where user opened last file */
    QPopupMenu* m_windowMenu;
    int m_itemMore;
    QString m_textMore;
    
    // program counter
    QString m_pcFile;
    int m_pcLine;			/* -1 if no PC */
    int m_pcFrame;

    ValueTip m_valueTip;
    QRect m_tipLocation;		/* where tip should appear */

public:
    // find dialog
    FindDialog m_findDlg;

    // Popup menu
    QPopupMenu m_menuFloat;
    QPopupMenu m_menuFileFloat;
};

#endif // WINSTACK_H
