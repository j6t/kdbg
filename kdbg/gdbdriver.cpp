/*
 * Copyright Johannes Sixt
 * This file is licensed under the GNU General Public License Version 2.
 * See the file COPYING in the toplevel directory of the source directory.
 */

#include "gdbdriver.h"
#include "exprwnd.h"
#include <qregexp.h>
#include <qstringlist.h>
#include <klocale.h>			/* i18n */
#include <ctype.h>
#include <stdlib.h>			/* strtol, atoi */
#include <string.h>			/* strcpy */

#include "assert.h"
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include "mydebug.h"

static void skipString(const char*& p);
static void skipNested(const char*& s, char opening, char closing);
static ExprValue* parseVar(const char*& s);
static bool parseName(const char*& s, QString& name, VarTree::NameKind& kind);
static bool parseValue(const char*& s, ExprValue* variable);
static bool parseNested(const char*& s, ExprValue* variable);
static bool parseVarSeq(const char*& s, ExprValue* variable);
static bool parseValueSeq(const char*& s, ExprValue* variable);

#define PROMPT "(kdbg)"
#define PROMPT_LEN 6
#define PROMPT_LAST_CHAR ')'		/* needed when searching for prompt string */


// TODO: make this cmd info stuff non-static to allow multiple
// simultaneous gdbs to run!

struct GdbCmdInfo {
    DbgCommand cmd;
    const char* fmt;			/* format string */
    enum Args {
	argNone, argString, argNum,
	argStringNum, argNumString,
	argString2, argNum2
    } argsNeeded;
};

#if 0
// This is  how the QString data print statement generally looks like.
// It is set by KDebugger via setPrintQStringDataCmd().

static const char printQStringStructFmt[] =
		// if the string data is junk, fail early
		"print ($qstrunicode=($qstrdata=(%s))->unicode)?"
		// print an array of shorts
		"(*(unsigned short*)$qstrunicode)@"
		// limit the length
		"(($qstrlen=(unsigned int)($qstrdata->len))>100?100:$qstrlen)"
		// if unicode data is 0, report a special value
		":1==0\n";
#endif
static const char printQStringStructFmt[] = "print (0?\"%s\":$kdbgundef)\n";

/*
 * The following array of commands must be sorted by the DC* values,
 * because they are used as indices.
 */
static GdbCmdInfo cmds[] = {
    { DCinitialize, "", GdbCmdInfo::argNone },
    { DCtty, "tty %s\n", GdbCmdInfo::argString },
    { DCexecutable, "file \"%s\"\n", GdbCmdInfo::argString },
    { DCtargetremote, "target remote %s\n", GdbCmdInfo::argString },
    { DCcorefile, "core-file %s\n", GdbCmdInfo::argString },
    { DCattach, "attach %s\n", GdbCmdInfo::argString },
    { DCinfolinemain, "kdbg_infolinemain\n", GdbCmdInfo::argNone },
    { DCinfolocals, "kdbg__alllocals\n", GdbCmdInfo::argNone },
    { DCinforegisters, "info all-registers\n", GdbCmdInfo::argNone},
    { DCexamine, "x %s %s\n", GdbCmdInfo::argString2 },
    { DCinfoline, "info line %s:%d\n", GdbCmdInfo::argStringNum },
    { DCdisassemble, "disassemble %s %s\n", GdbCmdInfo::argString2 },
    { DCsetargs, "set args %s\n", GdbCmdInfo::argString },
    { DCsetenv, "set env %s %s\n", GdbCmdInfo::argString2 },
    { DCunsetenv, "unset env %s\n", GdbCmdInfo::argString },
    { DCsetoption, "setoption %s %d\n", GdbCmdInfo::argStringNum},
    { DCcd, "cd %s\n", GdbCmdInfo::argString },
    { DCbt, "bt\n", GdbCmdInfo::argNone },
    { DCrun, "run\n", GdbCmdInfo::argNone },
    { DCcont, "cont\n", GdbCmdInfo::argNone },
    { DCstep, "step\n", GdbCmdInfo::argNone },
    { DCstepi, "stepi\n", GdbCmdInfo::argNone },
    { DCnext, "next\n", GdbCmdInfo::argNone },
    { DCnexti, "nexti\n", GdbCmdInfo::argNone },
    { DCfinish, "finish\n", GdbCmdInfo::argNone },
    { DCuntil, "until %s:%d\n", GdbCmdInfo::argStringNum },
    { DCkill, "kill\n", GdbCmdInfo::argNone },
    { DCbreaktext, "break %s\n", GdbCmdInfo::argString },
    { DCbreakline, "break %s:%d\n", GdbCmdInfo::argStringNum },
    { DCtbreakline, "tbreak %s:%d\n", GdbCmdInfo::argStringNum },
    { DCbreakaddr, "break *%s\n", GdbCmdInfo::argString },
    { DCtbreakaddr, "tbreak *%s\n", GdbCmdInfo::argString },
    { DCwatchpoint, "watch %s\n", GdbCmdInfo::argString },
    { DCdelete, "delete %d\n", GdbCmdInfo::argNum },
    { DCenable, "enable %d\n", GdbCmdInfo::argNum },
    { DCdisable, "disable %d\n", GdbCmdInfo::argNum },
    { DCprint, "print %s\n", GdbCmdInfo::argString },
    { DCprintDeref, "print *(%s)\n", GdbCmdInfo::argString },
    { DCprintStruct, "print %s\n", GdbCmdInfo::argString },
    { DCprintQStringStruct, printQStringStructFmt, GdbCmdInfo::argString},
    { DCframe, "frame %d\n", GdbCmdInfo::argNum },
    { DCfindType, "whatis %s\n", GdbCmdInfo::argString },
    { DCinfosharedlib, "info sharedlibrary\n", GdbCmdInfo::argNone },
    { DCthread, "thread %d\n", GdbCmdInfo::argNum },
    { DCinfothreads, "info threads\n", GdbCmdInfo::argNone },
    { DCinfobreak, "info breakpoints\n", GdbCmdInfo::argNone },
    { DCcondition, "condition %d %s\n", GdbCmdInfo::argNumString},
    { DCsetpc, "set variable $pc=%s\n", GdbCmdInfo::argString },
    { DCignore, "ignore %d %d\n", GdbCmdInfo::argNum2},
    { DCprintWChar, "print ($s=%s)?*$s@wcslen($s):0x0\n", GdbCmdInfo::argString },
    { DCsetvariable, "set variable %s=%s\n", GdbCmdInfo::argString2 },
};

#define NUM_CMDS (int(sizeof(cmds)/sizeof(cmds[0])))
#define MAX_FMTLEN 200

GdbDriver::GdbDriver() :
	DebuggerDriver(),
	m_gdbMajor(4), m_gdbMinor(16)
{
    strcpy(m_prompt, PROMPT);
    m_promptMinLen = PROMPT_LEN;
    m_promptLastChar = PROMPT_LAST_CHAR;

#ifndef NDEBUG
    // check command info array
    char* perc;
    for (int i = 0; i < NUM_CMDS; i++) {
	// must be indexable by DbgCommand values, i.e. sorted by DbgCommand values
	assert(i == cmds[i].cmd);
	// a format string must be associated
	assert(cmds[i].fmt != 0);
	assert(strlen(cmds[i].fmt) <= MAX_FMTLEN);
	// format string must match arg specification
	switch (cmds[i].argsNeeded) {
	case GdbCmdInfo::argNone:
	    assert(strchr(cmds[i].fmt, '%') == 0);
	    break;
	case GdbCmdInfo::argString:
	    perc = strchr(cmds[i].fmt, '%');
	    assert(perc != 0 && perc[1] == 's');
	    assert(strchr(perc+2, '%') == 0);
	    break;
	case GdbCmdInfo::argNum:
	    perc = strchr(cmds[i].fmt, '%');
	    assert(perc != 0 && perc[1] == 'd');
	    assert(strchr(perc+2, '%') == 0);
	    break;
	case GdbCmdInfo::argStringNum:
	    perc = strchr(cmds[i].fmt, '%');
	    assert(perc != 0 && perc[1] == 's');
	    perc = strchr(perc+2, '%');
	    assert(perc != 0 && perc[1] == 'd');
	    assert(strchr(perc+2, '%') == 0);
	    break;
	case GdbCmdInfo::argNumString:
	    perc = strchr(cmds[i].fmt, '%');
	    assert(perc != 0 && perc[1] == 'd');
	    perc = strchr(perc+2, '%');
	    assert(perc != 0 && perc[1] == 's');
	    assert(strchr(perc+2, '%') == 0);
	    break;
	case GdbCmdInfo::argString2:
	    perc = strchr(cmds[i].fmt, '%');
	    assert(perc != 0 && perc[1] == 's');
	    perc = strchr(perc+2, '%');
	    assert(perc != 0 && perc[1] == 's');
	    assert(strchr(perc+2, '%') == 0);
	    break;
	case GdbCmdInfo::argNum2:
	    perc = strchr(cmds[i].fmt, '%');
	    assert(perc != 0 && perc[1] == 'd');
	    perc = strchr(perc+2, '%');
	    assert(perc != 0 && perc[1] == 'd');
	    assert(strchr(perc+2, '%') == 0);
	    break;
	}
    }
    assert(strlen(printQStringStructFmt) <= MAX_FMTLEN);
#endif
}

GdbDriver::~GdbDriver()
{
}


QString GdbDriver::driverName() const
{
    return "GDB";
}

QString GdbDriver::defaultGdb()
{
    return
	"gdb"
	" --fullname"	/* to get standard file names each time the prog stops */
	" --nx";	/* do not execute initialization files */
}

QString GdbDriver::defaultInvocation() const
{
    if (m_defaultCmd.isEmpty()) {
	return defaultGdb();
    } else {
	return m_defaultCmd;
    }
}

QStringList GdbDriver::boolOptionList() const
{
    // no options
    return QStringList();
}

bool GdbDriver::startup(QString cmdStr)
{
    if (!DebuggerDriver::startup(cmdStr))
	return false;

    static const char gdbInitialize[] =
	/*
	 * Work around buggy gdbs that do command line editing even if they
	 * are not on a tty. The readline library echos every command back
	 * in this case, which is confusing for us.
	 */
	"set editing off\n"
	"set confirm off\n"
	"set print static-members off\n"
	"set print asm-demangle on\n"
	/*
	 * Don't assume that program functions invoked from a watch expression
	 * always succeed.
	 */
	"set unwindonsignal on\n"
	/*
	 * Write a short macro that prints all locals: local variables and
	 * function arguments.
	 */
	"define kdbg__alllocals\n"
	"info locals\n"			/* local vars supersede args with same name */
	"info args\n"			/* therefore, arguments must come last */
	"end\n"
	/*
	 * Work around a bug in gdb-6.3: "info line main" crashes gdb.
	 */
	"define kdbg_infolinemain\n"
	"list\n"
	"info line\n"
	"end\n"
	// change prompt string and synchronize with gdb
	"set prompt " PROMPT "\n"
	;

    executeCmdString(DCinitialize, gdbInitialize, false);

    // assume that QString::null is ok
    cmds[DCprintQStringStruct].fmt = printQStringStructFmt;

    return true;
}

void GdbDriver::commandFinished(CmdQueueItem* cmd)
{
    // command string must be committed
    if (!cmd->m_committed) {
	// not commited!
	TRACE("calling " + (__PRETTY_FUNCTION__ + (" with uncommited command:\n\t" +
	      cmd->m_cmdString)));
	return;
    }

    switch (cmd->m_cmd) {
    case DCinitialize:
	// get version number from preamble
	{
	    int len;
	    QRegExp GDBVersion("\\nGDB [0-9]+\\.[0-9]+");
	    int offset = GDBVersion.match(m_output, 0, &len);
	    if (offset >= 0) {
		char* start = m_output + offset + 5;	// skip "\nGDB "
		char* end;
		m_gdbMajor = strtol(start, &end, 10);
		m_gdbMinor = strtol(end + 1, 0, 10);	// skip "."
		if (start == end) {
		    // nothing was parsed
		    m_gdbMajor = 4;
		    m_gdbMinor = 16;
		}
	    } else {
		// assume some default version (what would make sense?)
		m_gdbMajor = 4;
		m_gdbMinor = 16;
	    }
	    // use a feasible core-file command
	    if (m_gdbMajor > 4 || (m_gdbMajor == 4 && m_gdbMinor >= 16)) {
#ifdef __FreeBSD__
		cmds[DCcorefile].fmt = "target FreeBSD-core %s\n";
#else
		cmds[DCcorefile].fmt = "target core %s\n";
#endif
	    } else {
		cmds[DCcorefile].fmt = "core-file %s\n";
	    }
	}
	break;
    default:;
    }

    /* ok, the command is ready */
    emit commandReceived(cmd, m_output);

    switch (cmd->m_cmd) {
    case DCcorefile:
    case DCinfolinemain:
    case DCframe:
    case DCattach:
    case DCrun:
    case DCcont:
    case DCstep:
    case DCstepi:
    case DCnext:
    case DCnexti:
    case DCfinish:
    case DCuntil:
	parseMarker();
    default:;
    }
}

/*
 * The --fullname option makes gdb send a special normalized sequence print
 * each time the program stops and at some other points. The sequence has
 * the form "\032\032filename:lineno:charoffset:(beg|middle):address".
 */
void GdbDriver::parseMarker()
{
    char* startMarker = strstr(m_output, "\032\032");
    if (startMarker == 0)
	return;

    // extract the marker
    startMarker += 2;
    TRACE(QString("found marker: ") + startMarker);
    char* endMarker = strchr(startMarker, '\n');
    if (endMarker == 0)
	return;

    *endMarker = '\0';

    // extract filename and line number
    static QRegExp MarkerRE(":[0-9]+:[0-9]+:[begmidl]+:0x");

    int len;
    int lineNoStart = MarkerRE.match(startMarker, 0, &len);
    if (lineNoStart >= 0) {
	int lineNo = atoi(startMarker + lineNoStart+1);

	// get address
	const char* addrStart = startMarker + lineNoStart + len - 2;
	DbgAddr address = QString(addrStart).stripWhiteSpace();

	// now show the window
	startMarker[lineNoStart] = '\0';   /* split off file name */
	emit activateFileLine(startMarker, lineNo-1, address);
    }
}


/*
 * Escapes characters that might lead to problems when they appear on gdb's
 * command line.
 */
static void normalizeStringArg(QString& arg)
{
    /*
     * Remove trailing backslashes. This approach is a little simplistic,
     * but we know that there is at the moment no case where a trailing
     * backslash would make sense.
     */
    while (!arg.isEmpty() && arg[arg.length()-1] == '\\') {
	arg = arg.left(arg.length()-1);
    }
}


QString GdbDriver::makeCmdString(DbgCommand cmd, QString strArg)
{
    assert(cmd >= 0 && cmd < NUM_CMDS);
    assert(cmds[cmd].argsNeeded == GdbCmdInfo::argString);

    normalizeStringArg(strArg);

    if (cmd == DCcd) {
	// need the working directory when parsing the output
	m_programWD = strArg;
    } else if (cmd == DCsetargs && !m_redirect.isEmpty()) {
	/*
	 * Use saved redirection. We prepend it in front of the user's
	 * arguments so that the user can override the redirections.
	*/
	strArg = m_redirect + " " + strArg;
    }

    QString cmdString;
    cmdString.sprintf(cmds[cmd].fmt, strArg.latin1());
    return cmdString;
}

QString GdbDriver::makeCmdString(DbgCommand cmd, int intArg)
{
    assert(cmd >= 0 && cmd < NUM_CMDS);
    assert(cmds[cmd].argsNeeded == GdbCmdInfo::argNum);

    QString cmdString;
    cmdString.sprintf(cmds[cmd].fmt, intArg);
    return cmdString;
}

QString GdbDriver::makeCmdString(DbgCommand cmd, QString strArg, int intArg)
{
    assert(cmd >= 0 && cmd < NUM_CMDS);
    assert(cmds[cmd].argsNeeded == GdbCmdInfo::argStringNum ||
	   cmds[cmd].argsNeeded == GdbCmdInfo::argNumString ||
	   cmd == DCexamine ||
	   cmd == DCtty);

    normalizeStringArg(strArg);

    QString cmdString;

    if (cmd == DCtty)
    {
	/*
	 * intArg specifies which channels should be redirected to
	 * /dev/null. It is a value or'ed together from RDNstdin,
	 * RDNstdout, RDNstderr. We store the value for a later DCsetargs
	 * command.
	 * 
	 * Note: We rely on that after the DCtty a DCsetargs will follow,
	 * which will ultimately apply the redirection.
	 */
	static const char* const runRedir[8] = {
	    "",
	    "</dev/null",
	    ">/dev/null",
	    "</dev/null >/dev/null",
	    "2>/dev/null",
	    "</dev/null 2>/dev/null",
	    ">/dev/null 2>&1",
	    "</dev/null >/dev/null 2>&1"
	};
	if (strArg.isEmpty())
	    intArg = 7;			/* failsafe if no tty */
	m_redirect = runRedir[intArg & 7];

	return makeCmdString(DCtty, strArg);   /* note: no problem if strArg empty */
    }

    if (cmd == DCexamine) {
	// make a format specifier from the intArg
	static const char size[16] = {
	    '\0', 'b', 'h', 'w', 'g'
	};
	static const char format[16] = {
	    '\0', 'x', 'd', 'u', 'o', 't',
	    'a',  'c', 'f', 's', 'i'
	};
	assert(MDTsizemask == 0xf);	/* lowest 4 bits */
	assert(MDTformatmask == 0xf0);	/* next 4 bits */
	int count = 16;			/* number of entities to print */
	char sizeSpec = size[intArg & MDTsizemask];
	char formatSpec = format[(intArg & MDTformatmask) >> 4];
	assert(sizeSpec != '\0');
	assert(formatSpec != '\0');
	// adjust count such that 16 lines are printed
	switch (intArg & MDTformatmask) {
	case MDTstring: case MDTinsn:
	    break;			/* no modification needed */
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

    if (cmds[cmd].argsNeeded == GdbCmdInfo::argStringNum)
    {
	// line numbers are zero-based
	if (cmd == DCuntil || cmd == DCbreakline ||
	    cmd == DCtbreakline || cmd == DCinfoline)
	{
	    intArg++;
	}
	if (cmd == DCinfoline)
	{
	    // must split off file name part
	    int slash = strArg.findRev('/');
	    if (slash >= 0)
		strArg = strArg.right(strArg.length()-slash-1);
	}
	cmdString.sprintf(cmds[cmd].fmt, strArg.latin1(), intArg);
    }
    else
    {
	cmdString.sprintf(cmds[cmd].fmt, intArg, strArg.latin1());
    }
    return cmdString;
}

QString GdbDriver::makeCmdString(DbgCommand cmd, QString strArg1, QString strArg2)
{
    assert(cmd >= 0 && cmd < NUM_CMDS);
    assert(cmds[cmd].argsNeeded == GdbCmdInfo::argString2);

    normalizeStringArg(strArg1);
    normalizeStringArg(strArg2);

    QString cmdString;
    cmdString.sprintf(cmds[cmd].fmt, strArg1.latin1(), strArg2.latin1());
    return cmdString;
}

QString GdbDriver::makeCmdString(DbgCommand cmd, int intArg1, int intArg2)
{
    assert(cmd >= 0 && cmd < NUM_CMDS);
    assert(cmds[cmd].argsNeeded == GdbCmdInfo::argNum2);

    QString cmdString;
    cmdString.sprintf(cmds[cmd].fmt, intArg1, intArg2);
    return cmdString;
}

CmdQueueItem* GdbDriver::executeCmd(DbgCommand cmd, bool clearLow)
{
    assert(cmd >= 0 && cmd < NUM_CMDS);
    assert(cmds[cmd].argsNeeded == GdbCmdInfo::argNone);

    if (cmd == DCrun) {
	m_haveCoreFile = false;
    }

    return executeCmdString(cmd, cmds[cmd].fmt, clearLow);
}

CmdQueueItem* GdbDriver::executeCmd(DbgCommand cmd, QString strArg,
				    bool clearLow)
{
    return executeCmdString(cmd, makeCmdString(cmd, strArg), clearLow);
}

CmdQueueItem* GdbDriver::executeCmd(DbgCommand cmd, int intArg,
				    bool clearLow)
{

    return executeCmdString(cmd, makeCmdString(cmd, intArg), clearLow);
}

CmdQueueItem* GdbDriver::executeCmd(DbgCommand cmd, QString strArg, int intArg,
				    bool clearLow)
{
    return executeCmdString(cmd, makeCmdString(cmd, strArg, intArg), clearLow);
}

CmdQueueItem* GdbDriver::executeCmd(DbgCommand cmd, QString strArg1, QString strArg2,
				    bool clearLow)
{
    return executeCmdString(cmd, makeCmdString(cmd, strArg1, strArg2), clearLow);
}

CmdQueueItem* GdbDriver::executeCmd(DbgCommand cmd, int intArg1, int intArg2,
				    bool clearLow)
{
    return executeCmdString(cmd, makeCmdString(cmd, intArg1, intArg2), clearLow);
}

CmdQueueItem* GdbDriver::queueCmd(DbgCommand cmd, QueueMode mode)
{
    return queueCmdString(cmd, cmds[cmd].fmt, mode);
}

CmdQueueItem* GdbDriver::queueCmd(DbgCommand cmd, QString strArg,
				  QueueMode mode)
{
    return queueCmdString(cmd, makeCmdString(cmd, strArg), mode);
}

CmdQueueItem* GdbDriver::queueCmd(DbgCommand cmd, int intArg,
				  QueueMode mode)
{
    return queueCmdString(cmd, makeCmdString(cmd, intArg), mode);
}

CmdQueueItem* GdbDriver::queueCmd(DbgCommand cmd, QString strArg, int intArg,
				  QueueMode mode)
{
    return queueCmdString(cmd, makeCmdString(cmd, strArg, intArg), mode);
}

CmdQueueItem* GdbDriver::queueCmd(DbgCommand cmd, QString strArg1, QString strArg2,
				  QueueMode mode)
{
    return queueCmdString(cmd, makeCmdString(cmd, strArg1, strArg2), mode);
}

void GdbDriver::terminate()
{
    kill(SIGTERM);
    m_state = DSidle;
}

void GdbDriver::detachAndTerminate()
{
    kill(SIGINT);
    flushCommands();
    executeCmdString(DCinitialize, "detach\nquit\n", true);
}

void GdbDriver::interruptInferior()
{
    kill(SIGINT);
    // remove accidentally queued commands
    flushHiPriQueue();
}

static bool isErrorExpr(const char* output)
{
    return
	strncmp(output, "Cannot access memory at", 23) == 0 ||
	strncmp(output, "Attempt to dereference a generic pointer", 40) == 0 ||
	strncmp(output, "Attempt to take contents of ", 28) == 0 ||
	strncmp(output, "Attempt to use a type name as an expression", 43) == 0 ||
	strncmp(output, "There is no member or method named", 34) == 0 ||
	strncmp(output, "A parse error in expression", 27) == 0 ||
	strncmp(output, "No symbol \"", 11) == 0 ||
	strncmp(output, "Internal error: ", 16) == 0;
}

/**
 * Returns true if the output is an error message. If wantErrorValue is
 * true, a new ExprValue object is created and filled with the error message.
 * If there are warnings, they are skipped and output points past the warnings
 * on return (even if there \e are errors).
 */
static bool parseErrorMessage(const char*& output,
			      ExprValue*& variable, bool wantErrorValue)
{
    // skip warnings
    while (strncmp(output, "warning:", 8) == 0)
    {
	char* end = strchr(output+8, '\n');
	if (end == 0)
	    output += strlen(output);
	else
	    output = end+1;
    }

    if (isErrorExpr(output))
    {
	if (wantErrorValue) {
	    // put the error message as value in the variable
	    variable = new ExprValue(QString(), VarTree::NKplain);
	    const char* endMsg = strchr(output, '\n');
	    if (endMsg == 0)
		endMsg = output + strlen(output);
	    variable->m_value = QString::fromLatin1(output, endMsg-output);
	} else {
	    variable = 0;
	}
	return true;
    }
    return false;
}

#if QT_VERSION >= 300
union Qt2QChar {
    short s;
    struct {
	uchar row;
	uchar cell;
    } qch;
};
#endif

void GdbDriver::setPrintQStringDataCmd(const char* cmd)
{
    // don't accept the command if it is empty
    if (cmd == 0 || *cmd == '\0')
	return;
    assert(strlen(cmd) <= MAX_FMTLEN);
    cmds[DCprintQStringStruct].fmt = cmd;
}

ExprValue* GdbDriver::parseQCharArray(const char* output, bool wantErrorValue, bool qt3like)
{
    ExprValue* variable = 0;

    /*
     * Parse off white space. gdb sometimes prints white space first if the
     * printed array leaded to an error.
     */
    while (isspace(*output))
	output++;

    // special case: empty string (0 repetitions)
    if (strncmp(output, "Invalid number 0 of repetitions", 31) == 0)
    {
	variable = new ExprValue(QString(), VarTree::NKplain);
	variable->m_value = "\"\"";
	return variable;
    }

    // check for error conditions
    if (parseErrorMessage(output, variable, wantErrorValue))
	return variable;

    // parse the array

    // find '='
    const char* p = output;
    p = strchr(p, '=');
    if (p == 0) {
	goto error;
    }
    // skip white space
    do {
	p++;
    } while (isspace(*p));

    if (*p == '{')
    {
	// this is the real data
	p++;				/* skip '{' */

	// parse the array
	QString result;
	QString repeatCount;
	enum { wasNothing, wasChar, wasRepeat } lastThing = wasNothing;
	/*
	 * A matrix for separators between the individual "things"
	 * that are added to the string. The first index is a bool,
	 * the second index is from the enum above.
	 */
	static const char* separator[2][3] = {
	    { "\"", 0,       ", \"" },	/* normal char is added */
	    { "'",  "\", '", ", '" }	/* repeated char is added */
	};

	while (isdigit(*p)) {
	    // parse a number
	    char* end;
	    unsigned short value = (unsigned short) strtoul(p, &end, 0);
	    if (end == p)
		goto error;		/* huh? no valid digits */
	    // skip separator and search for a repeat count
	    p = end;
	    while (isspace(*p) || *p == ',')
		p++;
	    bool repeats = strncmp(p, "<repeats ", 9) == 0;
	    if (repeats) {
		const char* start = p;
		p = strchr(p+9, '>');	/* search end and advance */
		if (p == 0)
		    goto error;
		p++;			/* skip '>' */
		repeatCount = QString::fromLatin1(start, p-start);
		while (isspace(*p) || *p == ',')
		    p++;
	    }
	    // p is now at the next char (or the end)

	    // interpret the value as a QChar
	    // TODO: make cross-architecture compatible
	    QChar ch;
	    if (qt3like) {
		ch = QChar(value);
	    } else {
#if QT_VERSION < 300
		(unsigned short&)ch = value;
#else
		Qt2QChar c;
		c.s = value;
		ch.setRow(c.qch.row);
		ch.setCell(c.qch.cell);
#endif
	    }

	    // escape a few frequently used characters
	    char escapeCode = '\0';
	    switch (ch.latin1()) {
	    case '\n': escapeCode = 'n'; break;
	    case '\r': escapeCode = 'r'; break;
	    case '\t': escapeCode = 't'; break;
	    case '\b': escapeCode = 'b'; break;
	    case '\"': escapeCode = '\"'; break;
	    case '\\': escapeCode = '\\'; break;
	    case '\0': if (value == 0) { escapeCode = '0'; } break;
	    }

	    // add separator
	    result += separator[repeats][lastThing];
	    // add char
	    if (escapeCode != '\0') {
		result += '\\';
		ch = escapeCode;
	    }
	    result += ch;

	    // fixup repeat count and lastThing
	    if (repeats) {
		result += "' ";
		result += repeatCount;
		lastThing = wasRepeat;
	    } else {
		lastThing = wasChar;
	    }
	}
	if (*p != '}')
	    goto error;

	// closing quote
	if (lastThing == wasChar)
	    result += "\"";

	// assign the value
	variable = new ExprValue(QString(), VarTree::NKplain);
	variable->m_value = result;
    }
    else if (strncmp(p, "true", 4) == 0)
    {
	variable = new ExprValue(QString(), VarTree::NKplain);
	variable->m_value = "QString::null";
    }
    else if (strncmp(p, "false", 5) == 0)
    {
	variable = new ExprValue(QString(), VarTree::NKplain);
	variable->m_value = "(null)";
    }
    else
	goto error;
    return variable;

error:
    if (wantErrorValue) {
	variable = new ExprValue(QString(), VarTree::NKplain);
	variable->m_value = "internal parse error";
    }
    return variable;
}

static ExprValue* parseVar(const char*& s)
{
    const char* p = s;
    
    // skip whitespace
    while (isspace(*p))
	p++;

    QString name;
    VarTree::NameKind kind;
    /*
     * Detect anonymouse struct values: The 'name =' part is missing:
     *    s = { a = 1, { b = 2 }}
     * Note that this detection works only inside structs when the anonymous
     * struct is not the first member:
     *    s = {{ a = 1 }, b = 2}
     * This is misparsed (by parseNested()) because it is mistakenly
     * interprets the second opening brace as the first element of an array
     * of structs.
     */
    if (*p == '{')
    {
	name = i18n("<anonymous struct or union>");
	kind = VarTree::NKanonymous;
    }
    else
    {
	if (!parseName(p, name, kind)) {
	    return 0;
	}

	// go for '='
	while (isspace(*p))
	    p++;
	if (*p != '=') {
	    TRACE(QString().sprintf("parse error: = not found after %s", (const char*)name));
	    return 0;
	}
	// skip the '=' and more whitespace
	p++;
	while (isspace(*p))
	    p++;
    }

    ExprValue* variable = new ExprValue(name, kind);
    
    if (!parseValue(p, variable)) {
	delete variable;
	return 0;
    }
    s = p;
    return variable;
}

static void skipNested(const char*& s, char opening, char closing)
{
    const char* p = s;

    // parse a nested type
    int nest = 1;
    p++;
    /*
     * Search for next matching `closing' char, skipping nested pairs of
     * `opening' and `closing'.
     */
    while (*p && nest > 0) {
	if (*p == opening) {
	    nest++;
	} else if (*p == closing) {
	    nest--;
	}
	p++;
    }
    if (nest != 0) {
	TRACE(QString().sprintf("parse error: mismatching %c%c at %-20.20s", opening, closing, s));
    }
    s = p;
}

/**
 * This function skips text that is delimited by nested angle bracktes, '<>'.
 * A complication arises because the delimited text can contain the names of
 * operator<<, operator>>, operator<, and operator>, which have to be treated
 * specially so that they do not count towards the nesting of '<>'.
 * This function assumes that the delimited text does not contain strings.
 */
static void skipNestedAngles(const char*& s)
{
    const char* p = s;

    int nest = 1;
    p++;		// skip the initial '<'
    while (*p && nest > 0)
    {
	// Below we can check for p-s >= 9 instead of 8 because
	// *s is '<' and cannot be part of "operator".
	if (*p == '<')
	{
	    if (p-s >= 9 && strncmp(p-8, "operator", 8) == 0) {
		if (p[1] == '<')
		    p++;
	    } else {
		nest++;
	    }
	}
	else if (*p == '>')
	{
	    if (p-s >= 9 && strncmp(p-8, "operator", 8) == 0) {
		if (p[1] == '>')
		    p++;
	    } else {
		nest--;
	    }
	}
	p++;
    }
    if (nest != 0) {
	TRACE(QString().sprintf("parse error: mismatching <> at %-20.20s", s));
    }
    s = p;
}

/**
 * Find the end of line that is not inside braces
 */
static void findEnd(const char*& s)
{
    const char* p = s;
    while (*p && *p!='\n') {
	while (*p && *p!='\n' && *p!='{')
	    p++;
	if (*p=='{') {
	    p++;
	    skipNested(p, '{', '}'); p--;
	}
    }
    s = p;
}

static bool isNumberish(const char ch)
{
    return (ch>='0' && ch<='9') || ch=='.' || ch=='x'; 
}

void skipString(const char*& p)
{
moreStrings:
    // opening quote
    char quote = *p++;
    while (*p != quote) {
	if (*p == '\\') {
	    // skip escaped character
	    // no special treatment for octal values necessary
	    p++;
	}
	// simply return if no more characters
	if (*p == '\0')
	    return;
	p++;
    }
    // closing quote
    p++;
    /*
     * Strings can consist of several parts, some of which contain repeated
     * characters.
     */
    if (quote == '\'') {
	// look ahaead for <repeats 123 times>
	const char* q = p+1;
	while (isspace(*q))
	    q++;
	if (strncmp(q, "<repeats ", 9) == 0) {
	    p = q+9;
	    while (*p != '\0' && *p != '>')
		p++;
	    if (*p != '\0') {
		p++;			/* skip the '>' */
	    }
	}
    }
    // is the string continued?
    if (*p == ',') {
	// look ahead for another quote
	const char* q = p+1;
	while (isspace(*q))
	    q++;
	if (*q == '"' || *q == '\'') {
	    // yes!
	    p = q;
	    goto moreStrings;
	}
    }
    /* very long strings are followed by `...' */
    if (*p == '.' && p[1] == '.' && p[2] == '.') {
	p += 3;
    }
}

static void skipNestedWithString(const char*& s, char opening, char closing)
{
    const char* p = s;

    // parse a nested expression
    int nest = 1;
    p++;
    /*
     * Search for next matching `closing' char, skipping nested pairs of
     * `opening' and `closing' as well as strings.
     */
    while (*p && nest > 0) {
	if (*p == opening) {
	    nest++;
	} else if (*p == closing) {
	    nest--;
	} else if (*p == '\'' || *p == '\"') {
	    skipString(p);
	    continue;
	}
	p++;
    }
    if (nest > 0) {
	TRACE(QString().sprintf("parse error: mismatching %c%c at %-20.20s", opening, closing, s));
    }
    s = p;
}

inline void skipName(const char*& p)
{
    // allow : (for enumeration values) and $ and . (for _vtbl.)
    while (isalnum(*p) || *p == '_' || *p == ':' || *p == '$' || *p == '.')
	p++;
}

static bool parseName(const char*& s, QString& name, VarTree::NameKind& kind)
{
    kind = VarTree::NKplain;

    const char* p = s;
    // examples of names:
    //  name
    //  <Object>
    //  <string<a,b<c>,7> >

    if (*p == '<') {
	skipNestedAngles(p);
	name = QString::fromLatin1(s, p - s);
	kind = VarTree::NKtype;
    }
    else
    {
	// name, which might be "static"; allow dot for "_vtbl."
	skipName(p);
	if (p == s) {
	    TRACE(QString().sprintf("parse error: not a name %-20.20s", s));
	    return false;
	}
	int len = p - s;
	if (len == 6 && strncmp(s, "static", 6) == 0) {
	    kind = VarTree::NKstatic;

	    // its a static variable, name comes now
	    while (isspace(*p))
		p++;
	    s = p;
	    skipName(p);
	    if (p == s) {
		TRACE(QString().sprintf("parse error: not a name after static %-20.20s", s));
		return false;
	    }
	    len = p - s;
	}
	name = QString::fromLatin1(s, len);
    }
    // return the new position
    s = p;
    return true;
}

static bool parseValue(const char*& s, ExprValue* variable)
{
    variable->m_value = "";

repeat:
    if (*s == '{') {
	// Sometimes we find the following output:
	//  {<text variable, no debug info>} 0x40012000 <access>
	//  {<data variable, no debug info>}
	//  {<variable (not text or data), no debug info>}
	if (strncmp(s, "{<text variable, ", 17) == 0 ||
	    strncmp(s, "{<data variable, ", 17) == 0 ||
	    strncmp(s, "{<variable (not text or data), ", 31) == 0)
	{
	    const char* start = s;
	    skipNested(s, '{', '}');
	    variable->m_value = QString::fromLatin1(start, s-start);
	    variable->m_value += ' ';	// add only a single space
	    while (isspace(*s))
		s++;
	    goto repeat;
	}
	else
	{
	    s++;
	    if (!parseNested(s, variable)) {
		return false;
	    }
	    // must be the closing brace
	    if (*s != '}') {
		TRACE("parse error: missing } of " +  variable->m_name);
		return false;
	    }
	    s++;
	    // final white space
	    while (isspace(*s))
		s++;
	}
    } else {
	// examples of leaf values (cannot be the empty string):
	//  123
	//  -123
	//  23.575e+37
	//  0x32a45
	//  @0x012ab4
	//  (DwContentType&) @0x8123456: {...}
	//  0x32a45 "text"
	//  10 '\n'
	//  <optimized out>
	//  0x823abc <Array<int> virtual table>
	//  (void (*)()) 0x8048480 <f(E *, char)>
	//  (E *) 0xbffff450
	//  red
	//  &parseP (HTMLClueV *, char *)
	//  Variable "x" is not available.
	//  The value of variable 'x' is distributed...
	//  -nan(0xfffff081defa0)

	const char*p = s;
    
	// check for type
	QString type;
	if (*p == '(') {
	    skipNested(p, '(', ')');

	    while (isspace(*p))
		p++;
	    variable->m_value = QString::fromLatin1(s, p - s);
	}

	bool reference = false;
	if (*p == '@') {
	    // skip reference marker
	    p++;
	    reference = true;
	}
	const char* start = p;
	if (*p == '-')
	    p++;

	// some values consist of more than one token
	bool checkMultiPart = false;

	if (p[0] == '0' && p[1] == 'x') {
	    // parse hex number
	    p += 2;
	    while (isxdigit(*p))
		p++;

	    /*
	     * Assume this is a pointer, but only if it's not a reference, since
	     * references can't be expanded.
	     */
	    if (!reference) {
		variable->m_varKind = VarTree::VKpointer;
	    } else {
		/*
		 * References are followed by a colon, in which case we'll
		 * find the value following the reference address.
		 */
		if (*p == ':') {
		    p++;
		} else {
		    // Paranoia. (Can this happen, i.e. reference not followed by ':'?)
		    reference = false;
		}
	    }
	    checkMultiPart = true;
	} else if (isdigit(*p)) {
	    // parse decimal number, possibly a float
	    while (isdigit(*p))
		p++;
	    if (*p == '.') {		/* TODO: obey i18n? */
		// In long arrays an integer may be followed by '...'.
		// We test for this situation and don't gobble the '...'.
		if (p[1] != '.' || p[0] != '.') {
		    // fractional part
		    p++;
		    while (isdigit(*p))
			p++;
		}
	    }
	    if (*p == 'e' || *p == 'E') {
		p++;
		// exponent
		if (*p == '-' || *p == '+')
		    p++;
		while (isdigit(*p))
		    p++;
	    }

	    // for char variables there is the char, eg. 10 '\n'
	    checkMultiPart = true;
	} else if (*p == '<') {
	    // e.g. <optimized out>
	    skipNestedAngles(p);
	} else if (*p == '"' || *p == '\'') {
	    // character may have multipart: '\000' <repeats 11 times>
	    checkMultiPart = *p == '\'';
	    // found a string
	    skipString(p);
	} else if (*p == '&') {
	    // function pointer
	    p++;
	    skipName(p);
	    while (isspace(*p)) {
		p++;
	    }
	    if (*p == '(') {
		skipNested(p, '(', ')');
	    }
	} else if (strncmp(p, "Variable \"", 10) == 0) {
	    // Variable "x" is not available.
	    p += 10;		// skip to "
	    skipName(p);
	    if (strncmp(p, "\" is not available.", 19) == 0) {
		p += 19;
	    }
	} else if (strncmp(p, "The value of variable '", 23) == 0) {
	    p += 23;
	    skipName(p);
	    const char* e = strchr(p, '.');
	    if (e == 0) {
		p += strlen(p);
	    } else {
		p = e+1;
	    }
	} else {
	    // must be an enumeration value
	    skipName(p);
	    // hmm, not necessarily: nan (floating point Not a Number)
	    // is followed by a number in ()
	    if (*p == '(')
		skipNested(p, '(', ')');
	}
	variable->m_value += QString::fromLatin1(start, p - start);

	// remove line breaks from the value; this is ok since
	// string values never contain a literal line break
	variable->m_value.replace('\n', ' ');

	if (checkMultiPart) {
	    // white space
	    while (isspace(*p))
		p++;
	    // may be followed by a string or <...>
	    start = p;
	    
	    if (*p == '"' || *p == '\'') {
		skipString(p);
	    } else if (*p == '<') {
		// if this value is part of an array, it might be followed
		// by <repeats 15 times>, which we don't skip here
		if (strncmp(p, "<repeats ", 9) != 0)
		    skipNestedAngles(p);
	    }
	    if (p != start) {
		// there is always a blank before the string,
		// which we will include in the final string value
		variable->m_value += QString::fromLatin1(start-1, (p - start)+1);
		// if this was a pointer, reset that flag since we 
		// now got the value
		variable->m_varKind = VarTree::VKsimple;
	    }
	}

	if (variable->m_value.length() == 0) {
	    TRACE("parse error: no value for " + variable->m_name);
	    return false;
	}

	// final white space
	while (isspace(*p))
	    p++;
	s = p;

	/*
	 * If this was a reference, the value follows. It might even be a
	 * composite variable!
	 */
	if (reference) {
	    goto repeat;
	}
    }

    return true;
}

static bool parseNested(const char*& s, ExprValue* variable)
{
    // could be a structure or an array
    while (isspace(*s))
	s++;

    const char* p = s;
    bool isStruct = false;
    /*
     * If there is a name followed by an = or an < -- which starts a type
     * name -- or "static", it is a structure
     */
    if (*p == '<' || *p == '}') {
	isStruct = true;
    } else if (strncmp(p, "static ", 7) == 0) {
	isStruct = true;
    } else if (isalpha(*p) || *p == '_' || *p == '$') {
	// look ahead for a comma after the name
	skipName(p);
	while (isspace(*p))
	    p++;
	if (*p == '=') {
	    isStruct = true;
	}
	p = s;				/* rescan the name */
    }
    if (isStruct) {
	if (!parseVarSeq(p, variable)) {
	    return false;
	}
	variable->m_varKind = VarTree::VKstruct;
    } else {
	if (!parseValueSeq(p, variable)) {
	    return false;
	}
	variable->m_varKind = VarTree::VKarray;
    }
    s = p;
    return true;
}

static bool parseVarSeq(const char*& s, ExprValue* variable)
{
    // parse a comma-separated sequence of variables
    ExprValue* var = variable;		/* var != 0 to indicate success if empty seq */
    for (;;) {
	if (*s == '}')
	    break;
	if (strncmp(s, "<No data fields>}", 17) == 0)
	{
	    // no member variables, so break out immediately
	    s += 16;			/* go to the closing brace */
	    break;
	}
	var = parseVar(s);
	if (var == 0)
	    break;			/* syntax error */
	variable->appendChild(var);
	if (*s != ',')
	    break;
	// skip the comma and whitespace
	s++;
	while (isspace(*s))
	    s++;
    }
    return var != 0;
}

static bool parseValueSeq(const char*& s, ExprValue* variable)
{
    // parse a comma-separated sequence of variables
    int index = 0;
    bool good;
    for (;;) {
	QString name;
	name.sprintf("[%d]", index);
	ExprValue* var = new ExprValue(name, VarTree::NKplain);
	good = parseValue(s, var);
	if (!good) {
	    delete var;
	    return false;
	}
	// a value may be followed by "<repeats 45 times>"
	if (strncmp(s, "<repeats ", 9) == 0) {
	    s += 9;
	    char* end;
	    int l = strtol(s, &end, 10);
	    if (end == s || strncmp(end, " times>", 7) != 0) {
		// should not happen
		delete var;
		return false;
	    }
	    TRACE(QString().sprintf("found <repeats %d times> in array", l));
	    // replace name and advance index
	    name.sprintf("[%d .. %d]", index, index+l-1);
	    var->m_name = name;
	    index += l;
	    // skip " times>" and space
	    s = end+7;
	    // possible final space
	    while (isspace(*s))
		s++;
	} else {
	    index++;
	}
	variable->appendChild(var);
	// long arrays may be terminated by '...'
	if (strncmp(s, "...", 3) == 0) {
	    s += 3;
	    ExprValue* var = new ExprValue("...", VarTree::NKplain);
	    var->m_value = i18n("<additional entries of the array suppressed>");
	    variable->appendChild(var);
	    break;
	}
	if (*s != ',') {
	    break;
	}
	// skip the comma and whitespace
	s++;
	while (isspace(*s))
	    s++;
	// sometimes there is a closing brace after a comma
//	if (*s == '}')
//	    break;
    }
    return true;
}

/**
 * Parses a stack frame.
 */
static void parseFrameInfo(const char*& s, QString& func,
			   QString& file, int& lineNo, DbgAddr& address)
{
    const char* p = s;

    // next may be a hexadecimal address
    if (*p == '0') {
	const char* start = p;
	p++;
	if (*p == 'x')
	    p++;
	while (isxdigit(*p))
	    p++;
	address = QString::fromLatin1(start, p-start);
	if (strncmp(p, " in ", 4) == 0)
	    p += 4;
    } else {
	address = DbgAddr();
    }
    const char* start = p;
    // check for special signal handler frame
    if (strncmp(p, "<signal handler called>", 23) == 0) {
	func = QString::fromLatin1(start, 23);
	file = QString();
	lineNo = -1;
	s = p+23;
	if (*s == '\n')
	    s++;
	return;
    }

    /*
     * Skip the function name. It is terminated by a left parenthesis
     * which does not delimit "(anonymous namespace)" and which is
     * outside the angle brackets <> of template parameter lists
     * and is preceded by a space.
     */
    while (*p != '\0')
    {
	if (*p == '<') {
	    // check for operator<< and operator<
	    if (p-start >= 8 && strncmp(p-8, "operator", 8) == 0)
	    {
		p++;
		if (*p == '<')
		    p++;
	    }
	    else
	    {
		// skip template parameter list
		skipNestedAngles(p);
	    }
	} else if (*p == '(') {
	    // this skips "(anonymous namespace)" as well as the formal
	    // parameter list of the containing function if this is a member
	    // of a nested class
	    skipNestedWithString(p, '(', ')');
	} else if (*p == ' ') {
	    ++p;
	    if (*p == '(')
		break; // parameter list found
	} else {
	    p++;
	}
    }

    if (*p == '\0') {
	func = start;
	file = QString();
	lineNo = -1;
	s = p;
	return;
    }
    /*
     * Skip parameters. But notice that for complicated conversion
     * functions (eg. "operator int(**)()()", ie. convert to pointer to
     * pointer to function) as well as operator()(...) we have to skip
     * additional pairs of parentheses. Furthermore, recent gdbs write the
     * demangled name followed by the arguments in a pair of parentheses,
     * where the demangled name can end in "const".
     */
    do {
	skipNestedWithString(p, '(', ')');
	while (isspace(*p))
	    p++;
	// skip "const"
	if (strncmp(p, "const", 5) == 0) {
	    p += 5;
	    while (isspace(*p))
		p++;
	}
    } while (*p == '(');

    // check for file position
    if (strncmp(p, "at ", 3) == 0) {
	p += 3;
	const char* fileStart = p;
	// go for the end of the line
	while (*p != '\0' && *p != '\n')
	    p++;
	// search back for colon
	const char* colon = p;
	do {
	    --colon;
	} while (*colon != ':');
	file = QString::fromLatin1(fileStart, colon-fileStart);
	lineNo = atoi(colon+1)-1;
	// skip new-line
	if (*p != '\0')
	    p++;
    } else {
	// check for "from shared lib"
	if (strncmp(p, "from ", 5) == 0) {
	    p += 5;
	    // go for the end of the line
	    while (*p != '\0' && *p != '\n')
		p++;
	    // skip new-line
	    if (*p != '\0')
		p++;
	}
	file = "";
	lineNo = -1;
    }
    // construct the function name (including file info)
    if (*p == '\0') {
	func = start;
    } else {
	func = QString::fromLatin1(start, p-start-1);	/* don't include \n */
    }
    s = p;

    /*
     * Replace \n (and whitespace around it) in func by a blank. We cannot
     * use QString::simplifyWhiteSpace() for this because this would also
     * simplify space that belongs to a string arguments that gdb sometimes
     * prints in the argument lists of the function.
     */
    ASSERT(!isspace(func[0].latin1()));		/* there must be non-white before first \n */
    int nl = 0;
    while ((nl = func.find('\n', nl)) >= 0) {
	// search back to the beginning of the whitespace
	int startWhite = nl;
	do {
	    --startWhite;
	} while (isspace(func[startWhite].latin1()));
	startWhite++;
	// search forward to the end of the whitespace
	do {
	    nl++;
	} while (isspace(func[nl].latin1()));
	// replace
	func.replace(startWhite, nl-startWhite, " ");
	/* continue searching for more \n's at this place: */
	nl = startWhite+1;
    }
}


/**
 * Parses a stack frame including its frame number
 */
static bool parseFrame(const char*& s, int& frameNo, QString& func,
		       QString& file, int& lineNo, DbgAddr& address)
{
    // Example:
    //  #1  0x8048881 in Dl::Dl (this=0xbffff418, r=3214) at testfile.cpp:72
    //  Breakpoint 3, Cl::f(int) const (this=0xbffff3c0, x=17) at testfile.cpp:155

    // must start with a hash mark followed by number
    // or with "Breakpoint " followed by number and comma
    if (s[0] == '#') {
	if (!isdigit(s[1]))
	    return false;
	s++;				/* skip the hash mark */
    } else if (strncmp(s, "Breakpoint ", 11) == 0) {
	if (!isdigit(s[11]))
	    return false;
	s += 11;			/* skip "Breakpoint" */
    } else
	return false;

    // frame number
    frameNo = atoi(s);
    while (isdigit(*s))
	s++;
    // space and comma
    while (isspace(*s) || *s == ',')
	s++;
    parseFrameInfo(s, func, file, lineNo, address);
    return true;
}

void GdbDriver::parseBackTrace(const char* output, std::list<StackFrame>& stack)
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

bool GdbDriver::parseFrameChange(const char* output, int& frameNo,
				 QString& file, int& lineNo, DbgAddr& address)
{
    QString func;
    return ::parseFrame(output, frameNo, func, file, lineNo, address);
}


bool GdbDriver::parseBreakList(const char* output, std::list<Breakpoint>& brks)
{
    // skip first line, which is the headline
    const char* p = strchr(output, '\n');
    if (p == 0)
	return false;
    p++;
    if (*p == '\0')
	return false;

    // split up a line
    const char* end;
    char* dummy;
    while (*p != '\0') {
	Breakpoint bp;
	// get Num
	bp.id = strtol(p, &dummy, 10);	/* don't care about overflows */
	p = dummy;
	// get Type
	while (isspace(*p))
	    p++;
	if (strncmp(p, "breakpoint", 10) == 0) {
	    p += 10;
	} else if (strncmp(p, "hw watchpoint", 13) == 0) {
	    bp.type = Breakpoint::watchpoint;
	    p += 13;
	} else if (strncmp(p, "watchpoint", 10) == 0) {
	    bp.type = Breakpoint::watchpoint;
	    p += 10;
	}
	while (isspace(*p))
	    p++;
	if (*p == '\0')
	    break;
	// get Disp
	bp.temporary = *p++ == 'd';
	while (*p != '\0' && !isspace(*p))	/* "keep" or "del" */
	    p++;
	while (isspace(*p))
	    p++;
	if (*p == '\0')
	    break;
	// get Enb
	bp.enabled = *p++ == 'y';
	while (*p != '\0' && !isspace(*p))	/* "y" or "n" */
	    p++;
	while (isspace(*p))
	    p++;
	if (*p == '\0')
	    break;
	// the address, if present
	if (bp.type == Breakpoint::breakpoint &&
	    strncmp(p, "0x", 2) == 0)
	{
	    const char* start = p;
	    while (*p != '\0' && !isspace(*p))
		p++;
	    bp.address = QString::fromLatin1(start, p-start);
	    while (isspace(*p) && *p != '\n')
		p++;
	    if (*p == '\0')
		break;
	}
	// remainder is location, hit and ignore count, condition
	end = strchr(p, '\n');
	if (end == 0) {
	    bp.location = p;
	    p += bp.location.length();
	} else {
	    bp.location = QString::fromLatin1(p, end-p).stripWhiteSpace();
	    p = end+1;			/* skip over \n */
	}

	// may be continued in next line
	while (isspace(*p)) {	/* p points to beginning of line */
	    // skip white space at beginning of line
	    while (isspace(*p))
		p++;

	    // seek end of line
	    end = strchr(p, '\n');
	    if (end == 0)
		end = p+strlen(p);

	    if (strncmp(p, "breakpoint already hit", 22) == 0) {
		// extract the hit count
		p += 22;
		bp.hitCount = strtol(p, &dummy, 10);
		TRACE(QString("hit count %1").arg(bp.hitCount));
	    } else if (strncmp(p, "stop only if ", 13) == 0) {
		// extract condition
		p += 13;
		bp.condition = QString::fromLatin1(p, end-p).stripWhiteSpace();
		TRACE("condition: "+bp.condition);
	    } else if (strncmp(p, "ignore next ", 12) == 0) {
		// extract ignore count
		p += 12;
		bp.ignoreCount = strtol(p, &dummy, 10);
		TRACE(QString("ignore count %1").arg(bp.ignoreCount));
	    } else {
		// indeed a continuation
		bp.location += " " + QString::fromLatin1(p, end-p).stripWhiteSpace();
	    }
	    p = end;
	    if (*p != '\0')
		p++;			/* skip '\n' */
	}
	brks.push_back(bp);
    }
    return true;
}

std::list<ThreadInfo> GdbDriver::parseThreadList(const char* output)
{
    std::list<ThreadInfo> threads;
    if (strcmp(output, "\n") == 0 || strncmp(output, "No stack.", 9) == 0) {
	// no threads
	return threads;
    }

    const char* p = output;
    while (*p != '\0') {
	ThreadInfo thr;
	// seach look for thread id, watching out for  the focus indicator
	thr.hasFocus = false;
	while (isspace(*p))		/* may be \n from prev line: see "No stack" below */
	    p++;
	if (*p == '*') {
	    thr.hasFocus = true;
	    p++;
	    // there follows only whitespace
	}
	char* end;
	thr.id = strtol(p, &end, 10);
	if (p == end) {
	    // syntax error: no number found; bail out
	    return threads;
	}
	p = end;

	// skip space
	while (isspace(*p))
	    p++;

	/*
	 * Now follows the thread's SYSTAG. It is terminated by two blanks.
	 */
	end = strstr(p, "  ");
	if (end == 0) {
	    // syntax error; bail out
	    return threads;
	}
	thr.threadName = QString::fromLatin1(p, end-p);
	p = end+2;

	/*
	 * Now follows a standard stack frame. Sometimes, however, gdb
	 * catches a thread at an instant where it doesn't have a stack.
	 */
	if (strncmp(p, "[No stack.]", 11) != 0) {
	    ::parseFrameInfo(p, thr.function, thr.fileName, thr.lineNo, thr.address);
	} else {
	    thr.function = "[No stack]";
	    thr.lineNo = -1;
	    p += 11;			/* \n is skipped above */
	}

	threads.push_back(thr);
    }
    return threads;
}

static bool parseNewBreakpoint(const char* o, int& id,
			       QString& file, int& lineNo, QString& address);
static bool parseNewWatchpoint(const char* o, int& id,
			       QString& expr);

bool GdbDriver::parseBreakpoint(const char* output, int& id,
				QString& file, int& lineNo, QString& address)
{
    const char* o = output;
    // skip lines of that begin with "(Cannot find"
    while (strncmp(o, "(Cannot find", 12) == 0) {
	o = strchr(o, '\n');
	if (o == 0)
	    return false;
	o++;				/* skip newline */
    }

    if (strncmp(o, "Breakpoint ", 11) == 0) {
	output += 11;			/* skip "Breakpoint " */
	return ::parseNewBreakpoint(output, id, file, lineNo, address);
    } else if (strncmp(o, "Hardware watchpoint ", 20) == 0) {
	output += 20;
	return ::parseNewWatchpoint(output, id, address);
    } else if (strncmp(o, "Watchpoint ", 11) == 0) {
	output += 11;
	return ::parseNewWatchpoint(output, id, address);
    }
    return false;
}

static bool parseNewBreakpoint(const char* o, int& id,
			       QString& file, int& lineNo, QString& address)
{
    // breakpoint id
    char* p;
    id = strtoul(o, &p, 10);
    if (p == o)
	return false;

    // check for the address
    if (strncmp(p, " at 0x", 6) == 0) {
	char* start = p+4;	       /* skip " at ", but not 0x */
	p += 6;
	while (isxdigit(*p))
	    ++p;
	address = QString::fromLatin1(start, p-start);
    }
    
    // file name
    char* fileStart = strstr(p, "file ");
    if (fileStart == 0)
	return !address.isEmpty();     /* parse error only if there's no address */
    fileStart += 5;
    
    // line number
    char* numStart = strstr(fileStart, ", line ");
    QString fileName = QString::fromLatin1(fileStart, numStart-fileStart);
    numStart += 7;
    int line = strtoul(numStart, &p, 10);
    if (numStart == p)
	return false;

    file = fileName;
    lineNo = line-1;			/* zero-based! */
    return true;
}

static bool parseNewWatchpoint(const char* o, int& id,
			       QString& expr)
{
    // watchpoint id
    char* p;
    id = strtoul(o, &p, 10);
    if (p == o)
	return false;

    if (strncmp(p, ": ", 2) != 0)
	return false;
    p += 2;

    // all the rest on the line is the expression
    expr = QString::fromLatin1(p, strlen(p)).stripWhiteSpace();
    return true;
}

void GdbDriver::parseLocals(const char* output, std::list<ExprValue*>& newVars)
{
    // check for possible error conditions
    if (strncmp(output, "No symbol table", 15) == 0)
    {
	return;
    }

    while (*output != '\0') {
	while (isspace(*output))
	    output++;
	if (*output == '\0')
	    break;
	// skip occurrences of "No locals" and "No args"
	if (strncmp(output, "No locals", 9) == 0 ||
	    strncmp(output, "No arguments", 12) == 0)
	{
	    output = strchr(output, '\n');
	    if (output == 0) {
		break;
	    }
	    continue;
	}

	ExprValue* variable = parseVar(output);
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

ExprValue* GdbDriver::parsePrintExpr(const char* output, bool wantErrorValue)
{
    ExprValue* var = 0;
    // check for error conditions
    if (!parseErrorMessage(output, var, wantErrorValue))
    {
	// parse the variable
	var = parseVar(output);
    }
    return var;
}

bool GdbDriver::parseChangeWD(const char* output, QString& message)
{
    bool isGood = false;
    message = QString(output).simplifyWhiteSpace();
    if (message.isEmpty()) {
	message = i18n("New working directory: ") + m_programWD;
	isGood = true;
    }
    return isGood;
}

bool GdbDriver::parseChangeExecutable(const char* output, QString& message)
{
    message = output;

    m_haveCoreFile = false;

    /*
     * Lines starting with the following do not indicate errors:
     *     Using host libthread_db
     *     (no debugging symbols found)
     */
    while (strncmp(output, "Using host libthread_db", 23) == 0 ||
	   strncmp(output, "(no debugging symbols found)", 28) == 0)
    {
	// this line is good, go to the next one
	const char* end = strchr(output, '\n');
	if (end == 0)
	    output += strlen(output);
	else
	    output = end+1;
    }

    /*
     * If we've parsed all lines, there was no error.
     */
    return output[0] == '\0';
}

bool GdbDriver::parseCoreFile(const char* output)
{
    // if command succeeded, gdb emits a line starting with "#0 "
    m_haveCoreFile = strstr(output, "\n#0 ") != 0;
    return m_haveCoreFile;
}

uint GdbDriver::parseProgramStopped(const char* output, QString& message)
{
    // optionally: "program changed, rereading symbols",
    // followed by:
    // "Program exited normally"
    // "Program terminated with wignal SIGSEGV"
    // "Program received signal SIGINT" or other signal
    // "Breakpoint..."
    
    // go through the output, line by line, checking what we have
    const char* start = output - 1;
    uint flags = SFprogramActive;
    message = QString();
    do {
	start++;			/* skip '\n' */

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
		(strncmp(start, "Program terminated", 18) == 0 && !m_haveCoreFile) ||
		strncmp(start, "ptrace: ", 8) == 0)
	    {
		flags &= ~SFprogramActive;
	    }

	    // set message
	    const char* endOfMessage = strchr(start, '\n');
	    if (endOfMessage == 0)
		endOfMessage = start + strlen(start);
	    message = QString::fromLatin1(start, endOfMessage-start);
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

QStringList GdbDriver::parseSharedLibs(const char* output)
{
    QStringList shlibs;
    if (strncmp(output, "No shared libraries loaded", 26) == 0)
	return shlibs;

    // parse the table of shared libraries

    // strip off head line
    output = strchr(output, '\n');
    if (output == 0)
	return shlibs;
    output++;				/* skip '\n' */
    QString shlibName;
    while (*output != '\0') {
	// format of a line is
	// 0x404c5000  0x40580d90  Yes         /lib/libc.so.5
	// 3 blocks of non-space followed by space
	for (int i = 0; *output != '\0' && i < 3; i++) {
	    while (*output != '\0' && !isspace(*output)) {	/* non-space */
		output++;
	    }
	    while (isspace(*output)) {	/* space */
		output++;
	    }
	}
	if (*output == '\0')
	    return shlibs;
	const char* start = output;
	output = strchr(output, '\n');
	if (output == 0)
	    output = start + strlen(start);
	shlibName = QString::fromLatin1(start, output-start);
	if (*output != '\0')
	    output++;
	shlibs.append(shlibName);
	TRACE("found shared lib " + shlibName);
    }
    return shlibs;
}

bool GdbDriver::parseFindType(const char* output, QString& type)
{
    if (strncmp(output, "type = ", 7) != 0)
	return false;

    /*
     * Everything else is the type. We strip off any leading "const" and any
     * trailing "&" on the grounds that neither affects the decoding of the
     * object. We also strip off all white-space from the type.
     */
    output += 7;
    if (strncmp(output, "const ", 6) == 0)
        output += 6;
    type = output;
    type.replace(QRegExp("\\s+"), "");
    if (type.endsWith("&"))
        type.truncate(type.length() - 1);
    return true;
}

std::list<RegisterInfo> GdbDriver::parseRegisters(const char* output)
{
    std::list<RegisterInfo> regs;
    if (strncmp(output, "The program has no registers now", 32) == 0) {
	return regs;
    }

    QString value;

    // parse register values
    while (*output != '\0')
    {
	RegisterInfo reg;
	// skip space at the start of the line
	while (isspace(*output))
	    output++;

	// register name
	const char* start = output;
	while (*output != '\0' && !isspace(*output))
	    output++;
	if (*output == '\0')
	    break;
	reg.regName = QString::fromLatin1(start, output-start);

	// skip space
	while (isspace(*output))
	    output++;

	/*
	 * If we find a brace now, this is a vector register. We look for
	 * the closing brace and treat the result as cooked value.
	 */
	if (*output == '{')
	{
	    start = output;
	    skipNested(output, '{', '}');
	    value = QString::fromLatin1(start, output-start).simplifyWhiteSpace();
	    // skip space, but not the end of line
	    while (isspace(*output) && *output != '\n')
		output++;
	    // get rid of the braces at the begining and the end
	    value.remove(0, 1);
	    if (value[value.length()-1] == '}') {
		value = value.left(value.length()-1);
	    }
	    // gdb 5.3 doesn't print a separate set of raw values
	    if (*output == '{') {
		// another set of vector follows
		// what we have so far is the raw value
		reg.rawValue = value;

		start = output;
		skipNested(output, '{', '}');
		value = QString::fromLatin1(start, output-start).simplifyWhiteSpace();
	    } else {
		// for gdb 5.3
		// find first type that does not have an array, this is the RAW value
		const char* end=start;
		findEnd(end);
		const char* cur=start; 
		while (cur<end) {
		    while (*cur != '=' && cur<end)
			cur++;
		    cur++; 
		    while (isspace(*cur) && cur<end)
			cur++;
		    if (isNumberish(*cur)) {
			end=cur;
			while (*end && (*end!='}') && (*end!=',') && (*end!='\n'))
			    end++;
			QString rawValue = QString::fromLatin1(cur, end-cur).simplifyWhiteSpace();
			reg.rawValue = rawValue;

			if (rawValue.left(2)=="0x") {
			    // ok we have a raw value, now get it's type
			    end=cur-1;
			    while (isspace(*end) || *end=='=') end--;
			    end++;
			    cur=end-1;
			    while (*cur!='{' && *cur!=' ')
				cur--;
			    cur++;
			    reg.type = QString::fromLatin1(cur, end-cur);
			}

			// end while loop 
			cur=end;
		    }
		}
		// skip to the end of line
		while (*output != '\0' && *output != '\n')
		    output++;
		// get rid of the braces at the begining and the end
		value.remove(0, 1);
		if (value[value.length()-1] == '}') {
		    value.truncate(value.length()-1);
		}
	    }
	    reg.cookedValue = value;
	}
	else
	{
	    // the rest of the line is the register value
	    start = output;
	    output = strchr(output,'\n');
	    if (output == 0)
		output = start + strlen(start);
	    value = QString::fromLatin1(start, output-start).simplifyWhiteSpace();

	    /*
	     * We split the raw from the cooked values.
	     * Some modern gdbs explicitly say: "0.1234 (raw 0x3e4567...)".
	     * Here, the cooked value comes first, and the raw value is in
	     * the second part.
	     */
	    int pos = value.find(" (raw ");
	    if (pos >= 0)
	    {
		reg.cookedValue = value.left(pos);
		reg.rawValue = value.mid(pos+6);
		if (reg.rawValue.right(1) == ")")	// remove closing bracket
		    reg.rawValue.truncate(reg.rawValue.length()-1);
	    }
	    else
	    {
		/*
		* In other cases we split off the first token (separated by
		* whitespace). It is the raw value. The remainder of the line
		* is the cooked value.
		*/
		int pos = value.find(' ');
		if (pos < 0) {
		    reg.rawValue = value;
		    reg.cookedValue = QString();
		} else {
		    reg.rawValue = value.left(pos);
		    reg.cookedValue = value.mid(pos+1);
		}
	    }
	}
	if (*output != '\0')
	    output++;			/* skip '\n' */

	regs.push_back(reg);
    }
    return regs;
}

bool GdbDriver::parseInfoLine(const char* output, QString& addrFrom, QString& addrTo)
{
    // "is at address" or "starts at address"
    const char* start = strstr(output, "s at address ");
    if (start == 0)
	return false;

    start += 13;
    const char* p = start;
    while (*p != '\0' && !isspace(*p))
	p++;
    addrFrom = QString::fromLatin1(start, p-start);

    start = strstr(p, "and ends at ");
    if (start == 0) {
	addrTo = addrFrom;
	return true;
    }

    start += 12;
    p = start;
    while (*p != '\0' && !isspace(*p))
	p++;
    addrTo = QString::fromLatin1(start, p-start);

    return true;
}

std::list<DisassembledCode> GdbDriver::parseDisassemble(const char* output)
{
    std::list<DisassembledCode> code;

    if (strncmp(output, "Dump of assembler", 17) != 0) {
	// error message?
	DisassembledCode c;
	c.code = output;
	code.push_back(c);
	return code;
    }

    // remove first line
    const char* p = strchr(output, '\n');
    if (p == 0)
	return code;			/* not a regular output */

    p++;

    // remove last line
    const char* end = strstr(output, "End of assembler");
    if (end == 0)
	end = p + strlen(p);

    // remove function offsets from the lines
    while (p != end)
    {
	DisassembledCode c;
	const char* start = p;
	// address
	while (p != end && !isspace(*p))
	    p++;
	c.address = QString::fromLatin1(start, p-start);

	// function name (enclosed in '<>', followed by ':')
	while (p != end && *p != '<')
	    p++;
	if (*p == '<')
	    skipNestedAngles(p);
	if (*p == ':')
	    p++;

	// space until code
	while (p != end && isspace(*p))
	    p++;

	// code until end of line
	start = p;
	while (p != end && *p != '\n')
	    p++;
	if (p != end)			/* include '\n' */
	    p++;

	c.code = QString::fromLatin1(start, p-start);
	code.push_back(c);
    }
    return code;
}

QString GdbDriver::parseMemoryDump(const char* output, std::list<MemoryDump>& memdump)
{
    if (isErrorExpr(output)) {
	// error; strip space
	QString msg = output;
	return msg.stripWhiteSpace();
    }

    const char* p = output;		/* save typing */

    // the address
    while (*p != 0) {
	MemoryDump md;

	const char* start = p;
	while (*p != '\0' && *p != ':' && !isspace(*p))
	    p++;
	md.address = QString::fromLatin1(start, p-start);
	if (*p != ':') {
	    // parse function offset
	    while (isspace(*p))
		p++;
	    start = p;
	    while (*p != '\0' && !(*p == ':' && isspace(p[1])))
		p++;
	    md.address.fnoffs = QString::fromLatin1(start, p-start);
	}
	if (*p == ':')
	    p++;
	// skip space; this may skip a new-line char!
	while (isspace(*p))
	    p++;
	// everything to the end of the line is the memory dump
	const char* end = strchr(p, '\n');
	if (end != 0) {
	    md.dump = QString::fromLatin1(p, end-p);
	    p = end+1;
	} else {
	    md.dump = QString::fromLatin1(p, strlen(p));
	    p += strlen(p);
	}
	memdump.push_back(md);
    }
    
    return QString();
}

QString GdbDriver::editableValue(VarTree* value)
{
    const char* s = value->value().latin1();

    // if the variable is a pointer value that contains a cast,
    // remove the cast
    if (*s == '(') {
	skipNested(s, '(', ')');
	// skip space
	while (isspace(*s))
	    ++s;
    }

repeat:
    const char* start = s;

    if (strncmp(s, "0x", 2) == 0)
    {
	s += 2;
	while (isxdigit(*s))
	    ++s;

	/*
	 * What we saw so far might have been a reference. If so, edit the
	 * referenced value. Otherwise, edit the pointer.
	 */
	if (*s == ':') {
	    // a reference
	    ++s;
	    goto repeat;
	}
	// a pointer
	// if it's a pointer to a string, remove the string
	const char* end = s;
	while (isspace(*s))
	    ++s;
	if (*s == '"') {
	    // a string
	    return QString::fromLatin1(start, end-start);
	} else {
	    // other pointer
	    return QString::fromLatin1(start, strlen(start));
	}
    }

    // else leave it unchanged (or stripped of the reference preamble)
    return s;
}

QString GdbDriver::parseSetVariable(const char* output)
{
    // if there is any output, it is an error message
    QString msg = output;
    return msg.stripWhiteSpace();
}


#include "gdbdriver.moc"
