// $Id$

// Copyright by Johannes Sixt, Keith Isdale
// This file is under GPL, the GNU General Public Licence

#include "xsldbgdriver.h"
#include "exprwnd.h"
#include <qstringlist.h>
#include <klocale.h>            /* i18n */
#include <ctype.h>
#include <stdlib.h>             /* strtol, atoi */
#include <string.h>             /* strcpy */

#include "assert.h"
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include "mydebug.h"


static VarTree *parseVar(const char *&s);
static bool parseName(const char *&s, QString & name,
                      VarTree::NameKind & kind);
static bool parseValue(const char *&s, VarTree * variable);

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
    {DCsetargs, "setargs %s\n", XsldbgCmdInfo::argString},
    {DCsetenv, "addparam %s %s\n", XsldbgCmdInfo::argString2},
    {DCunsetenv, "unset env %s\n", XsldbgCmdInfo::argString},
    {DCsetoption, "setoption %s %d\n", XsldbgCmdInfo::argStringNum},
    {DCcd, "chdir %s\n", XsldbgCmdInfo::argString},
    {DCbt, "where\n", XsldbgCmdInfo::argNone},
    {DCrun, "run\nsource\n", XsldbgCmdInfo::argNone}, /* Ensure that at the start
							 of executing XSLT we show the XSLT file */
    {DCcont, "continue\n", XsldbgCmdInfo::argNone},
    {DCstep, "step\n", XsldbgCmdInfo::argNone},
    {DCstepi, "print 'DCstepi'\n", XsldbgCmdInfo::argNone},
    {DCnext, "next\n", XsldbgCmdInfo::argNone},
    {DCnexti, "print 'nexti'\n", XsldbgCmdInfo::argNone},
    {DCfinish, "quit\n", XsldbgCmdInfo::argNone},
    {DCuntil, "continue %s:%d\n", XsldbgCmdInfo::argStringNum},
    {DCkill, "quit\n", XsldbgCmdInfo::argNone},
    {DCbreaktext, "break %s\n", XsldbgCmdInfo::argString},
    {DCbreakline, "break -l %s %d\n", XsldbgCmdInfo::argStringNum},
    {DCtbreakline, "print `tbreak %s:%d`\n", XsldbgCmdInfo::argStringNum },
    {DCbreakaddr, "print `break *%s`\n", XsldbgCmdInfo::argString },
    {DCtbreakaddr, "print `tbreak *%s`\n", XsldbgCmdInfo::argString },
    {DCwatchpoint, "print 'watch %s'\n", XsldbgCmdInfo::argString},
    {DCdelete, "delete %d\n", XsldbgCmdInfo::argNum},
    {DCenable, "enable %d\n", XsldbgCmdInfo::argNum},
    {DCdisable, "disable %d\n", XsldbgCmdInfo::argNum},
    {DCprint, "print %s\n", XsldbgCmdInfo::argString},
    {DCprintStruct, "print 'print %s'\n", XsldbgCmdInfo::argString},
    {DCprintQStringStruct, "print 'print %s'\n", XsldbgCmdInfo::argString},
    {DCframe, "frame %d\n", XsldbgCmdInfo::argNum},
    {DCfindType, "print 'whatis %s'\n", XsldbgCmdInfo::argString},
    {DCinfosharedlib, "stylesheets\n", XsldbgCmdInfo::argNone},
    {DCthread, "print 'thread %d'\n", XsldbgCmdInfo::argNum},
    {DCinfothreads, "print 'info threads'\n", XsldbgCmdInfo::argNone},
    {DCinfobreak, "show\n", XsldbgCmdInfo::argNone},
    {DCcondition, "print 'condition %d %s'\n", XsldbgCmdInfo::argNumString},
    {DCignore, "print 'ignore %d %d'\n", XsldbgCmdInfo::argNum2},
};

#define NUM_CMDS (int(sizeof(cmds)/sizeof(cmds[0])))
#define MAX_FMTLEN 200

XsldbgDriver::XsldbgDriver():
DebuggerDriver(), m_gdbMajor(2), m_gdbMinor(0)
{
    m_promptRE.setPattern("\\(xsldbg\\) .*> $");
    m_promptMinLen = 11;
    m_promptLastChar = ' ';
  
    m_markerRE.setPattern("^Breakpoint at file ");

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
    return "xsldbg --shell --gdb";
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

    static const char xsldbgInitialize[] = "pwd\n";     /* don't need to do anything else */

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
            parseMarker();
	}
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
    static QRegExp MarkerRE(" : line [0-9]+");

    int lineNoStart = MarkerRE.match(startMarker, 0, &len);

    if (lineNoStart >= 0) {
        int lineNo = atoi(startMarker + lineNoStart + 8);

        DbgAddr address;

        // now show the window
        startMarker[lineNoStart] = '\0';        /* split off file name */
        TRACE("Got file and line number");
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
    }

    SIZED_QString(cmdString, MAX_FMTLEN + strArg.length());
    cmdString.sprintf(cmds[cmd].fmt, strArg.latin1());
    return cmdString;
}

QString
XsldbgDriver::makeCmdString(DbgCommand cmd, int intArg)
{
    assert(cmd >= 0 && cmd < NUM_CMDS);
    assert(cmds[cmd].argsNeeded == XsldbgCmdInfo::argNum);

    SIZED_QString(cmdString, MAX_FMTLEN + 30);

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

    SIZED_QString(cmdString, MAX_FMTLEN + 30 + strArg.length());

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

    SIZED_QString(cmdString,
                  MAX_FMTLEN + strArg1.length() + strArg2.length());
    cmdString.sprintf(cmds[cmd].fmt, strArg1.latin1(), strArg2.latin1());
    return cmdString;
}

QString
XsldbgDriver::makeCmdString(DbgCommand cmd, int intArg1, int intArg2)
{
    assert(cmd >= 0 && cmd < NUM_CMDS);
    assert(cmds[cmd].argsNeeded == XsldbgCmdInfo::argNum2);

    SIZED_QString(cmdString, MAX_FMTLEN + 60);
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
#define   ERROR_WORD_COUNT 3
    static const char *errorWords[ERROR_WORD_COUNT] = {
      "Error:", 
      "Unknown command",  
      "Warning:"
    };
    static int errorWordLength[ERROR_WORD_COUNT] = {
      5,  /* Error */
      15, /* Unknown command*/ 
      7  /* Warning */
    };
    
    for (wordIndex = 0; wordIndex < ERROR_WORD_COUNT; wordIndex++){
      if (strncmp(output, 
		  errorWords[wordIndex], 
		  errorWordLength[wordIndex]) == 0){
	result = true;
        TRACE(QString("Error/Warning from xsldbg ") + output);
	break;
      }
    }

    return result;
}

/**
 * Returns true if the output is an error message. If wantErrorValue is
 * true, a new VarTree object is created and filled with the error message.
 */
static bool
parseErrorMessage(const char *output,
                  VarTree * &variable, bool wantErrorValue)
{
    if (isErrorExpr(output)) {
        if (wantErrorValue) {
            // put the error message as value in the variable
            variable = new VarTree(QString(), VarTree::NKplain);
            const char *endMsg = strchr(output, '\n');

            if (endMsg == 0)
                endMsg = output + strlen(output);
            variable->m_value = FROM_LATIN1(output, endMsg - output);
        } else {
            variable = 0;
        }
        return true;
    }
    return false;
}


VarTree *
XsldbgDriver::parseQCharArray(const char */*output*/, bool /*wantErrorValue*/,
                              bool /*qt3like*/)
{
    VarTree *variable = 0;

    TRACE("XsldbgDriver::parseQCharArray not implmented");
    return variable;
}

static VarTree *
parseVar(const char *&s)
{
    const char *p = s;
    bool foundLocalVar = false;
    VarTree *variable = 0L;
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
            char nameBuffer[100];

            p = p + 2;          /* skip the "= " */
            strncpy(nameBuffer, p, nextLine - p);
            p = nextLine + 1;
            variable = new VarTree(nameBuffer, kind);
            if (variable != 0L) {
                variable->setDeleteChildren(true);
                parseValue(p, variable);
            }
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
      variable = new VarTree(name, kind);
      if (variable != 0L) {
        variable->setDeleteChildren(true);
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
      variable = new VarTree(name, kind);
      if (variable != 0L) {
        variable->setDeleteChildren(true);
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


    name = FROM_LATIN1(s, len);
    /* XSL variables will have a $ prefix to be evaluated 
     * properly */
    //TRACE(QString("parseName got name" ) +  name);

    // return the new position
    s = p;
    return true;
}

static bool
parseValue(const char *&s, VarTree * variable)
{
    const char *start = s, *end = s;
    VarTree * childValue; 
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
        "runtime error",
	 "xmlXPathEval:",
	0		     				     
    };
    static char valueBuffer[255];
    int markerIndex = 0, foundEnd = 0;
    size_t copySize;

    if (variable == 0L)
        return false;           /* should never happen but .. */

    variable->m_value = "";
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
        if (end) {
            end++;

            copySize = end - start - 1;
            if (copySize > sizeof(valueBuffer))
                copySize = sizeof(valueBuffer);

            strncpy(valueBuffer, start, copySize);
            valueBuffer[copySize] = '\0';
	    TRACE("Got value :");
	    TRACE(valueBuffer);
	    childValue = new VarTree(valueBuffer, VarTree::NKplain);
            variable->appendChild(childValue);

            start = end;
        } else {
	    childValue = new VarTree(start, VarTree::NKplain);
            variable->appendChild(childValue);
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

    lineNo = -1;

    /* skip 'template :\" */
    p = p + 11;
    //TRACE("parseFrameInfo");
    //    TRACE(p);
    func = "";
    while ((*p != '\"') && (*p != '\0')) {
        func.append(*p);
        p++;
    }
    ASSERT(p <= endPos);
    if (p >= endPos) {
        /* panic */
        return;
    }

    /* skip  mode :".*" */
    while (p && *p != '"')
      p++;
    if (p)
      p++;
    while (p && *p != '"')
      p++;   
    if (p)
      p++;
    while (p && *p != '"')
      p++; 
    /* skip '" in file ' */
    p = p + 10;
    //   TRACE(p);
    file = "";
    while (!isspace(*p) && (*p != '\0')) {
        file.append(*p);
        p++;
    }
    ASSERT(p <= endPos);
    if (p >= endPos) {
        /* panic */
        return;
    }
    //    TRACE(p);
    /* skip  ' : line '" */
    p = p + 8;
    //    TRACE(p);
    ASSERT(p <= endPos);
    if (p >= endPos) {
        /* panic */
        return;
    }
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

    //TRACE(QString("Got frame : template :\"") + func + "\"file:" +
    //      file + " line : " + lineNoString);

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
                             QList < StackFrame > &stack)
{
    QString func, file;
    int lineNo, frameNo;
    DbgAddr address;

    while (::parseFrame(output, frameNo, func, file, lineNo, address)) {
        StackFrame *frm = new StackFrame;

        frm->frameNo = frameNo;
        frm->fileName = file;
        frm->lineNo = lineNo;
        frm->address = address;
        frm->var = new VarTree(func, VarTree::NKplain);
        stack.append(frm);
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
                             QList < Breakpoint > &brks)
{
   TRACE("parseBreakList");
    /* skip the first blank line */
   const char *p;
   
    // split up a line
    QString location, file, lineNo;
    QString address;
    QString templateName;	 
    int hits = 0;
    int enabled = 0;
    uint ignoreCount = 0;
    QString condition;
    char *dummy;
    p =  strchr(output, '\n');/* skip the first blank line*/

    while ((p != 0) && (*p != '\0')) {
        templateName = QString();
        p++;
        Breakpoint::Type bpType = Breakpoint::breakpoint;
        //qDebug("Looking at :%s", p);
        if (strncmp(p, " Breakpoint", 11) != 0)
            break;
        p = p + 11;
        if (*p == '\0')
            break;

        //TRACE(p);
        // get Num
        long bpNum = strtol(p, &dummy, 10);     /* don't care about overflows */

        p = dummy;
        if ((p == 0) || (p[1] == '\0'))
            break;
        p++;

        //TRACE(p);    
        // Get breakpoint state ie enabled/disabled
        if (strncmp(p, "enabled", 7) == 0) {
            enabled = true;
            p = p + 7;
        } else {
            if (strncmp(p, "disabled", 8) == 0) {
                p = p + 8;
                enabled = false;
            } else{
	      TRACE("Parse error in breakpoint list");
	      TRACE(p);
	      return false;
	    }
        }

	//TRACE("Looking for template");
	//TRACE(p);

	/* check for ' for template :"'*/
	if (strncmp(p, " for template :\"", 16) == 0){ 
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
	if (strncmp(p, " mode :\"", 8) == 0) {
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

        /* grab file name */
        while (!isspace(*p)) {
            file.append(*p);
            p++;
        }
        if (*p == '\0')
            break;

        /* skip ' : line ' */
        p = p + 8;
        //TRACE(p);    
        while (isdigit(*p)) {
            lineNo.append(*p);
            p++;
        }


        //TRACE("Got breakpoint");

        Breakpoint *bp = new Breakpoint;

        if (bp != 0) {
            // take 1 of line number
            lineNo.setNum(lineNo.toInt() - 1);
            bp->id = bpNum;
            bp->type = bpType;
            bp->temporary = false;
            bp->enabled = enabled;
            location.append("in ").append(templateName).append(" at ");
	    location.append(file).append(":").append(lineNo);
            bp->location = location;
            bp->fileName = file;
            bp->lineNo = lineNo.toInt();
            bp->address = address;
            bp->hitCount = hits;
            bp->ignoreCount = ignoreCount;
            bp->condition = condition;
            brks.append(bp);
            location = "";
            lineNo = "";
            file = "";
        } else
            TRACE("Outof memory, breakpoint not created");

        if (p != 0) {
            p = strchr(p, '\n');
        }
    }
    return true;
}

bool
XsldbgDriver::parseThreadList(const char */*output*/,
                              QList < ThreadInfo > &/*threads*/)
{
    return true;
}

bool
XsldbgDriver::parseBreakpoint(const char */*output*/, int &/*id*/,
                              QString & /*file*/, int &/*lineNo*/)
{
    TRACE("parseBreakpoint");
    return true;
}

void
XsldbgDriver::parseLocals(const char *output, QList < VarTree > &newVars)
{

    /* keep going until error or xsldbg prompt is found */
    while (*output != '\0') {
        VarTree *variable = parseVar(output);

        if (variable == 0) {
            break;
        }
        // do not add duplicates
        for (VarTree * o = newVars.first(); o != 0; o = newVars.next()) {
            if (o->getText() == variable->getText()) {
                delete variable;

                goto skipDuplicate;
            }
        }
        newVars.append(variable);
      skipDuplicate:;
    }
}


bool
XsldbgDriver::parsePrintExpr(const char *output, bool wantErrorValue,
                             VarTree * &var)
{
    // check for error conditions
    if (parseErrorMessage(output, var, wantErrorValue)) {
        return false;
    } else {
        // parse the variable
        var = parseVar(output);
        return true;
    }
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
    QRegExp exp(".*Load of source deferred use run command.*");
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
    QRegExp exp(".*Load of xml data deferred use run command.*");
    int len, index = exp.match(output, 0, &len);

    if (index != -1) {
        m_haveCoreFile = true;
        TRACE("Parsed xml data file");
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

        if (strncmp(start, "Program ", 8) == 0 ||
            strncmp(start, "ptrace: ", 8) == 0) {
            /*
             * When we receive a signal, the program remains active.
             *
             * Special: If we "stopped" in a corefile, the string "Program
             * terminated with signal"... is displayed. (Normally, we see
             * "Program received signal"... when a signal happens.)
             */
            if (strncmp(start, "Program exited", 14) == 0 ||
                (strncmp(start, "Program terminated", 18) == 0
                 && !m_haveCoreFile)
                || strncmp(start, "ptrace: ", 8) == 0) {
                flags &= ~SFprogramActive;
            }
            // set message
            const char *endOfMessage = strchr(start, '\n');

           if (endOfMessage == 0)
                endOfMessage = start + strlen(start);
            message = FROM_LATIN1(start, endOfMessage - start);
        } else if (strncmp(start, "Breakpoint ", 11) == 0) {
            /*
             * We stopped at a (permanent) breakpoint (gdb doesn't tell us
             * that it stopped at a temporary breakpoint).
             */
            flags |= SFrefreshBreak;
        } else if (strstr(start, "re-reading symbols.") != 0) {
            flags |= SFrefreshSource;
        }
        // next line, please
        start = strchr(start, '\n');
    } while (start != 0);

    /*
     * Gdb only notices when new threads have appeared, but not when a
     * thread finishes. So we always have to assume that the list of
     * threads has changed.
     */
    flags |= SFrefreshThreads;

    return flags;


}

void
XsldbgDriver::parseSharedLibs(const char */*output*/, QStrList & /*shlibs*/)
{
    /* empty */
}

bool
XsldbgDriver::parseFindType(const char */*output*/, QString & /*type*/)
{
    return true;
}

void
XsldbgDriver::parseRegisters(const char */*output*/,
                             QList < RegisterInfo > &/*regs*/)
{

}

bool
XsldbgDriver::parseInfoLine(const char */*output*/, QString & /*addrFrom*/,
                            QString & /*addrTo*/)
{
    return true;
}

void
XsldbgDriver::parseDisassemble(const char */*output*/,
                               QList < DisassembledCode > &/*code*/)
{
    /* empty */
}

QString
XsldbgDriver::parseMemoryDump(const char */*output*/,
                              QList < MemoryDump > &/*memdump*/)
{
    return i18n("No memory dump available");
}


#include "xsldbgdriver.moc"
