/*
 * Copyright Johannes Sixt, Keith Isdale
 * This file is licensed under the GNU General Public License Version 2.
 * See the file COPYING in the toplevel directory of the source directory.
 */

#ifndef XSLDBGDRIVER_H
#define XSLDBGDRIVER_H

#include "dbgdriver.h"
#include "qregexp.h"


class XsldbgDriver:public DebuggerDriver {
  Q_OBJECT public:
    XsldbgDriver();
    ~XsldbgDriver();

    virtual QString driverName() const;
    virtual QString defaultInvocation() const;
    virtual QStringList boolOptionList() const;
    static QString defaultXsldbg();
    virtual bool startup(QString cmdStr);
    virtual void commandFinished(CmdQueueItem * cmd);
    void slotReceiveOutput(KProcess * process, char *buffer, int buflen);

    virtual CmdQueueItem *executeCmd(DbgCommand, bool clearLow = false);
    virtual CmdQueueItem *executeCmd(DbgCommand, QString strArg,
                                     bool clearLow = false);
    virtual CmdQueueItem *executeCmd(DbgCommand, int intArg,
                                     bool clearLow = false);
    virtual CmdQueueItem *executeCmd(DbgCommand, QString strArg,
                                     int intArg, bool clearLow = false);
    virtual CmdQueueItem *executeCmd(DbgCommand, QString strArg1,
                                     QString strArg2, bool clearLow =
                                     false);
    virtual CmdQueueItem *executeCmd(DbgCommand, int intArg1, int intArg2,
                                     bool clearLow = false);
    virtual CmdQueueItem *queueCmd(DbgCommand, QueueMode mode);
    virtual CmdQueueItem *queueCmd(DbgCommand, QString strArg,
                                   QueueMode mode);
    virtual CmdQueueItem *queueCmd(DbgCommand, int intArg, QueueMode mode);
    virtual CmdQueueItem *queueCmd(DbgCommand, QString strArg, int intArg,
                                   QueueMode mode);
    virtual CmdQueueItem *queueCmd(DbgCommand, QString strArg1,
                                   QString strArg2, QueueMode mode);

    virtual void terminate();
    virtual void detachAndTerminate();
    virtual void interruptInferior();

    virtual void setPrintQStringDataCmd(const char* cmd);
    /**
     * Parses the output as an array of QChars.
     */
    virtual ExprValue *parseQCharArray(const char *output,
                                     bool wantErrorValue, bool qt3like);

    virtual void parseBackTrace(const char *output,
                                std::list < StackFrame > &stack);
    virtual bool parseFrameChange(const char *output, int &frameNo,
                                  QString & file, int &lineNo,
                                  DbgAddr & address);
    virtual bool parseBreakList(const char *output,
                                std::list < Breakpoint > &brks);
    virtual std::list<ThreadInfo> parseThreadList(const char *output);
    virtual bool parseBreakpoint(const char *output, int &id,
                                 QString & file, int &lineNo, QString& address);
    virtual void parseLocals(const char *output,
                             std::list < ExprValue* > &newVars);
    virtual ExprValue * parsePrintExpr(const char *output, bool wantErrorValue);
    virtual bool parseChangeWD(const char *output, QString & message);
    virtual bool parseChangeExecutable(const char *output,
                                       QString & message);
    virtual bool parseCoreFile(const char *output);
    virtual uint parseProgramStopped(const char *output,
                                     QString & message);
    virtual QStringList parseSharedLibs(const char *output);
    virtual bool parseFindType(const char *output, QString & type);
    virtual std::list<RegisterInfo> parseRegisters(const char *output);
    virtual bool parseInfoLine(const char *output, QString & addrFrom,
                               QString & addrTo);
    virtual std::list<DisassembledCode> parseDisassemble(const char *output);
    virtual QString parseMemoryDump(const char *output,
                                    std::list < MemoryDump > &memdump);
    virtual QString parseSetVariable(const char* output);

  protected:
    int m_gdbMajor, m_gdbMinor;
    QString m_programWD;        /* just an intermediate storage */
    QString m_xslFile;		/* needed to display it initially */
    bool m_haveDataFile;       /* have we set the XML data file to use? */ 
    QString m_redirect;         /* redirection to /dev/null */
    bool m_haveCoreFile;
    QRegExp m_markerRE;

    QString makeCmdString(DbgCommand cmd, QString strArg);
    QString makeCmdString(DbgCommand cmd, int intArg);
    QString makeCmdString(DbgCommand cmd, QString strArg, int intArg);
    QString makeCmdString(DbgCommand cmd, QString strArg1,
                          QString strArg2);
    QString makeCmdString(DbgCommand cmd, int intArg1, int intArg2);
    void parseMarker();
};

#endif // XSLDBGDRIVER_H
