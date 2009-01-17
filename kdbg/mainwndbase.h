/*
 * Copyright Johannes Sixt
 * This file is licensed under the GNU General Public License Version 2.
 * See the file COPYING in the toplevel directory of the source directory.
 */

#ifndef MAINWNDBASE_H
#define MAINWNDBASE_H

#include <qlineedit.h>
#include <qlayout.h>
#include <qpushbutton.h>
#include <qcstring.h>
#include "exprwnd.h"
#include "sys/types.h"			/* pid_t */

// forward declarations
class KDebugger;
class TTYWindow;
class UpdateUI;
class KToolBar;
class KStatusBar;
class KProcess;
class DebuggerDriver;

class WatchWindow : public QWidget
{
    Q_OBJECT
public:
    WatchWindow(QWidget* parent, const char* name, WFlags f = 0);
    ~WatchWindow();
    ExprWnd* watchVariables() { return &m_watchVariables; }
    QString watchText() const { return m_watchEdit.text(); }
    int columnWidth(int i) const { return m_watchVariables.columnWidth(i); }
    void setColumnWidth(int i, int w) { m_watchVariables.setColumnWidth(i, w); }

protected:
    QLineEdit m_watchEdit;
    QPushButton m_watchAdd;
    QPushButton m_watchDelete;
    ExprWnd m_watchVariables;
    QVBoxLayout m_watchV;
    QHBoxLayout m_watchH;

    virtual bool eventFilter(QObject* ob, QEvent* ev);
    virtual void dragEnterEvent(QDragEnterEvent* event);
    virtual void dropEvent(QDropEvent* event);

signals:
    void addWatch();
    void deleteWatch();
    void textDropped(const QString& text);

protected slots:
    void slotWatchHighlighted();
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
    void setTranscript(const QString& name);
    /**
     * Starts to debug the specified program using the specified language
     * driver.
     */
    bool debugProgram(const QString& executable, QString lang, QWidget* parent);
    /**
     * Specifies the process to attach to after the program is loaded.
     */
    void setAttachPid(const QString& pid);

    // the following are needed to handle program arguments
    void setCoreFile(const QString& corefile);
    void setRemoteDevice(const QString &remoteDevice);
    void overrideProgramArguments(const QString& args);
    /** helper around KFileDialog */
    static QString myGetFileName(QString caption,
				 QString dir, QString filter,
				 QWidget* parent);
    /** invokes the global options dialog */
    virtual void doGlobalOptions(QWidget* parent);

protected:
    // settings
    virtual void saveSettings(KConfig*);
    virtual void restoreSettings(KConfig*);

    // override must return the integrated output window
    virtual TTYWindow* ttyWindow() = 0;

    // statusbar texts
    QString m_statusActive;

    // output window
    QString m_outputTermCmdStr;
    QString m_outputTermKeepScript;
    KProcess* m_outputTermProc;
    int m_ttyLevel;
    virtual QString createOutputWindow();	/* returns terminal name */
    void shutdownTermWindow();

    QString m_lastDirectory;		/* the dir of the most recently opened file */

    QString m_transcriptFile;		/* where gdb dialog is logged */

    bool m_popForeground;		/* whether main wnd raises when prog stops */
    int m_backTimeout;			/* when wnd goes back */
    int m_tabWidth;			/* tab width in characters (can be 0) */
    QString m_sourceFilter;
    QString m_headerFilter;

    // the debugger proper
    QString m_debuggerCmdStr;
    KDebugger* m_debugger;
    void setupDebugger(QWidget* parent,
		       ExprWnd* localVars,
		       ExprWnd* watchVars,
		       QListBox* backtrace);
    DebuggerDriver* driverFromLang(QString lang);
    /**
     * This function derives a driver name from the contents of the named
     * file.
     */
    QString driverNameFromFile(const QString& exe);

public:
    /*
     * Important! The following functions must be overridden in derived
     * classes and be declared as slots! Note: These must not be declared
     * virtual here since Qt signal mechanism fails miserably (because this
     * class will not be the left-most base class!).
     */
    void newStatusMsg(KStatusBar* statusbar);
    void slotDebuggerStarting();
};

#endif // MAINWNDBASE_H
