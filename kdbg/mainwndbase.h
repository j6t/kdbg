/*
 * Copyright Johannes Sixt
 * This file is licensed under the GNU General Public License Version 2.
 * See the file COPYING in the toplevel directory of the source directory.
 */

#ifndef MAINWNDBASE_H
#define MAINWNDBASE_H

#include <qstring.h>
#include "watchwindow.h"

// forward declarations
class KDebugger;
class KProcess;
class DebuggerDriver;
class QListBox;

class DebuggerMainWndBase
{
public:
    DebuggerMainWndBase();
    virtual ~DebuggerMainWndBase();

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

protected:
    QString m_transcriptFile;		/* where gdb dialog is logged */

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
};

#endif // MAINWNDBASE_H
