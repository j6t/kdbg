// $Id$

// Copyright by Johannes Sixt
// This file is under GPL, the GNU General Public Licence

#include "gdbdriver.h"
#include "exprwnd.h"
#include <qregexp.h>
#if QT_VERSION >= 200
#include <klocale.h>			/* i18n */
#else
#include <kapp.h>
#endif
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
static VarTree* parseVar(const char*& s);
static bool parseName(const char*& s, QString& name, VarTree::NameKind& kind);
static bool parseValue(const char*& s, VarTree* variable);
static bool parseNested(const char*& s, VarTree* variable);
static bool parseVarSeq(const char*& s, VarTree* variable);
static bool parseValueSeq(const char*& s, VarTree* variable);

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

static const char printQStringStructFmt[] =
		// if the string data is junk, fail early
		"print ($qstrunicode=($qstrdata=(%s))->unicode)?"
		// print an array of shorts
		"(*(unsigned short*)$qstrunicode)@"
		// limit the length
		"(($qstrlen=(unsigned int)($qstrdata->len))>100?100:$qstrlen)"
		// if unicode data is 0, check if it is QString::null
		":($qstrdata==QString::null.d)\n";

// However, gdb can't always find QString::null (I don't know why)
// which results in an error; we autodetect this situation in
// parseQt2QStrings and revert to a safe expression.
// Same as above except for last line:
static const char printQStringStructNoNullFmt[] =
		"print ($qstrunicode=($qstrdata=(%s))->unicode)?"
		"(*(unsigned short*)$qstrunicode)@"
		"(($qstrlen=(unsigned int)($qstrdata->len))>100?100:$qstrlen)"
		":1==0\n";

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
    { DCinfolinemain, "info line main\n", GdbCmdInfo::argNone },
    { DCinfolocals, "kdbg__alllocals\n", GdbCmdInfo::argNone },
    { DCinforegisters, "info all-registers\n", GdbCmdInfo::argNone},
    { DCinfoline, "info line %s:%d\n", GdbCmdInfo::argStringNum },
    { DCdisassemble, "disassemble %s %s\n", GdbCmdInfo::argString2 },
    { DCsetargs, "set args %s\n", GdbCmdInfo::argString },
    { DCsetenv, "set env %s %s\n", GdbCmdInfo::argString2 },
    { DCunsetenv, "unset env %s\n", GdbCmdInfo::argString },
    { DCcd, "cd %s\n", GdbCmdInfo::argString },
    { DCbt, "bt\n", GdbCmdInfo::argNone },
    { DCrun, "run\n", GdbCmdInfo::argNone },
    { DCcont, "cont\n", GdbCmdInfo::argNone },
    { DCstep, "step\n", GdbCmdInfo::argNone },
    { DCnext, "next\n", GdbCmdInfo::argNone },
    { DCfinish, "finish\n", GdbCmdInfo::argNone },
    { DCuntil, "until %s:%d\n", GdbCmdInfo::argStringNum },
    { DCkill, "kill\n", GdbCmdInfo::argNone },
    { DCbreaktext, "break %s\n", GdbCmdInfo::argString },
    { DCbreakline, "break %s:%d\n", GdbCmdInfo::argStringNum },
    { DCtbreakline, "tbreak %s:%d\n", GdbCmdInfo::argStringNum },
    { DCbreakaddr, "break *%s\n", GdbCmdInfo::argString },
    { DCtbreakaddr, "tbreak *%s\n", GdbCmdInfo::argString },
    { DCdelete, "delete %d\n", GdbCmdInfo::argNum },
    { DCenable, "enable %d\n", GdbCmdInfo::argNum },
    { DCdisable, "disable %d\n", GdbCmdInfo::argNum },
    { DCprint, "print %s\n", GdbCmdInfo::argString },
    { DCprintStruct, "print %s\n", GdbCmdInfo::argString },
    { DCprintQStringStruct, printQStringStructFmt, GdbCmdInfo::argString},
    { DCframe, "frame %d\n", GdbCmdInfo::argNum },
    { DCfindType, "whatis %s\n", GdbCmdInfo::argString },
    { DCinfosharedlib, "info sharedlibrary\n", GdbCmdInfo::argNone },
    { DCinfobreak, "info breakpoints\n", GdbCmdInfo::argNone },
    { DCcondition, "condition %d %s\n", GdbCmdInfo::argNumString},
    { DCignore, "ignore %d %d\n", GdbCmdInfo::argNum2},
};

#define NUM_CMDS (int(sizeof(cmds)/sizeof(cmds[0])))
#define MAX_FMTLEN 200

GdbDriver::GdbDriver() :
	DebuggerDriver(),
	m_gdbMajor(4), m_gdbMinor(16)
{
    strcpy(m_prompt, PROMPT);
    m_promptLen = PROMPT_LEN;
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
    assert(strlen(printQStringStructNoNullFmt) <= MAX_FMTLEN);
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
    return defaultGdb();
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
	 * Write a short macro that prints all locals: local variables and
	 * function arguments.
	 */
	"define kdbg__alllocals\n"
	"info locals\n"			/* local vars supersede args with same name */
	"info args\n"			/* therefore, arguments must come last */
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
		cmds[DCcorefile].fmt = "target core %s\n";
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
    case DCnext:
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

    int lineNoStart = MarkerRE.match(startMarker);
    if (lineNoStart >= 0) {
	int lineNo = atoi(startMarker + lineNoStart+1);

	// now show the window
	startMarker[lineNoStart] = '\0';   /* split off file name */
	emit activateFileLine(startMarker, lineNo-1);
    }
}


#if QT_VERSION < 200
#define LATIN1(str) ((str).isNull() ? "" : (str).data())
#else
#define LATIN1(str) (str).latin1()
#endif

QString GdbDriver::makeCmdString(DbgCommand cmd, QString strArg)
{
    assert(cmd >= 0 && cmd < NUM_CMDS);
    assert(cmds[cmd].argsNeeded == GdbCmdInfo::argString);

    if (cmd == DCcd) {
	// need the working directory when parsing the output
	m_programWD = strArg;
    } else if (cmd == DCsetargs) {
	// attach saved redirection
#if QT_VERSION < 200
	strArg.detach();
#endif
	strArg += m_redirect;
    }

    SIZED_QString(cmdString, MAX_FMTLEN+strArg.length());
    cmdString.sprintf(cmds[cmd].fmt, LATIN1(strArg));
    return cmdString;
}

QString GdbDriver::makeCmdString(DbgCommand cmd, int intArg)
{
    assert(cmd >= 0 && cmd < NUM_CMDS);
    assert(cmds[cmd].argsNeeded == GdbCmdInfo::argNum);

    SIZED_QString(cmdString, MAX_FMTLEN+30);

    cmdString.sprintf(cmds[cmd].fmt, intArg);
    return cmdString;
}

QString GdbDriver::makeCmdString(DbgCommand cmd, QString strArg, int intArg)
{
    assert(cmd >= 0 && cmd < NUM_CMDS);
    assert(cmds[cmd].argsNeeded == GdbCmdInfo::argStringNum ||
	   cmds[cmd].argsNeeded == GdbCmdInfo::argNumString ||
	   cmd == DCtty);

    SIZED_QString(cmdString, MAX_FMTLEN+30+strArg.length());

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
	    " </dev/null",
	    " >/dev/null",
	    " </dev/null >/dev/null",
	    " 2>/dev/null",
	    " </dev/null 2>/dev/null",
	    " >/dev/null 2>&1",
	    " </dev/null >/dev/null 2>&1"
	};
	if (strArg.isEmpty())
	    intArg = 7;			/* failsafe if no tty */
	m_redirect = runRedir[intArg & 7];

	return makeCmdString(DCtty, strArg);   /* note: no problem if strArg empty */
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
	cmdString.sprintf(cmds[cmd].fmt, LATIN1(strArg), intArg);
    }
    else
    {
	cmdString.sprintf(cmds[cmd].fmt, intArg, LATIN1(strArg));
    }
    return cmdString;
}

QString GdbDriver::makeCmdString(DbgCommand cmd, QString strArg1, QString strArg2)
{
    assert(cmd >= 0 && cmd < NUM_CMDS);
    assert(cmds[cmd].argsNeeded == GdbCmdInfo::argString2);

    SIZED_QString(cmdString, MAX_FMTLEN+strArg1.length()+strArg2.length());
    cmdString.sprintf(cmds[cmd].fmt, LATIN1(strArg1), LATIN1(strArg2));
    return cmdString;
}

QString GdbDriver::makeCmdString(DbgCommand cmd, int intArg1, int intArg2)
{
    assert(cmd >= 0 && cmd < NUM_CMDS);
    assert(cmds[cmd].argsNeeded == GdbCmdInfo::argNum2);

    SIZED_QString(cmdString, MAX_FMTLEN+60);
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
	strncmp(output, "No symbol \"", 11) == 0 ||
	strncmp(output, "Internal error: ", 16) == 0;
}

/**
 * Returns true if the output is an error message. If wantErrorValue is
 * true, a new VarTree object is created and filled with the error message.
 */
static bool parseErrorMessage(const char* output,
			      VarTree*& variable, bool wantErrorValue)
{
    if (isErrorExpr(output))
    {
	if (wantErrorValue) {
	    // put the error message as value in the variable
	    variable = new VarTree(QString(), VarTree::NKplain);
	    const char* endMsg = strchr(output, '\n');
	    if (endMsg == 0)
		endMsg = output + strlen(output);
	    variable->m_value = FROM_LATIN1(output, endMsg-output);
	} else {
	    variable = 0;
	}
	return true;
    }
    return false;
}

#if QT_VERSION < 200
struct QChar {
    // this is the XChar2b on X11
    uchar row;
    uchar cell;
    static QString toQString(QChar* unicode, int len);
    QChar() : row(0), cell(0) { }
    QChar(char c) : row(0), cell(c) { }
    operator char() const { return row ? 0 : cell; }
};

QString QChar::toQString(QChar* unicode, int len)
{
    QString result(len+1);
    char* data = result.data();
    data[len] = '\0';
    while (len >= 0) {
	data[len] = unicode[len].cell;
	--len;
    }
    return result;
}
#endif

VarTree* GdbDriver::parseQCharArray(const char* output, bool wantErrorValue)
{
    VarTree* variable = 0;

    /*
     * Parse off white space. gdb sometimes prints white space first if the
     * printed array leaded to an error.
     */
    const char* p = output;
    while (isspace(*p))
	p++;

    // check if this is an error indicating that gdb does not know about QString::null
    if (cmds[DCprintQStringStruct].fmt == printQStringStructFmt &&
	strncmp(p, "Internal error: could not find static variable null", 51) == 0)
    {
	/* QString::null doesn't work, use an alternate expression */
	cmds[DCprintQStringStruct].fmt = printQStringStructNoNullFmt;
	// continue and let parseOffErrorExpr catch the error
    }

    // special case: empty string (0 repetitions)
    if (strncmp(p, "Invalid number 0 of repetitions", 31) == 0)
    {
	variable = new VarTree(QString(), VarTree::NKplain);
	variable->m_value = "\"\"";
	return variable;
    }

    // check for error conditions
    if (parseErrorMessage(p, variable, wantErrorValue))
	return variable;

    // parse the array

    // find '='
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
		repeatCount = FROM_LATIN1(start, p-start);
		while (isspace(*p) || *p == ',')
		    p++;
	    }
	    // p is now at the next char (or the end)

	    // interpret the value as a QChar
	    // TODO: make cross-architecture compatible
	    QChar ch;
	    (unsigned short&)ch = value;

	    // escape a few frequently used characters
	    char escapeCode = '\0';
	    switch (char(ch)) {
	    case '\n': escapeCode = 'n'; break;
	    case '\r': escapeCode = 'r'; break;
	    case '\t': escapeCode = 't'; break;
	    case '\b': escapeCode = 'b'; break;
	    case '\"': escapeCode = '\"'; break;
	    case '\\': escapeCode = '\\'; break;
#if QT_VERSION < 200
		// since we only deal with ascii values must always escape '\0'
	    case '\0': escapeCode = '0'; break;
#else
	    case '\0': if (value == 0) { escapeCode = '0'; } break;
#endif
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
	variable = new VarTree(QString(), VarTree::NKplain);
	variable->m_value = result;
    }
    else if (strncmp(p, "true", 4) == 0)
    {
	variable = new VarTree(QString(), VarTree::NKplain);
	variable->m_value = "QString::null";
    }
    else if (strncmp(p, "false", 5) == 0)
    {
	variable = new VarTree(QString(), VarTree::NKplain);
	variable->m_value = "(null)";
    }
    else
	goto error;
    return variable;

error:
    if (wantErrorValue) {
	variable = new VarTree(QString(), VarTree::NKplain);
	variable->m_value = "internal parse error";
    }
    return variable;
}

static VarTree* parseVar(const char*& s)
{
    const char* p = s;
    
    // skip whitespace
    while (isspace(*p))
	p++;

    QString name;
    VarTree::NameKind kind;
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

    VarTree* variable = new VarTree(name, kind);
    variable->setDeleteChildren(true);
    
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
    if (nest > 0) {
	TRACE(QString().sprintf("parse error: mismatching %c%c at %-20.20s", opening, closing, s));
    }
    s = p;
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
	skipNested(p, '<', '>');
	name = FROM_LATIN1(s, p - s);
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
	name = FROM_LATIN1(s, len);
    }
    // return the new position
    s = p;
    return true;
}

static bool parseValue(const char*& s, VarTree* variable)
{
    variable->m_value = "";

repeat:
    if (*s == '{') {
	s++;
	if (!parseNested(s, variable)) {
	    return false;
	}
	// must be the closing brace
	if (*s != '}') {
	    TRACE("parse error: missing } of " +  variable->getText());
	    return false;
	}
	s++;
	// final white space
	while (isspace(*s))
	    s++;
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

	const char*p = s;
    
	// check for type
	QString type;
	if (*p == '(') {
	    skipNested(p, '(', ')');

	    while (isspace(*p))
		p++;
	    variable->m_value = FROM_LATIN1(s, p - s);
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
		// fractional part
		p++;
		while (isdigit(*p))
		    p++;
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
	    skipNested(p, '<', '>');
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
	} else {
	    // must be an enumeration value
	    skipName(p);
	}
	variable->m_value += FROM_LATIN1(start, p - start);

	if (checkMultiPart) {
	    // white space
	    while (isspace(*p))
		p++;
	    // may be followed by a string or <...>
	    start = p;
	    
	    if (*p == '"' || *p == '\'') {
		skipString(p);
	    } else if (*p == '<') {
		skipNested(p, '<', '>');
	    }
	    if (p != start) {
		// there is always a blank before the string,
		// which we will include in the final string value
		variable->m_value += FROM_LATIN1(start-1, (p - start)+1);
		// if this was a pointer, reset that flag since we 
		// now got the value
		variable->m_varKind = VarTree::VKsimple;
	    }
	}

	if (variable->m_value.length() == 0) {
	    TRACE("parse error: no value for " + variable->getText());
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

	if (variable->m_varKind == VarTree::VKpointer) {
	    variable->setDelayedExpanding(true);
	}
    }

    return true;
}

static bool parseNested(const char*& s, VarTree* variable)
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

static bool parseVarSeq(const char*& s, VarTree* variable)
{
    // parse a comma-separated sequence of variables
    VarTree* var = variable;		/* var != 0 to indicate success if empty seq */
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

static bool parseValueSeq(const char*& s, VarTree* variable)
{
    // parse a comma-separated sequence of variables
    int index = 0;
    bool good;
    for (;;) {
	QString name;
	name.sprintf("[%d]", index);
	index++;
	VarTree* var = new VarTree(name, VarTree::NKplain);
	var->setDeleteChildren(true);
	good = parseValue(s, var);
	if (!good) {
	    delete var;
	    return false;
	}
	variable->appendChild(var);
	if (*s != ',') {
	    break;
	}
	// skip the command and whitespace
	s++;
	while (isspace(*s))
	    s++;
	// sometimes there is a closing brace after a comma
//	if (*s == '}')
//	    break;
    }
    return true;
}

#if QT_VERSION < 200
#define ISSPACE(c) isspace((c))
#else
// c is a QChar
#define ISSPACE(c) (c).isSpace()
#endif

static bool parseFrame(const char*& s, int& frameNo, QString& func, QString& file, int& lineNo)
{
    // Example:
    //  #1  0x8048881 in Dl::Dl (this=0xbffff418, r=3214) at testfile.cpp:72

    // must start with a hash mark followed by number
    if (s[0] != '#' || !isdigit(s[1]))
	return false;

    const char* p = s;
    p++;				/* skip the hash mark */
    // frame number
    frameNo = atoi(p);
    while (isdigit(*p))
	p++;
    // space
    while (isspace(*p))
	p++;
    // next may be a hexadecimal address
    if (*p == '0') {
	p++;
	if (*p == 'x')
	    p++;
	while (isxdigit(*p))
	    p++;
	if (strncmp(p, " in ", 4) == 0)
	    p += 4;
    }
    const char* start = p;
    // search opening parenthesis
    while (*p != '\0' && *p != '(')
	p++;
    if (*p == '\0') {
	func = start;
	file = "";
	lineNo = -1;
	s = p;
	return true;
    }
    /*
     * Skip parameters. But notice that for complicated conversion
     * functions (eg. "operator int(**)()()", ie. convert to pointer to
     * pointer to function) as well as operator()(...) we have to skip
     * additional pairs of parentheses.
     */
    do {
	skipNestedWithString(p, '(', ')');
	while (isspace(*p))
	    p++;
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
	file = FROM_LATIN1(fileStart, colon-fileStart);
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
	func = FROM_LATIN1(start, p-start-1);	/* don't include \n */
    }
    s = p;

    // replace \n (and whitespace around it) in func by a blank
    ASSERT(!ISSPACE(func[0]));		/* there must be non-white before first \n */
    int nl = 0;
    while ((nl = func.find('\n', nl)) >= 0) {
	// search back to the beginning of the whitespace
	int startWhite = nl;
	do {
	    --startWhite;
	} while (ISSPACE(func[startWhite]));
	startWhite++;
	// search forward to the end of the whitespace
	do {
	    nl++;
	} while (ISSPACE(func[nl]));
	// replace
	func.replace(startWhite, nl-startWhite, " ");
	/* continue searching for more \n's at this place: */
	nl = startWhite+1;
    }
    return true;
}

#undef ISSPACE

void GdbDriver::parseBackTrace(const char* output, QList<StackFrame>& stack)
{
    QString func, file;
    int lineNo, frameNo;

    while (::parseFrame(output, frameNo, func, file, lineNo)) {
	StackFrame* frm = new StackFrame;
	frm->frameNo = frameNo;
	frm->fileName = file;
	frm->lineNo = lineNo;
	frm->var = new VarTree(func, VarTree::NKplain);
	stack.append(frm);
    }
}

bool GdbDriver::parseFrameChange(const char* output,
				 int& frameNo, QString& file, int& lineNo)
{
    QString func;
    return ::parseFrame(output, frameNo, func, file, lineNo);
}


bool GdbDriver::parseBreakList(const char* output, QList<Breakpoint>& brks)
{
    // skip first line, which is the headline
    const char* p = strchr(output, '\n');
    if (p == 0)
	return false;
    p++;
    if (*p == '\0')
	return false;

    // split up a line
    QString location;
    QString address;
    int hits = 0;
    uint ignoreCount = 0;
    QString condition;
    const char* end;
    char* dummy;
    while (*p != '\0') {
	// get Num
	long bpNum = strtol(p, &dummy, 10);	/* don't care about overflows */
	p = dummy;
	// skip Type
	while (isspace(*p))
	    p++;
	while (*p != '\0' && !isspace(*p))	/* "breakpoint" */
	    p++;
	while (isspace(*p))
	    p++;
	if (*p == '\0')
	    break;
	// get Disp
	char disp = *p++;
	while (*p != '\0' && !isspace(*p))	/* "keep" or "del" */
	    p++;
	while (isspace(*p))
	    p++;
	if (*p == '\0')
	    break;
	// get Enb
	char enable = *p++;
	while (*p != '\0' && !isspace(*p))	/* "y" or "n" */
	    p++;
	while (isspace(*p))
	    p++;
	if (*p == '\0')
	    break;
	// the address, if present
	if (strncmp(p, "0x", 2) == 0) {
	    const char* start = p;
	    while (*p != '\0' && !isspace(*p))
		p++;
	    address = FROM_LATIN1(start, p-start);
	    while (isspace(*p) && *p != '\n')
		p++;
	    if (*p == '\0')
		break;
	} else {
	    address = QString();
	}
	// remainder is location, hit and ignore count, condition
	hits = 0;
	ignoreCount = 0;
	condition = QString();
	end = strchr(p, '\n');
	if (end == 0) {
	    location = p;
	    p += location.length();
	} else {
	    location = FROM_LATIN1(p, end-p).stripWhiteSpace();
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
		hits = strtol(p, &dummy, 10);
		TRACE(QString().sprintf("hit count %d", hits));
	    } else if (strncmp(p, "stop only if ", 13) == 0) {
		// extract condition
		p += 13;
		condition = FROM_LATIN1(p, end-p).stripWhiteSpace();
		TRACE("condition: "+condition);
	    } else if (strncmp(p, "ignore next ", 12) == 0) {
		// extract ignore count
		p += 12;
		ignoreCount = strtol(p, &dummy, 10);
		TRACE(QString().sprintf("ignore count %d", ignoreCount));
	    } else {
		// indeed a continuation
		location += " " + FROM_LATIN1(p, end-p).stripWhiteSpace();
	    }
	    p = end;
	    if (*p != '\0')
		p++;			/* skip '\n' */
	}
	Breakpoint* bp = new Breakpoint;
	bp->id = bpNum;
	bp->temporary = disp == 'd';
	bp->enabled = enable == 'y';
	bp->location = location;
	bp->address = address;
	bp->hitCount = hits;
	bp->ignoreCount = ignoreCount;
	bp->condition = condition;
	brks.append(bp);
    }
    return true;
}

bool GdbDriver::parseBreakpoint(const char* output, int& id,
				QString& file, int& lineNo)
{
    const char* o = output;
    // skip lines of that begin with "(Cannot find"
    while (strncmp(o, "(Cannot find", 12) == 0) {
	o = strchr(o, '\n');
	if (o == 0)
	    return false;
	o++;				/* skip newline */
    }

    if (strncmp(o, "Breakpoint ", 11) != 0)
	return false;
    
    // breakpoint id
    output += 11;			/* skip "Breakpoint " */
    char* p;
    int num = strtoul(output, &p, 10);
    if (p == o)
	return false;
    
    // file name
    char* fileStart = strstr(p, "file ");
    if (fileStart == 0)
	return false;
    fileStart += 5;
    
    // line number
    char* numStart = strstr(fileStart, ", line ");
    QString fileName = FROM_LATIN1(fileStart, numStart-fileStart);
    numStart += 7;
    int line = strtoul(numStart, &p, 10);
    if (numStart == p)
	return false;

    id = num;
    file = fileName;
    lineNo = line-1;			/* zero-based! */
    return true;
}

void GdbDriver::parseLocals(const char* output, QList<VarTree>& newVars)
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

	VarTree* variable = parseVar(output);
	if (variable == 0) {
	    break;
	}
	// do not add duplicates
	for (VarTree* o = newVars.first(); o != 0; o = newVars.next()) {
	    if (o->getText() == variable->getText()) {
		delete variable;
		goto skipDuplicate;
	    }
	}
	newVars.append(variable);
    skipDuplicate:;
    }
}

bool GdbDriver::parsePrintExpr(const char* output, bool wantErrorValue, VarTree*& var)
{
    // check for error conditions
    if (parseErrorMessage(output, var, wantErrorValue))
    {
	return false;
    } else {
	// parse the variable
	var = parseVar(output);
	return true;
    }
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
     * The command is successful if there is no output or the single
     * message (no debugging symbols found)...
     */
    return
	output[0] == '\0' ||
	strcmp(output, "(no debugging symbols found)...") == 0;
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
	    message = FROM_LATIN1(start, endOfMessage-start);
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

    return flags;
}

void GdbDriver::parseSharedLibs(const char* output, QStrList& shlibs)
{
    if (strncmp(output, "No shared libraries loaded", 26) == 0)
	return;

    // parse the table of shared libraries

    // strip off head line
    output = strchr(output, '\n');
    if (output == 0)
	return;
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
	    return;
	const char* start = output;
	output = strchr(output, '\n');
	if (output == 0)
	    output = start + strlen(start);
	shlibName = FROM_LATIN1(start, output-start);
	if (*output != '\0')
	    output++;
	shlibs.append(shlibName);
	TRACE("found shared lib " + shlibName);
    }
}

bool GdbDriver::parseFindType(const char* output, QString& type)
{
    if (strncmp(output, "type = ", 7) != 0)
	return false;

    /*
     * Everything else is the type. We strip off all white-space from the
     * type.
     */
    output += 7;
    type = output;
    type.replace(QRegExp("\\s+"), "");
    return true;
}

void GdbDriver::parseRegisters(const char* output, QList<RegisterInfo>& regs)
{
    if (strncmp(output, "The program has no registers now", 32) == 0) {
	return;
    }

    QString regName;
    QString value;

    // parse register values
    while (*output != '\0')
    {
	// skip space at the start of the line
	while (isspace(*output))
	    output++;

	// register name
	const char* start = output;
	while (*output != '\0' && !isspace(*output))
	    output++;
	if (*output == '\0')
	    break;
	regName = FROM_LATIN1(start, output-start);

	// skip space
	while (isspace(*output))
	    output++;
	// the rest of the line is the register value
	start = output;
	output = strchr(output,'\n');
	if (output == 0)
	    output = start + strlen(start);
	value = FROM_LATIN1(start, output-start).simplifyWhiteSpace();

	if (*output != '\0')
	    output++;			/* skip '\n' */

	RegisterInfo* reg = new RegisterInfo;
	reg->regName = regName;

	/*
	 * We split the raw from the cooked values. For this purpose, we
	 * split off the first token (separated by whitespace). It is the
	 * raw value. The remainder of the line is the cooked value.
	 */
	int pos = value.find(' ');
	if (pos < 0) {
	    reg->rawValue = value;
	    reg->cookedValue = QString();
	} else {
	    reg->rawValue = value.left(pos);
	    reg->cookedValue = value.mid(pos+1,value.length());
	    /*
	     * Some modern gdbs explicitly say: "0.1234 (raw 0x3e4567...)".
	     * Here the raw value is actually in the second part.
	     */
	    if (reg->cookedValue.left(5) == "(raw ") {
		QString raw = reg->cookedValue.right(reg->cookedValue.length()-5);
		if (raw[raw.length()-1] == ')')	/* remove closing bracket */
		    raw = raw.left(raw.length()-1);
		reg->cookedValue = reg->rawValue;
		reg->rawValue = raw;
	    }
	}

	regs.append(reg);
    }
}

bool GdbDriver::parseInfoLine(const char* output, QString& addrFrom, QString& addrTo)
{
    const char* start = strstr(output, "starts at address ");
    if (start == 0)
	return false;

    start += 18;
    const char* p = start;
    while (*p != '\0' && !isspace(*p))
	p++;
    addrFrom = FROM_LATIN1(start, p-start);

    start = strstr(p, "and ends at ");
    if (start == 0)
	return false;

    start += 12;
    p = start;
    while (*p != '\0' && !isspace(*p))
	p++;
    addrTo = FROM_LATIN1(start, p-start);

    return true;
}

void GdbDriver::parseDisassemble(const char* output, QList<DisassembledCode>& code)
{
    code.clear();

    if (strncmp(output, "Dump of assembler", 17) != 0) {
	// error message?
	DisassembledCode c;
	c.code = output;
	code.append(new DisassembledCode(c));
	return;
    }

    // remove first line
    const char* p = strchr(output, '\n');
    if (p == 0)
	return;				/* not a regular output */

    p++;

    // remove last line
    const char* end = strstr(output, "\nEnd of assembler");
    if (end == 0)
	end = p + strlen(p);

    QString address;

    // remove function offsets from the lines
    while (p != end)
    {
	const char* start = p;
	// address
	while (p != end && !isspace(*p))
	    p++;
	address = FROM_LATIN1(start, p-start);

	// function name (enclosed in '<>', followed by ':')
	while (p != end && *p != '<')
	    p++;
	if (*p == '<')
	    skipNested(p, '<', '>');
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

	DisassembledCode* c = new DisassembledCode;
	c->address = address;
	c->code = FROM_LATIN1(start, p-start);
	code.append(c);
    }
}


#include "gdbdriver.moc"
