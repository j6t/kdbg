// $Id$

// Copyright by Johannes Sixt
// This file is under GPL, the GNU General Public Licence

#ifndef MAINWNDBASE_H
#define MAINWNDBASE_H

#include <qlineedit.h>
#include <qlayout.h>
#include <qpushbutton.h>
#include "exprwnd.h"
#include "sys/types.h"			/* pid_t */

// forward declarations
class KDebugger;
class UpdateUI;
class KStdAccel;
class KToolBar;
class KStatusBar;
class KProcess;

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


class DebuggerMainWndBase
{
public:
    DebuggerMainWndBase();
    virtual ~DebuggerMainWndBase();

    /**
     * Sets the command to invoke the terminal that displays the program
     * output. If cmd is the empty string, the default is substituted.
     */
    void setTerminalCmd(const QString& cmd);
    /**
     * Sets the command to invoke the debugger.
     */
    void setDebuggerCmdStr(const QString& cmd);
    /**
     * Specifies the file where to write the transcript.
     */
    void setTranscript(const char* name);

    // the following are needed to handle program arguments
    bool debugProgram(const QString& executable);
    void setCoreFile(const QString& corefile);
    void setRemoteDevice(const QString &remoteDevice);
    /** returns true if the command was handled */
    bool handleCommand(int item);

protected:
    // settings
    virtual void saveSettings(KConfig*);
    virtual void restoreSettings(KConfig*);

    // override must return the toolbar containing the animation and
    // buttons to update
    virtual KToolBar* dbgToolBar() = 0;
    // override must return the statusbar
    virtual KStatusBar* dbgStatusBar() = 0;
    // override must return the main window (usually this)
    virtual QWidget* dbgMainWnd() = 0;

    // statusbar texts
    QString m_statusActive;

    // animated button
    QList<QPixmap> m_animation;
    uint m_animationCounter;
    void initAnimation();

    // output window
    QString m_outputTermCmdStr;
    QString m_outputTermName;
    QString m_outputTermKeepScript;
    KProcess* m_outputTermProc;
    virtual bool createOutputWindow();
    void shutdownTermWindow();

    QString m_lastDirectory;		/* the dir of the most recently opened file */

    QString m_transcriptFile;		/* where gdb dialog is logged */

    // the debugger proper
    QString m_debuggerCmdStr;
    KDebugger* m_debugger;
    void setupDebugger(ExprWnd* localVars,
		       ExprWnd* watchVars,
		       QListBox* backtrace);

public:
    /*
     * Important! The following functions must be overridden in derived
     * classes and be declared as slots! Note: These must not be declared
     * virtual here since Qt signal mechanism fails miserably (because this
     * class will not be the left-most base class!).
     */
    void updateUIItem(UpdateUI* item);
    void updateLineItems();
    void slotNewStatusMsg();
    void slotAnimationTimeout();
    void slotGlobalOptions();
    void slotDebuggerStarting();
};

#endif // MAINWNDBASE_H
