// $Id$

// Copyright by Johannes Sixt
// This file is under GPL, the GNU General Public Licence

#ifndef MAINWNDBASE_H
#define MAINWNDBASE_H

#include <qlineedit.h>
#include <qlayout.h>
#include <qpushbutton.h>
#include <ktmainwindow.h>
#include "exprwnd.h"

// forward declarations
class KDebugger;
class UpdateUI;
class KStdAccel;

extern KStdAccel* keys;


class WatchWindow : public QWidget
{
    Q_OBJECT
public:
    WatchWindow(QWidget* parent, const char* name, WFlags f = 0);
    ~WatchWindow();
    ExprWnd* watchVariables() { return &m_watchVariables; }
    QString watchText() const { return m_watchEdit.text(); }

protected:
    QLineEdit m_watchEdit;
    QPushButton m_watchAdd;
    QPushButton m_watchDelete;
    ExprWnd m_watchVariables;
    QVBoxLayout m_watchV;
    QHBoxLayout m_watchH;

signals:
    void addWatch();
    void deleteWatch();

protected slots:
    void slotWatchHighlighted(int);
};


class DebuggerMainWndBase : public KTMainWindow
{
    Q_OBJECT
public:
    DebuggerMainWndBase(const char* name);
    ~DebuggerMainWndBase();

    // the following are needed to handle program arguments
    bool debugProgram(const QString& executable);
    void setCoreFile(const QString& corefile);
    void setRemoteDevice(const QString &remoteDevice);

protected:
    // settings
    virtual void saveSettings(KConfig*);
    virtual void restoreSettings(KConfig*);

    // statusbar texts
    QString m_statusActive;

    // animated button
    QList<QPixmap> m_animation;
    uint m_animationCounter;
    void initAnimation();

    // the debugger proper
    KDebugger* m_debugger;
    void setupDebugger(ExprWnd* localVars,
		       ExprWnd* watchVars,
		       QListBox* backtrace);

signals:
    void forwardMenuCallback(int item);

public slots:
    virtual void menuCallback(int item);
    virtual void updateUIItem(UpdateUI* item);
    virtual void updateUI();
    virtual void updateLineItems();
    void slotNewStatusMsg();
    void slotAnimationTimeout();
    void slotGlobalOptions();
};

#endif // MAINWNDBASE_H
