/*
 * Copyright Johannes Sixt, Keith Isdale
 * This file is licensed under the GNU General Public License Version 2.
 * See the file COPYING in the toplevel directory of the source directory.
 */

#include "xsldbgdriver.h"
#include "exprwnd.h"
#include <qstringlist.h>
#include <klocale.h>            /* i18n */
#include <ctype.h>
#include <stdlib.h>             /* strtol, atoi */
#include <string.h>             /* strcpy */
#include <kmessagebox.h>

#include "assert.h"
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include "mydebug.h"


static ExprValue *parseVar(const char *&s);
static bool parseName(const char *&s, QString & name,
                      VarTree::NameKind & kind);
static bool parseValue(const char *&s, ExprValue * variable);
static bool isErrorExpr(const char *output);

#define TERM_IO_ALLOWED 1

// TODO: make this cmd info stuff non-static to allow multiple
// simultaneous gdbs to run!

struct XsldbgCmdInfo {
    DbgCommand cmd;
    const char *fmt;            /* format string */
    enum Args {
        argNone, argString, argNum,
        argStringNum, argNumString,
        argString2, argNum2
    } argsNeeded;
};

/*
 * The following array of commands must be sorted by the DC* values,
 * because they are used as indices.
 */
static XsldbgCmdInfo cmds[] = {
    {DCinitialize, "init\n", XsldbgCmdInfo::argNone},
    {DCtty, "tty %s\n", XsldbgCmdInfo::argString},
    {DCexecutable, "source %s\n", XsldbgCmdInfo::argString},    /* force a restart */
    {DCtargetremote, "print 'target remote %s'\n", XsldbgCmdInfo::argString},
    {DCcorefile, "data  %s\n", XsldbgCmdInfo::argString},       /* force a restart */
    {DCattach, "print 'attach %s'\n", XsldbgCmdInfo::argString},
    {DCinfolinemain, "print 'info main line'\n", XsldbgCmdInfo::argNone},
    {DCinfolocals, "locals -f\n", XsldbgCmdInfo::argNone},
    {DCinforegisters, "print 'info reg'\n", XsldbgCmdInfo::argNone},
    {DCexamine, "print 'x %s %s'\n", XsldbgCmdInfo::argString2},
    {DCinfoline, "print 'templates %s:%d'\n", XsldbgCmdInfo::argStringNum},
    {DCdisassemble, "print 'disassemble %s %s'\n", XsldbgCmdInfo::argString2},
    {DCsetargs, "data %s\n", XsldbgCmdInfo::argString},
    {DCsetenv, "addparam %s %s\n", XsldbgCmdInfo::argString2},
    {DCunsetenv, "unset env %s\n", XsldbgCmdInfo::argString},
    {DCsetoption, "setoption %s %d\n", XsldbgCmdInfo::argStringNum},
    {DCcd, "chdir %s\n", XsldbgCmdInfo::argString},
    {DCbt, "where\n", XsldbgCmdInfo::argNone},
    {DCrun, "run\nsource\n", XsldbgCmdInfo::argNone}, /* Ensure that at the start
							 of executing XSLT we show the XSLT file */
    {DCcont, "continue\n", XsldbgCmdInfo::argNone},
    {DCstep, "step\n", XsldbgCmdInfo::argNone},
    {DCstepi, "step\n", XsldbgCmdInfo::argNone},
    {DCnext, "next\n", XsldbgCmdInfo::argNone},
    {DCnexti, "next\n", XsldbgCmdInfo::argNone},
    {DCfinish, "stepup\n", XsldbgCmdInfo::argNone},
    {DCuntil, "continue %s:%d\n", XsldbgCmdInfo::argStringNum},
    {DCkill, "quit\n", XsldbgCmdInfo::argNone},
    {DCbreaktext, "break %s\n", XsldbgCmdInfo::argString},
    {DCbreakline, "break -l %s %d\n", XsldbgCmdInfo::argStringNum},
    {DCtbreakline, "break -l %s %d\n", XsldbgCmdInfo::argStringNum },
    {DCbreakaddr, "print `break *%s`\n", XsldbgCmdInfo::argString },
    {DCtbreakaddr, "print `tbreak *%s`\n", XsldbgCmdInfo::argString },
    {DCwatchpoint, "print 'watch %s'\n", XsldbgCmdInfo::argString},
    {DCdelete, "delete %d\n", XsldbgCmdInfo::argNum},
    {DCenable, "enable %d\n", XsldbgCmdInfo::argNum},
    {DCdisable, "disable %d\n", XsldbgCmdInfo::argNum},
    {DCprint, "print %s\n", XsldbgCmdInfo::argString},
    {DCprintDeref, "print 'print (*%s)'\n", XsldbgCmdInfo::argString},
    {DCprintStruct, "print 'print %s'\n", XsldbgCmdInfo::argString},
    {DCprintQStringStruct, "print 'print %s'\n", XsldbgCmdInfo::argString},
    {DCframe, "frame %d\n", XsldbgCmdInfo::argNum},
    {DCfindType, "print 'whatis %s'\n", XsldbgCmdInfo::argString},
    {DCinfosharedlib, "stylesheets\n", XsldbgCmdInfo::argNone},
    {DCthread, "print 'thread %d'\n", XsldbgCmdInfo::argNum},
    {DCinfothreads, "print 'info threads'\n", XsldbgCmdInfo::argNone},
    {DCinfobreak, "show\n", XsldbgCmdInfo::argNone},
    {DCcondition, "print 'condition %d %s'\n", XsldbgCmdInfo::argNumString},
    {DCsetpc, "print 'set variable $pc=%s'\n", XsldbgCmdInfo::argString},
    {DCignore, "print 'ignore %d %d'\n", XsldbgCmdInfo::argNum2},
    {DCprintWChar, "print 'ignore %s'\n", XsldbgCmdInfo::argString},
    {DCsetvariable, "set %s %s\n", XsldbgCmdInfo::argString2},
};

#define NUM_CMDS (int(sizeof(cmds)/sizeof(cmds[0])))
#define MAX_FMTLEN 200

XsldbgDriver::XsldbgDriver():
DebuggerDriver(), m_gdbMajor(2), m_gdbMinor(0)
{
    m_promptRE.setPattern("\\(xsldbg\\) .*> ");
    m_promptMinLen = 11;
    m_promptLastChar = ' ';
  
    m_markerRE.setPattern("^Breakpoint for file ");
    m_haveDataFile = FALSE;

#ifndef NDEBUG
    // check command info array
    char *perc;

    for (int i = 0; i < NUM_CMDS; i++) {
        // must be indexable by DbgCommand values, i.e. sorted by DbgCommand values
        assert(i == cmds[i].cmd);
        // a format string must be associated
        assert(cmds[i].fmt != 0);
        assert(strlen(cmds[i].fmt) <= MAX_FMTLEN);
        // format string must match arg specification
        switch (cmds[i].argsNeeded) {
            case XsldbgCmdInfo::argNone:
                assert(strchr(cmds[i].fmt, '%') == 0);
                break;
            case XsldbgCmdInfo::argString:
                perc = strchr(cmds[i].fmt, '%');
                assert(perc != 0 && perc[1] == 's');
                assert(strchr(perc + 2, '%') == 0);
                break;
            case XsldbgCmdInfo::argNum:
                perc = strchr(cmds[i].fmt, '%');
                assert(perc != 0 && perc[1] == 'd');
                assert(strchr(perc + 2, '%') == 0);
                break;
            case XsldbgCmdInfo::argStringNum:
                perc = strchr(cmds[i].fmt, '%');
                assert(perc != 0 && perc[1] == 's');
                perc = strchr(perc + 2, '%');
                assert(perc != 0 && perc[1] == 'd');
                assert(strchr(perc + 2, '%') == 0);
                break;
            case XsldbgCmdInfo::argNumString:
                perc = strchr(cmds[i].fmt, '%');
                assert(perc != 0 && perc[1] == 'd');
                perc = strchr(perc + 2, '%');
                assert(perc != 0 && perc[1] == 's');
                assert(strchr(perc + 2, '%') == 0);
                break;
            case XsldbgCmdInfo::argString2:
                perc = strchr(cmds[i].fmt, '%');
                assert(perc != 0 && perc[1] == 's');
                perc = strchr(perc + 2, '%');
                assert(perc != 0 && perc[1] == 's');
                assert(strchr(perc + 2, '%') == 0);
                break;
            case XsldbgCmdInfo::argNum2:
                perc = strchr(cmds[i].fmt, '%');
                assert(perc != 0 && perc[1] == 'd');
                perc = strchr(perc + 2, '%');
                assert(perc != 0 && perc[1] == 'd');
                assert(strchr(perc + 2, '%') == 0);
                break;
        }
    }
#endif
}

XsldbgDriver::~XsldbgDriver()
{
}


QString
XsldbgDriver::driverName() const
{
    return "XSLDBG";
}

QString
XsldbgDriver::defaultXsldbg()
{
    return "xsldbg --lang en --shell --gdb";
}

QString
XsldbgDriver::defaultInvocation() const
{
    return defaultXsldbg();
}

QStringList XsldbgDriver::boolOptionList() const
{
    QStringList allOptions;
    allOptions.append("verbose");
    allOptions.append("repeat");
    allOptions.append("debug");
    allOptions.append("novalid");
    allOptions.append("noout");
    allOptions.append("html");
    allOptions.append("docbook");
    allOptions.append("nonet");
    allOptions.append("catalogs");
    allOptions.append("xinclude");
    allOptions.append("profile");
    return allOptions;
}


void
XsldbgDriver::slotReceiveOutput(KProcess * process, char *buffer,
                                int buflen)
{
    //TRACE(buffer);
    if (m_state != DSidle) {
        //    TRACE(buffer);
        DebuggerDriver::slotReceiveOutput(process, buffer, buflen);
    } else {
        if (strncmp(buffer, "quit", 4) == 0) {
            TRACE("Ignoring text when xsldbg is quiting");
        } else {
            TRACE
                ("Stray output received by XsldbgDriver::slotReceiveOutput");
            TRACE(buffer);
        }
    }
}

bool
XsldbgDriver::startup(QString cmdStr)
{
    if (!DebuggerDriver::startup(cmdStr))
	return false;

    static const char xsldbgInitialize[] = "pwd\nsetoption gdb 2\n";     /* don't need to do anything else */

    executeCmdString(DCinitialize, xsldbgInitialize, false);

    return true;
}

void
XsldbgDriver::commandFinished(CmdQueueItem * cmd)
{

    TRACE(__PRETTY_FUNCTION__);
    // command string must be committed
    if (!cmd->m_committed) {
        // not commited!
        TRACE("calling " +
              (__PRETTY_FUNCTION__ +
               (" with uncommited command:\n\t" + cmd->m_cmdString)));
        return;
    }

    switch (cmd->m_cmd) {
        case DCinitialize:
            // get version number from preamble
            {
                int len;
                QRegExp xsldbgVersion("^XSLDBG [0-9]+\\.[0-9]+\\.[0-9]+");
                int offset = xsldbgVersion.match(m_output, 0, &len);

                if (offset >= 0) {
                    char *start = m_output + offset + 7;        // skip "^XSLDBG "
                    char *end;

                    TRACE("Reading version");
                    TRACE(start);
                    m_gdbMajor = strtol(start, &end, 10);
                    m_gdbMinor = strtol(end + 1, 0, 10);        // skip "."
                    if (start == end) {
                        // nothing was parsed
                        m_gdbMajor = 0;
                        m_gdbMinor = 7;
                    }
                } else {
                    // assume some default version (what would make sense?)
                    m_gdbMajor = 0;
                    m_gdbMinor = 7;
                }
                TRACE(QString("Got version ") +
                      QString::number(m_gdbMajor) + "." +
                      QString::number(m_gdbMinor));
                break;
            }
        default:;
    }
    /* ok, the command is ready */
    emit commandReceived(cmd, m_output);

    switch (cmd->m_cmd) {
        case DCbt:
        case DCinfolocals:
        case DCrun:
        case DCcont:
        case DCstep:
        case DCnext:
        case DCfinish:{
	  if (!::isErrorExpr(m_output))
            parseMarker();
	  else{
	    // This only shows an error for DCinfolocals 
	    //  need to update KDebugger::handleRunCommand ? 
	    KMessageBox::sorry(0L, m_output);
	  }
	}
            break;

       case DCinfolinemain:
	    if (!m_xslFile.isEmpty())
		emit activateFileLine(m_xslFile, 0, DbgAddr());
	    break;

        default:;
    }
}

void
XsldbgDriver::parseMarker()
{

    // TRACE("parseMarker : xsldbg");
    //  TRACE(m_output);
    int len, markerStart = -1;
    char *p = m_output;

    while (markerStart == -1) {
        if ((p == 0) || (*p == '\0')) {
            m_output[0] = '\0';
            return;
        }
        //TRACE(QString("parseMarker is looking at :") + p);
        markerStart = m_markerRE.match(p, 0, &len);
        if (markerStart == -1) {
            // try to marker on next line !
            p = strchr(p, '\n');
            if ((p != 0) && (*p != '\0'))
                p++;
        }
    }


    // extract the marker
    char *startMarker = p + markerStart + len;

    //TRACE(QString("found marker:") + startMarker);
    char *endMarker = strchr(startMarker, '\n');

    if (endMarker == 0)
        return;

    *endMarker = '\0';

    // extract filename and line number
    static QRegExp MarkerRE(" at line [0-9]+");

    int lineNoStart = MarkerRE.match(startMarker, 0, &len);

    if (lineNoStart >= 0) {
        int lineNo = atoi(startMarker + lineNoStart + 8);

        DbgAddr address;

        // now show the window
        startMarker[lineNoStart-1] = '\0';        /* split off file name */
        TRACE("Got file and line number");
	startMarker++;
        TRACE(QString(startMarker) + ": " +  QString::number(lineNo));
        emit activateFileLine(startMarker, lineNo - 1, address);
    }
}


/*
 * Escapes characters that might lead to problems when they appear on gdb's
 * command line.
 */
static void
normalizeStringArg(QString & arg)
{
    /*
     * Remove trailing backslashes. This approach is a little simplistic,
     * but we know that there is at the moment no case where a trailing
     * backslash would make sense.
     */
    while (!arg.isEmpty() && arg[arg.length() - 1] == '\\') {
        arg = arg.left(arg.length() - 1);
    }
}


QString
XsldbgDriver::makeCmdString(DbgCommand cmd, QString strArg)
{
    assert(cmd >= 0 && cmd < NUM_CMDS);
    assert(cmds[cmd].argsNeeded == XsldbgCmdInfo::argString);

    normalizeStringArg(strArg);

    if (cmd == DCcd) {
        // need the working directory when parsing the output
        m_programWD = strArg;
    } else if (cmd == DCexecutable) {
        // want to display the XSL file
	m_xslFile = strArg;
    }

    QString cmdString;
    cmdString.sprintf(cmds[cmd].fmt, strArg.latin1());
    return cmdString;
}

QString
XsldbgDriver::makeCmdString(DbgCommand cmd, int intArg)
{
    assert(cmd >= 0 && cmd < NUM_CMDS);
    assert(cmds[cmd].argsNeeded == XsldbgCmdInfo::argNum);

    QString cmdString;
    cmdString.sprintf(cmds[cmd].fmt, intArg);
    return cmdString;
}

QString
XsldbgDriver::makeCmdString(DbgCommand cmd, QString strArg, int intArg)
{
    assert(cmd >= 0 && cmd < NUM_CMDS);
    assert(cmds[cmd].argsNeeded == XsldbgCmdInfo::argStringNum ||
           cmds[cmd].argsNeeded == XsldbgCmdInfo::argNumString ||
           cmd == DCexamine || cmd == DCtty);

    normalizeStringArg(strArg);

    QString cmdString;

    if (cmd == DCtty) {
        /*
         * intArg specifies which channels should be redirected to
         * /dev/null. It is a value or'ed together from RDNstdin,
         * RDNstdout, RDNstderr.
         */
        static const char *const runRedir[8] = {
            "",
            " </dev/null",
            " >/dev/null",
            " </dev/null >/dev/null",
            " 2>/dev/null",
            " </dev/null 2>/dev/null",
            " >/dev/null 2>&1",
            " </dev/null >/dev/null 2>&1"
        };

        if (strArg.isEmpty())
            intArg = 7;         /* failsafe if no tty */
        m_redirect = runRedir[intArg & 7];

        return makeCmdString(DCtty, strArg);    /* note: no problem if strArg empty */
    }

    if (cmd == DCexamine) {
        // make a format specifier from the intArg
        static const char size[16] = {
            '\0', 'b', 'h', 'w', 'g'
        };
        static const char format[16] = {
            '\0', 'x', 'd', 'u', 'o', 't',
            'a', 'c', 'f', 's', 'i'
        };

        assert(MDTsizemask == 0xf);     /* lowest 4 bits */
        assert(MDTformatmask == 0xf0);  /* next 4 bits */
        int count = 16;         /* number of entities to print */
        char sizeSpec = size[intArg & MDTsizemask];
        char formatSpec = format[(intArg & MDTformatmask) >> 4];

        assert(sizeSpec != '\0');
        assert(formatSpec != '\0');
        // adjust count such that 16 lines are printed
        switch (intArg & MDTformatmask) {
            case MDTstring:
            case MDTinsn:
                break;          /* no modification needed */
            default:
                // all cases drop through:
                switch (intArg & MDTsizemask) {
                    case MDTbyte:
                    case MDThalfword:
                        count *= 2;
                    case MDTword:
                        count *= 2;
                    case MDTgiantword:
                        count *= 2;
                }
                break;
        }
        QString spec;

        spec.sprintf("/%d%c%c", count, sizeSpec, formatSpec);

        return makeCmdString(DCexamine, spec, strArg);
    }

    if (cmds[cmd].argsNeeded == XsldbgCmdInfo::argStringNum) {
        // line numbers are zero-based
        if (cmd == DCuntil || cmd == DCbreakline ||
            cmd == DCtbreakline || cmd == DCinfoline) {
            intArg++;
        }
        if (cmd == DCinfoline) {
            // must split off file name part
            int slash = strArg.findRev('/');

            if (slash >= 0)
                strArg = strArg.right(strArg.length() - slash - 1);
        }
        cmdString.sprintf(cmds[cmd].fmt, strArg.latin1(), intArg);
    } else {
        cmdString.sprintf(cmds[cmd].fmt, intArg, strArg.latin1());
    }
    return cmdString;
}

QString
XsldbgDriver::makeCmdString(DbgCommand cmd, QString strArg1,
                            QString strArg2)
{
    assert(cmd >= 0 && cmd < NUM_CMDS);
    assert(cmds[cmd].argsNeeded == XsldbgCmdInfo::argString2);

    normalizeStringArg(strArg1);
    normalizeStringArg(strArg2);

    QString cmdString;
    cmdString.sprintf(cmds[cmd].fmt, strArg1.latin1(), strArg2.latin1());
    return cmdString;
}

QString
XsldbgDriver::makeCmdString(DbgCommand cmd, int intArg1, int intArg2)
{
    assert(cmd >= 0 && cmd < NUM_CMDS);
    assert(cmds[cmd].argsNeeded == XsldbgCmdInfo::argNum2);

    QString cmdString;
    cmdString.sprintf(cmds[cmd].fmt, intArg1, intArg2);
    return cmdString;
}

CmdQueueItem *
XsldbgDriver::executeCmd(DbgCommand cmd, bool clearLow)
{
    assert(cmd >= 0 && cmd < NUM_CMDS);
    assert(cmds[cmd].argsNeeded == XsldbgCmdInfo::argNone);

    if (cmd == DCrun) {
        m_haveCoreFile = false;
    }

    return executeCmdString(cmd, cmds[cmd].fmt, clearLow);
}

CmdQueueItem *
XsldbgDriver::executeCmd(DbgCommand cmd, QString strArg, bool clearLow)
{
    return executeCmdString(cmd, makeCmdString(cmd, strArg), clearLow);
}

CmdQueueItem *
XsldbgDriver::executeCmd(DbgCommand cmd, int intArg, bool clearLow)
{

    return executeCmdString(cmd, makeCmdString(cmd, intArg), clearLow);
}

CmdQueueItem *
XsldbgDriver::executeCmd(DbgCommand cmd, QString strArg, int intArg,
                         bool clearLow)
{
    return executeCmdString(cmd, makeCmdString(cmd, strArg, intArg),
                            clearLow);
}

CmdQueueItem *
XsldbgDriver::executeCmd(DbgCommand cmd, QString strArg1, QString strArg2,
                         bool clearLow)
{
    return executeCmdString(cmd, makeCmdString(cmd, strArg1, strArg2),
                            clearLow);
}

CmdQueueItem *
XsldbgDriver::executeCmd(DbgCommand cmd, int intArg1, int intArg2,
                         bool clearLow)
{
    return executeCmdString(cmd, makeCmdString(cmd, intArg1, intArg2),
                            clearLow);
}

CmdQueueItem *
XsldbgDriver::queueCmd(DbgCommand cmd, QueueMode mode)
{
    return queueCmdString(cmd, cmds[cmd].fmt, mode);
}

CmdQueueItem *
XsldbgDriver::queueCmd(DbgCommand cmd, QString strArg, QueueMode mode)
{
    return queueCmdString(cmd, makeCmdString(cmd, strArg), mode);
}

CmdQueueItem *
XsldbgDriver::queueCmd(DbgCommand cmd, int intArg, QueueMode mode)
{
    return queueCmdString(cmd, makeCmdString(cmd, intArg), mode);
}

CmdQueueItem *
XsldbgDriver::queueCmd(DbgCommand cmd, QString strArg, int intArg,
                       QueueMode mode)
{
    return queueCmdString(cmd, makeCmdString(cmd, strArg, intArg), mode);
}

CmdQueueItem *
XsldbgDriver::queueCmd(DbgCommand cmd, QString strArg1, QString strArg2,
                       QueueMode mode)
{
    return queueCmdString(cmd, makeCmdString(cmd, strArg1, strArg2), mode);
}

void
XsldbgDriver::terminate()
{
    qDebug("XsldbgDriver::Terminate");
    flushCommands();
    executeCmdString(DCinitialize, "quit\n", true);
    kill(SIGTERM);
    m_state = DSidle;
}

void
XsldbgDriver::detachAndTerminate()
{
    qDebug("XsldbgDriver::detachAndTerminate");
    flushCommands();
    executeCmdString(DCinitialize, "quit\n", true);
    kill(SIGINT);
}

void
XsldbgDriver::interruptInferior()
{
    // remove accidentally queued commands
    qDebug("interruptInferior");
    flushHiPriQueue();
    kill(SIGINT);
}

static bool
isErrorExpr(const char *output)
{
    int  wordIndex;
    bool result = false;
#define   ERROR_WORD_COUNT 6
    static const char *errorWords[ERROR_WORD_COUNT] = {
      "Error:", 
      "error:", // libxslt error 
      "Unknown command",  
      "Warning:",
      "warning:", // libxslt warning
      "Information:" // xsldbg information	
    };
    static int errorWordLength[ERROR_WORD_COUNT] = {
      6,  /* Error */
      6,  /* rror */
      15, /* Unknown command*/ 
      8,  /* Warning */
      8,  /* warning */
      12  /* Information */
    };
    
    for (wordIndex = 0; wordIndex < ERROR_WORD_COUNT; wordIndex++){
      // ignore any warnings relating to local variables not being available
      if (strncmp(output, 
		  errorWords[wordIndex], 
		  errorWordLength[wordIndex]) == 0 && 
		    (wordIndex == 0 && strstr(output, "try stepping past the xsl:param") == 0) )   {
	result = true;
        TRACE(QString("Error/Warning/Information from xsldbg ") + output);
	break;
      }
    }

    return result;
}

/**
 * Returns true if the output is an error message. If wantErrorValue is
 * true, a new ExprValue object is created and filled with the error message.
 */
static bool
parseErrorMessage(const char *output,
                  ExprValue * &variable, bool wantErrorValue)
{
    if (isErrorExpr(output)) {
        if (wantErrorValue) {
            // put the error message as value in the variable
            variable = new ExprValue(QString(), VarTree::NKplain);
            const char *endMsg = strchr(output, '\n');

            if (endMsg == 0)
                endMsg = output + strlen(output);
            variable->m_value = QString::fromLatin1(output, endMsg - output);
        } else {
            variable = 0;
        }
        return true;
    }
    return false;
}


void
XsldbgDriver::setPrintQStringDataCmd(const char* /*cmd*/)
{
}

ExprValue *
XsldbgDriver::parseQCharArray(const char */*output*/, bool /*wantErrorValue*/,
                              bool /*qt3like*/)
{
    ExprValue *variable = 0;

    TRACE("XsldbgDriver::parseQCharArray not implmented");
    return variable;
}

static ExprValue *
parseVar(const char *&s)
{
    const char *p = s;
    bool foundLocalVar = false;
    ExprValue *variable = 0L;
    QString name;

    VarTree::NameKind kind;

    TRACE(__PRETTY_FUNCTION__);
    TRACE(p);

    if (parseErrorMessage(p, variable, false) == true) {
        TRACE("Found error message");
        return variable;
    }

    if (strncmp(p, " Local", 6) == 0) {
        foundLocalVar = true;
        /* skip " Local" */
        p = p + 6;
        TRACE("Found local variable");
    } else if (strncmp(p, " Global", 7) == 0) {
        /* skip " Global" */
        p = p + 7;
	TRACE("Found global variable");
    } else if (strncmp(p, "= ", 2) == 0) {
        /* we're processing the result of a "print command" */
        /* find next line */
        char *nextLine = strchr(p, '\n');

	TRACE("Found print expr");
        if (nextLine) {
            p = p + 2;          /* skip the "= " */
            name = QString::fromLatin1(p, nextLine - p);
            kind = VarTree::NKplain;
            p = nextLine + 1;
            variable = new ExprValue(name, kind);
            variable->m_varKind = VarTree::VKsimple;
            parseValue(p, variable);
            return variable;
        }
    } else
        return variable;        /* don't know what to do this this data abort!! */

    // skip whitespace
    while (isspace(*p))
        p++;

    if (*p != '='){
      // No value provided just a name
      TRACE(QString("Parse var: name") + p);
      if (!parseName(p, name, kind)) {
        return 0;
      }
      variable = new ExprValue(name, kind);
      if (variable != 0L) {
	  variable->m_varKind = VarTree::VKsimple;
      }
    }else{
      p++;
      // skip whitespace
      while (isspace(*p))
        p++;
      TRACE(QString("Parse var: name") + p);
      if (!parseName(p, name, kind)) {
        return 0;
      }
      variable = new ExprValue(name, kind);
      if (variable != 0L) {
	  variable->m_varKind = VarTree::VKsimple;
      }
      if (*p == '\n')
	p++;
      if (!parseValue(p, variable)) {
	delete variable;
	return 0;
      }
    }

    if (*p == '\n')
        p++;

    s = p;
    return variable;
}


inline void
skipName(const char *&p)
{
    // allow : (for enumeration values) and $ and . (for _vtbl.)
    while (isalnum(*p) || *p == '_' || *p == ':' || *p == '$' || *p == '.')
        p++;
}

static bool
parseName(const char *&s, QString & name, VarTree::NameKind & kind)
{
    /*    qDebug(__PRETTY_FUNCTION__); */
    kind = VarTree::NKplain;

    const char *p = s;
    int len = 0;

    // examples of names:
    //  help_cmd

    while ((*p != '\n') && (*p != '\0')) {
        len++;
        p++;
    }


    name = QString::fromLatin1(s, len);
    /* XSL variables will have a $ prefix to be evaluated 
     * properly */
    //TRACE(QString("parseName got name" ) +  name);

    // return the new position
    s = p;
    return true;
}

static bool
parseValue(const char *&s, ExprValue * variable)
{
    const char *start = s, *end = s;
    ExprValue * childValue; 
    #define VALUE_END_MARKER_INDEX  0

    /* This mark the end of a value */
    static const char *marker[] = { 
         "\032\032",            /* value end marker*/   
         "(xsldbg) ",
        "Breakpoint at",        /* stepped to next location */
        "Breakpoint in",        /* reached a set breakpoint */
        "Reached ",             /* reached template */
        "Error:",
	 "Warning:",
	 "Information:",
        "runtime error",
	 "xmlXPathEval:",
	0		     				     
    };
    static char valueBuffer[2048];
    int markerIndex = 0, foundEnd = 0;
    size_t copySize;

    if (variable == 0L)
        return false;           /* should never happen but .. */

    while (start && (*start != '\0')) {
        /* look for the next marker */
        for (markerIndex = 0; marker[markerIndex] != 0; markerIndex++) {
            foundEnd =
                strncmp(start, marker[markerIndex],
                        strlen(marker[markerIndex])) == 0;
            if (foundEnd)
                break;
        }

        if (foundEnd)
            break;


        end = strchr(start, '\n');
        if (end) 
            copySize = end - start;
	else
	    copySize = strlen(start);
	if (copySize >= sizeof(valueBuffer))
	    copySize = sizeof(valueBuffer)-1;

	strncpy(valueBuffer, start, copySize);
	valueBuffer[copySize] = '\0';
	TRACE("Got value :");
	TRACE(valueBuffer);
	if ((variable->m_varKind == VarTree::VKsimple)) {
	    if (!variable->m_value.isEmpty()){
		variable->m_varKind = VarTree::VKarray;
		childValue = new ExprValue(variable->m_value, VarTree::NKplain);
		variable->appendChild(childValue);
		childValue = new ExprValue(valueBuffer, VarTree::NKplain);
		variable->appendChild(childValue);
		variable->m_value = "";
	    }else{
		variable->m_value = valueBuffer;
	    }
	}else{
	    childValue = new ExprValue(valueBuffer, VarTree::NKplain);
	    variable->appendChild(childValue);
	}

	if (*end =='\n'){
	    start = end + 1;
	}else{
	    start = end + 1;
	    break;
	}
    }

    if (foundEnd == 0)
      TRACE(QString("Unable to find end on value near :") + start);

    // If we've got something otherthan a end of value marker then
    //  advance to the end of this buffer
    if (markerIndex != VALUE_END_MARKER_INDEX){
      while (start && *start != '\0')
        start++;
    }else{
      start = start + strlen(marker[0]);
    }

    s = start;

    return true;
}


/**
 * Parses a stack frame.
 */
static void
parseFrameInfo(const char *&s, QString & func,
               QString & file, int &lineNo, DbgAddr & /*address*/)
{
    const char *p = s, *endPos = s + strlen(s);
    QString lineNoString;

    TRACE("parseFrameInfo");

    lineNo = -1;

    /* skip 'template :\" */
    p = p + 11;
    //    TRACE(p);
    func = "";
    while ((*p != '\"') && (*p != '\0')) {
        func.append(*p);
        p++;
    }
    while ((*p != '\0') && *p != '"')
      p++;
    if (*p != '\0')
      p++;
    ASSERT(p <= endPos);
    if (p >= endPos) {
        /* panic */
        return;
    }

    /* skip  mode :".*" */
    while ((*p != '\0') && *p != '"')
      p++;   
    if (*p != '\0')
      p++;
    while ((*p != '\0')  && *p != '"')
      p++;
 
    /* skip '" in file ' */
    p = p + 10;
    if(*p == '"')
      p++; 
    //   TRACE(p);
    file = "";
    while (!isspace(*p) && (*p != '\"') && (*p != '\0')) {
        file.append(*p);
        p++;
    }
    if(*p == '"')
      p++; 
    ASSERT(p <= endPos);
    if (p >= endPos) {
        /* panic */
        return;
    }

    //    TRACE(p);
    /* skip  ' : line '" */
    p = p + 9;
    //    TRACE(p);
    ASSERT(p <= endPos);
    if (p >= endPos) {
        /* panic */
        return;
    }
    //    TRACE(p);
    if (isdigit(*p)) {
        /* KDbg uses an offset of +1 for its line numbers */
        lineNo = atoi(p) - 1;
	lineNoString = QString::number(lineNo);
    }
    /* convert func into format needed */
    func.append(" at ");
    func.append(file);
    func.append(':');
    func.append(lineNoString);

    /*advance to next line */
    p = strchr(p, '\n');
    if (p)
        p++;
    s = p;

}

#undef ISSPACE

/**
 * Parses a stack frame including its frame number
 */
static bool
parseFrame(const char *&s, int &frameNo, QString & func,
           QString & file, int &lineNo, DbgAddr & address)
{

    // TRACE("XsldbgDriver ::parseFrame");
    /* skip leading 'where' or 'frame <frame_no>' */
    if ((strncmp(s, "where", 5) == 0) || (strncmp(s, "frame", 5) == 0)) {
        s = strchr(s, '\n');
        if ((*s != '\0') && (*s != '#'))
            s++;
    }
    // TRACE(s);

    // Example:
    //#1 template :"/" in file /home/keith/anon_CVS/xsldbg/docs/en/xsldoc.xsl : line 21
    // must start with a hash mark followed by number
    if (s[0] != '#' || !isdigit(s[1]))
        return false;

    //TRACE("XsldbgDriver ::parseFrame got #");
    s++;                        /* skip the hash mark */
    // frame number
    frameNo = atoi(s);
    while (isdigit(*s))
        s++;

    //TRACE(QString("Got frame ").append(QString::number(frameNo)));
    // space
    while (isspace(*s))
        s++;
    parseFrameInfo(s, func, file, lineNo, address);
    // TRACE("Will next look at ");
    // TRACE(s);
    return true;
}

void
XsldbgDriver::parseBackTrace(const char *output,
                             std::list < StackFrame > &stack)
{
    QString func, file;
    int lineNo, frameNo;
    DbgAddr address;

    while (::parseFrame(output, frameNo, func, file, lineNo, address)) {
        stack.push_back(StackFrame());
        StackFrame* frm = &stack.back();

        frm->frameNo = frameNo;
        frm->fileName = file;
        frm->lineNo = lineNo;
        frm->address = address;
        frm->var = new ExprValue(func, VarTree::NKplain);
    }
}

bool
XsldbgDriver::parseFrameChange(const char *output, int &frameNo,
                               QString & file, int &lineNo,
                               DbgAddr & address)
{
    QString func;

    return::parseFrame(output, frameNo, func, file, lineNo, address);
}


bool
XsldbgDriver::parseBreakList(const char *output,
                             std::list < Breakpoint > &brks)
{
   TRACE("parseBreakList");
    /* skip the first blank line */
   const char *p;
   
    // split up a line
    Breakpoint bp;
    char *dummy;
    p =  strchr(output, '\n');/* skip the first blank line*/

    while ((p != 0) && (*p != '\0')) {
	if (*p == '\n')
	    p++;
        QString templateName;
        //qDebug("Looking at :%s", p);
        if (strncmp(p, " Breakpoint", 11) != 0)
            break;
        p = p + 11;
        if (*p == '\0')
            break;

        //TRACE(p);
        // get Num
        bp.id = strtol(p, &dummy, 10);     /* don't care about overflows */

        p = dummy;
        if ((p == 0) || (p[1] == '\0'))
            break;
        p++;

        //TRACE(p);    
        // Get breakpoint state ie enabled/disabled
        if (strncmp(p, "enabled", 7) == 0) {
            bp.enabled = true;
            p = p + 7;
        } else {
            if (strncmp(p, "disabled", 8) == 0) {
                p = p + 8;
                bp.enabled = false;
            } else{
	      TRACE("Parse error in breakpoint list");
	      TRACE(p);
	      return false;
	    }
        }

	//TRACE("Looking for template");
	//TRACE(p);
	if (strncmp(p, " for template: \"", 16) == 0){
	  p = p + 16;
	  //TRACE("Looking for template name near");
	  //TRACE(p);
	  /* get the template name */
	  while (p && (*p != '\0') && (*p != '\"')){
	    templateName.append(*p);
	    p++;
	  }
	  if (*p == '\"'){
	    p++;
	  }else{
	    TRACE("Error missed \" near");
	    TRACE(p);
	  }
	}

	//TRACE("Looking for mode near");
	//TRACE(p);
        if (strncmp(p, " mode: \"", 8) == 0){ 
	  p = p + 8;
	  while (p && *p != '\"')
	    p++;
	  if (p)
	    p++;
	}

	if (strncmp(p, " in file ", 9) != 0){
	  TRACE("Parse error in breakpoint list");
	  TRACE(p);
	  return false;
	}
	
	
        /* skip ' in file ' */
        p = p + 9;
	//	TRACE(p);

	if (*p == '\"')
	    p++;
        /* grab file name */
        QString file;
        while ((*p != '\"') && !isspace(*p)) {
            file.append(*p);
            p++;
        }
	if (*p == '\"')
	    p++;
        if (*p == '\0')
            break;

        /* skip ' : line ' */
        p = p + 8;
        while (isspace(*p)) {
            p++;
        }
        //TRACE(p);    
        QString lineNo;
        while (isdigit(*p)) {
            lineNo.append(*p);
            p++;
        }

        // bp.lineNo is zero-based
        bp.lineNo = lineNo.toInt() - 1;
        bp.location = QString("in %1 at %2:%3").arg(templateName, file, lineNo);
        bp.fileName = file;
        brks.push_back(bp);

        if (p != 0) {
            p = strchr(p, '\n');
	    if (p)
		p++;
        }
    }
    return true;
}

std::list<ThreadInfo>
XsldbgDriver::parseThreadList(const char */*output*/)
{
    return std::list<ThreadInfo>();
}

bool
XsldbgDriver::parseBreakpoint(const char *output, int &id,
                              QString &file, int &lineNo, QString &address)
{
    // check for errors
    if ( strncmp(output, "Error:", 6) == 0) {
	return false;
    }

    char *dummy;
    if (strncmp(output, "Breakpoint ", 11) != 0)
	return false;

    output += 11;
    if (!isdigit(*output))
	return false;

    // get Num
    id = strtol(output, &dummy, 10);     /* don't care about overflows */
    if (output == dummy)
	return false;

    // the file name + lineNo will be filled in later from the breakpoint list
    file = address = QString();
    lineNo = 0;
    return true;
}

void
XsldbgDriver::parseLocals(const char *output, std::list < ExprValue* > &newVars)
{

    /* keep going until error or xsldbg prompt is found */
    while (*output != '\0') {
        ExprValue *variable = parseVar(output);

        if (variable == 0) {
            break;
        }
        // do not add duplicates
        for (std::list<ExprValue*>::iterator o = newVars.begin(); o != newVars.end(); ++o) {
            if ((*o)->m_name == variable->m_name) {
                delete variable;

                goto skipDuplicate;
            }
        }
        newVars.push_back(variable);
      skipDuplicate:;
    }
}


ExprValue *
XsldbgDriver::parsePrintExpr(const char *output, bool wantErrorValue)
{
    ExprValue* var = 0;
    // check for error conditions
    if (!parseErrorMessage(output, var, wantErrorValue)) {
        // parse the variable
        var = parseVar(output);
    }
    return var;
}

bool
XsldbgDriver::parseChangeWD(const char *output, QString & message)
{
    bool isGood = false;

    if (strncmp(output, "Change to directory", 20) == 0) {
        output = output + 20;   /* skip 'Change to directory' */
        message = QString(output).simplifyWhiteSpace();
        if (message.isEmpty()) {
            message = i18n("New working directory: ") + m_programWD;
            isGood = true;
        }
    }
    return isGood;
}

bool
XsldbgDriver::parseChangeExecutable(const char *output, QString & message)
{
    message = output;
    TRACE(QString("XsldbgDriver::parseChangeExecutable :") + output);
    m_haveCoreFile = false;

    /*
     * The command is successful if there is no output or the single
     * message (no debugging symbols found)...
     */
    QRegExp exp(".*Load of source deferred. Use the run command.*");
    int len, index = exp.match(output, 0, &len);

    if (index != -1) {
        TRACE("Parsed stylesheet executable");
        message = "";
    }
    return (output[0] == '\0') || (index != -1);
}

bool
XsldbgDriver::parseCoreFile(const char *output)
{
    TRACE("XsldbgDriver::parseCoreFile");
    TRACE(output);
    QRegExp exp(".*Load of data file deferred. Use the run command.*");
    int len, index = exp.match(output, 0, &len);

    if (index != -1) {
        m_haveCoreFile = true;
        TRACE("Parsed data file name");
    }

    return m_haveCoreFile;
}

uint
XsldbgDriver::parseProgramStopped(const char *output, QString & message)
{
    /* Not sure about this function leave it here for the moment */
    /*
     * return DebuggerDriver::SFrefreshBreak & DebuggerDriver::SFprogramActive;
     */

    // go through the output, line by line, checking what we have
    const char *start = output - 1;
    uint flags = SFprogramActive;

    message = QString();
    do {
        start++;                /* skip '\n' */

        if (strncmp(start, "Finished stylesheet\n\032\032\n", 21) == 0){ 
       //     flags &= ~SFprogramActive;
	    break;
	}

        // next line, please
        start = strchr(start, '\n');
    } while (start != 0);

    return flags;
}

QStringList
XsldbgDriver::parseSharedLibs(const char */*output*/)
{
    return QStringList();
}

bool
XsldbgDriver::parseFindType(const char */*output*/, QString & /*type*/)
{
    return true;
}

std::list<RegisterInfo> 
XsldbgDriver::parseRegisters(const char */*output*/)
{
    return std::list<RegisterInfo>();
}

bool
XsldbgDriver::parseInfoLine(const char */*output*/, QString & /*addrFrom*/,
                            QString & /*addrTo*/)
{
    return false;
}

std::list<DisassembledCode>
XsldbgDriver::parseDisassemble(const char */*output*/)
{
    return std::list<DisassembledCode>();
}

QString
XsldbgDriver::parseMemoryDump(const char */*output*/,
                              std::list < MemoryDump > &/*memdump*/)
{
    return i18n("No memory dump available");
}

QString
XsldbgDriver::parseSetVariable(const char */*output*/)
{
    QString msg;
    return msg;
}


#include "xsldbgdriver.moc"
