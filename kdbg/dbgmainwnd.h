// $Id$

// Copyright by Johannes Sixt
// This file is under GPL, the GNU General Public Licence

#ifndef DBGMAINWND_H
#define DBGMAINWND_H

#include <qlistbox.h>
#include <qlined.h>
#include <qlayout.h>
#include <qpushbt.h>
#include <ktmainwindow.h>
#include <knewpanner.h>
#include "winstack.h"
#include "exprwnd.h"

// forward declarations
class KDebugger;
class UpdateUI;
class KStdAccel;

extern KStdAccel* keys;


class DebuggerMainWnd : public KTMainWindow
{
    Q_OBJECT
public:
    DebuggerMainWnd(const char* name);
    ~DebuggerMainWnd();

    // the following are needed to handle program arguments
    bool debugProgram(const QString& executable);
    void setCoreFile(const QString& corefile);

protected:
    // session properties
    virtual void saveProperties(KConfig*);
    virtual void readProperties(KConfig*);
    // settings
    void saveSettings(KConfig*);
    void restoreSettings(KConfig*);

    // statusbar texts
    void updateLineStatus(int lineNo);	/* zero-based line number */
    QString m_statusActive;

    void initMenu();
    void initToolbar();
    void initAnimation();

    // view windows
    KNewPanner m_mainPanner;
    KNewPanner m_leftPanner;
    KNewPanner m_rightPanner;
    WinStack m_filesWindow;
    QListBox m_btWindow;
    ExprWnd m_localVariables;

    QWidget m_watches;
    QLineEdit m_watchEdit;
    QPushButton m_watchAdd;
    QPushButton m_watchDelete;
    ExprWnd m_watchVariables;
    QVBoxLayout m_watchV;
    QHBoxLayout m_watchH;

    // menus
    QPopupMenu m_menuFile;
    QPopupMenu m_menuView;
    QPopupMenu m_menuProgram;
    QPopupMenu m_menuBrkpt;
    QPopupMenu m_menuWindow;
    QPopupMenu m_menuHelp;

    // animated button
    QList<QPixmap> m_animation;
    uint m_animationCounter;

    // the debugger proper
    KDebugger* m_debugger;

protected:
    virtual void closeEvent(QCloseEvent* e);

signals:
    void forwardMenuCallback(int item);

public slots:
    virtual void menuCallback(int item);
    virtual void updateUIItem(UpdateUI* item);
    void newStatusMsg();
    void slotFileChanged();
    void slotLineChanged();
    void slotAnimationTimeout();
    void updateUI();
    void updateLineItems();
    void slotAddWatch();
    void slotWatchHighlighted(int);
    void slotNewFileLoaded();
    void slotGlobalOptions();
};

#endif // DBGMAINWND_H
