/*
 * Copyright Johannes Sixt
 * This file is licensed under the GNU General Public License Version 2.
 * See the file COPYING in the toplevel directory of the source directory.
 */

#ifndef GDBDRIVER_H
#define GDBDRIVER_H

#include "dbgdriver.h"


class GdbDriver : public DebuggerDriver
{
    Q_OBJECT
public:
    GdbDriver();
    ~GdbDriver();
    
    QString driverName() const override;
    QString defaultInvocation() const override;
    void setDefaultInvocation(QString cmd) { m_defaultCmd = cmd; }
    static QString defaultGdb();
    bool startup(QString cmdStr) override;
    void commandFinished(CmdQueueItem* cmd) override;

    void terminate() override;
    void detachAndTerminate() override;
    void interruptInferior() override;
    void setPrintQStringDataCmd(const char* cmd) override;
    ExprValue* parseQCharArray(const char* output, bool wantErrorValue, bool qt3like) override;
    void parseBackTrace(const char* output, std::list<StackFrame>& stack) override;
    bool parseFrameChange(const char* output, int& frameNo,
				  QString& file, int& lineNo, DbgAddr& address) override;
    bool parseBreakList(const char* output, std::list<Breakpoint>& brks) override;
    std::list<ThreadInfo> parseThreadList(const char* output) override;
    bool parseBreakpoint(const char* output, int& id,
				 QString& file, int& lineNo, QString& address) override;
    void parseLocals(const char* output, std::list<ExprValue*>& newVars) override;
    ExprValue* parsePrintExpr(const char* output, bool wantErrorValue) override;
    bool parseChangeWD(const char* output, QString& message) override;
    bool parseChangeExecutable(const char* output, QString& message) override;
    bool parseCoreFile(const char* output) override;
    uint parseProgramStopped(const char* output, bool haveCoreFile,
				     QString& message) override;
    QStringList parseSharedLibs(const char* output) override;
    bool parseFindType(const char* output, QString& type) override;
    std::list<RegisterInfo> parseRegisters(const char* output) override;
    bool parseInfoLine(const char* output,
			       QString& addrFrom, QString& addrTo) override;
    QString parseInfoTarget(const char* output) override;
    std::list<DisassembledCode> parseDisassemble(const char* output) override;
    QString parseMemoryDump(const char* output, std::list<MemoryDump>& memdump) override;
    QString parseSetVariable(const char* output) override;
    QString editableValue(VarTree* value) override;
    QString parseSetDisassFlavor(const char* output) override;
protected:
    QString m_programWD;		/* just an intermediate storage */
    QString m_redirect;			/* redirection to /dev/null */
    bool m_littleendian = true;		/* if gdb works with little endian or big endian */
    QString m_defaultCmd;		/* how to invoke gdb */

    QString makeCmdString(DbgCommand cmd) override;
    QString makeCmdString(DbgCommand cmd, QString strArg) override;
    QString makeCmdString(DbgCommand cmd, int intArg) override;
    QString makeCmdString(DbgCommand cmd, QString strArg, int intArg) override;
    QString makeCmdString(DbgCommand cmd, QString strArg1, QString strArg2) override;
    QString makeCmdString(DbgCommand cmd, int intArg1, int intArg2) override;
    QString makeCmdString(DbgCommand cmd, QString strArg, int intArg1, int intArg2) override;
    int findPrompt(const QByteArray& output) const override;
    void parseMarker(CmdQueueItem* cmd);
};

#endif // GDBDRIVER_H
