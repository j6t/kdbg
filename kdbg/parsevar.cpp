// $Id$

// Copyright by Johannes Sixt
// This file is under GPL, the GNU General Public Licence

#include "parsevar.h"
#include "exprwnd.h"
#include <ctype.h>
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include "mydebug.h"

bool parseName(const char*& s, QString& name, VarTree::NameKind& kind);
bool parseValue(const char*& s, VarTree* variable);
bool parseNested(const char*& s, VarTree* variable);
bool parseVarSeq(const char*& s, VarTree* variable);
bool parseValueSeq(const char*& s, VarTree* variable);

VarTree* parseVar(const char*& s) 
{
    const char* p = s;
    
    // syntax is complicated

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

void skipNested(const char*& s, char opening, char closing)
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

void skipNestedWithString(const char*& s, char opening, char closing)
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

bool parseName(const char*& s, QString& name, VarTree::NameKind& kind)
{
    kind = VarTree::NKplain;

    const char* p = s;
    // examples of names:
    //  name
    //  <Object>
    //  <string<a,b<c>,7> >

    if (*p == '<') {
	skipNested(p, '<', '>');
	name = QString(s, (p - s)+1);
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
	name = QString(s, len+1);
    }
    // return the new position
    s = p;
    return true;
}

bool parseValue(const char*& s, VarTree* variable)
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
	    variable->m_value = QString(s, (p - s)+1);
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
	variable->m_value += QString(start, (p - start)+1);

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
		variable->m_value += QString(start-1, (p - start)+2);
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

bool parseNested(const char*& s, VarTree* variable)
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

bool parseVarSeq(const char*& s, VarTree* variable)
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

bool parseValueSeq(const char*& s, VarTree* variable)
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
