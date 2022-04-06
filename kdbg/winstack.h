/*
 * Copyright Johannes Sixt
 * This file is licensed under the GNU General Public License Version 2.
 * See the file COPYING in the toplevel directory of the source directory.
 */

#ifndef WINSTACK_H
#define WINSTACK_H

#include <QDialog>
#include <QLabel>
#include <QLineEdit>
#include <QCheckBox>
#include <QPushButton>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QTabWidget>
#include <list>

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
    void done(int result) override;

    QLineEdit m_searchText;
    QCheckBox m_caseCheck;
    QPushButton m_buttonForward;
    QPushButton m_buttonBackward;
    QPushButton m_buttonClose;

signals:
    void closed();

protected:
    void closeEvent(QCloseEvent* ev) override;
    QVBoxLayout m_layout;
    QHBoxLayout m_buttons;
};


/** Class for Goto Line dialog. */
class GotoDialog : public QDialog
{
    Q_OBJECT
public:
    GotoDialog();
    ~GotoDialog();

    QString lineText() const { return m_lineText.text(); }
    void done(int result) override;

    QLabel m_label;
    QLineEdit m_lineText;
    QPushButton m_buttonClose;

signals:
    void closed();

protected:
    void closeEvent(QCloseEvent* ev) override;
    QHBoxLayout m_layout;
};


class WinStack : public QTabWidget
{
    Q_OBJECT
public:
    WinStack(QWidget* parent);
    ~WinStack();

    /**
     * Slot activate also looks in this directory when the specified file is
     * a relative path.
     */
    void setExtraDirectory(const QString& dir) { m_lastOpenDir = dir; }
    void activateFile(const QString& fileName);
    bool activeLine(QString& filename, int& lineNo);
    bool activeLine(QString& filename, int& lineNo, DbgAddr& address);
    bool hasWindows() const { return count() > 0; }
    QString activeFileName() const;
    SourceWindow* activeWindow() const;
    SourceWindow* windowAt(int i) const;

    QSize sizeHint() const override;

signals:
    void toggleBreak(const QString&, int, const DbgAddr&, bool);
    void enadisBreak(const QString&, int, const DbgAddr&);
    void newFileLoaded();
    void initiateValuePopup(const QString&);
    void disassemble(const QString&, int);
    void setTabWidth(int numChars);
    void moveProgramCounter(const QString&, int, const DbgAddr&);

public slots:
    virtual void slotFindForward();
    virtual void slotFindBackward();
    virtual void slotGotoLine();
    virtual void activate(const QString& filename, int lineNo, const DbgAddr& address);
    void updatePC(const QString& filename, int lineNo, const DbgAddr& address, int frameNo);
    void reloadAllFiles();
    void updateLineItems(const KDebugger* deb);
    void slotSetTabWidth(int numChars);

    void slotFileReload();
    void slotViewFind();
    void slotViewGoto();
    void slotBrkptSet();
    void slotBrkptSetTemp();
    void slotBrkptEnable();
    void slotMoveProgramCounter();
    void slotClose();
    void slotCloseTab(int tab);

    // Displays the value tip at m_tipLocation
    void slotShowValueTip(const QString& tipText);

    // Shows the disassembled code at the location given by file and lineNo
    void slotDisassembled(const QString& fileName, int lineNo,
			  const std::list<DisassembledCode>& disass);

    // Updates line items after expanding/collapsing disassembled code
    void slotExpandCollapse(int lineNo);

    /** Handles changes in disassembly flavor.
     */
    void slotFlavorChanged(const QString& flavor, const QString& target);

protected:
    bool activatePath(QString pathname, int lineNo, const DbgAddr& address);
    virtual bool activateWindow(SourceWindow* fw, int lineNo, const DbgAddr& address);	/* -1 doesnt change line */
    void contextMenuEvent(QContextMenuEvent* e) override;
    bool event(QEvent* event) override;
    void setPC(bool set, const QString& fileName, int lineNo,
	       const DbgAddr& address, int frameNo);
    SourceWindow* findByFileName(const QString& fileName) const;
    QString m_lastOpenDir;		/* where user opened last file */
    
    // program counter
    QString m_pcFile;
    int m_pcLine;			/* -1 if no PC */
    QString m_pcAddress;		/* exact address of PC */
    int m_pcFrame;

    QPoint m_tipLocation;		/* where tip should appear */
    QRect m_tipRegion;			/* where tip should remain */

    int m_tabWidth;			/* number of chars */

public:
    // find dialog
    FindDialog m_findDlg;
    GotoDialog m_gotoDlg;
};

#endif // WINSTACK_H
