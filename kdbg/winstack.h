/*
 * Copyright Johannes Sixt
 * This file is licensed under the GNU General Public License Version 2.
 * See the file COPYING in the toplevel directory of the source directory.
 */

#ifndef WINSTACK_H
#define WINSTACK_H

#include <qptrlist.h>
#include <qdialog.h>
#include <qlineedit.h>
#include <qlayout.h>
#include <qcheckbox.h>
#include <qpushbutton.h>
#include <qpopupmenu.h>
#include <qtooltip.h>
#include "valarray.h"

// forward declarations
class KDebugger;
class WinStack;
class SourceWindow;
class DisassembledCode;
struct DbgAddr;

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
    virtual ~ValueTip() {}	// Qt3's QToolTip lacks virtual dtor!
    virtual void maybeTip(const QPoint& p);
    void tip(const QRect& r, const QString& s) { QToolTip::tip(r, s); }
};


class WinStack : public QWidget
{
    Q_OBJECT
public:
    WinStack(QWidget* parent, const char* name);
    virtual ~WinStack();

    enum { WindowMore=0x100, WindowMask=0xf };

    /**
     * The menu set with setWindowMenu will be modified by this widget to
     * list the available windows. The specified popup menu must be set up
     * to contain an entry with ID WindowMore. The windows will be inserted
     * before this entry.
     */
    void setWindowMenu(QPopupMenu* menu);
    /**
     * Slot activate also looks in this directory when the specified file is
     * a relative path.
     */
    void setExtraDirectory(const QString& dir) { m_lastOpenDir = dir; }
    void activateFile(const QString& fileName);
    bool activeLine(QString& filename, int& lineNo);
    bool activeLine(QString& filename, int& lineNo, DbgAddr& address);
    void maybeTip(const QPoint& p);
    bool hasWindows() const { return m_fileList.size() > 0; }
    QString activeFileName() const;

    virtual QSize sizeHint() const;
    virtual void resizeEvent(QResizeEvent*);

signals:
    void fileChanged();
    void lineChanged();
    void toggleBreak(const QString&, int, const DbgAddr&, bool);
    void enadisBreak(const QString&, int, const DbgAddr&);
    void newFileLoaded();
    void initiateValuePopup(const QString&);
    void disassemble(const QString&, int);
    void setTabWidth(int numChars);
    void moveProgramCounter(const QString&, int, const DbgAddr&);

public slots:
    void selectWindow(int id);		/* 1-based index, 0 means dialog More... */
    virtual void slotFindForward();
    virtual void slotFindBackward();
    virtual void activate(const QString& filename, int lineNo, const DbgAddr& address);
    void updatePC(const QString& filename, int lineNo, const DbgAddr& address, int frameNo);
    void reloadAllFiles();
    void updateLineItems(const KDebugger* deb);
    void slotSetTabWidth(int numChars);

    void slotFileReload();
    void slotViewFind();
    void slotBrkptSet();
    void slotBrkptSetTemp();
    void slotBrkptEnable();
    void slotMoveProgramCounter();

    // Displays the value tip at m_tipLocation
    void slotShowValueTip(const QString& tipText);

    // Shows the disassembled code at the location given by file and lineNo
    void slotDisassembled(const QString& fileName, int lineNo,
			  const QList<DisassembledCode>& disass);

    // Updates line items after expanding/collapsing disassembled code
    void slotExpandCollapse(int lineNo);

protected:
    bool activatePath(QString pathname, int lineNo, const DbgAddr& address);
    virtual bool activateWindow(SourceWindow* fw, int lineNo, const DbgAddr& address);	/* -1 doesnt change line */
    virtual void changeWindowMenu();
    virtual void contextMenuEvent(QContextMenuEvent* e);
    void setPC(bool set, const QString& fileName, int lineNo,
	       const DbgAddr& address, int frameNo);
    ValArray<SourceWindow*> m_fileList;
    SourceWindow* m_activeWindow;
    QString m_lastOpenDir;		/* where user opened last file */
    QPopupMenu* m_windowMenu;
    
    // program counter
    QString m_pcFile;
    int m_pcLine;			/* -1 if no PC */
    QString m_pcAddress;		/* exact address of PC */
    int m_pcFrame;

    ValueTip m_valueTip;
    QRect m_tipLocation;		/* where tip should appear */

    int m_tabWidth;			/* number of chars */

public:
    // find dialog
    FindDialog m_findDlg;
};

#endif // WINSTACK_H
