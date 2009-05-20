/*
 * Copyright Johannes Sixt
 * This file is licensed under the GNU General Public License Version 2.
 * See the file COPYING in the toplevel directory of the source directory.
 */

#include <qdir.h>
#include <qptrlist.h>
#include <kglobal.h>
#include <kstandarddirs.h>
#include <ksimpleconfig.h>
#include <list>
#include <algorithm>
#include <iterator>
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include "typetable.h"
#include "mydebug.h"

//! the TypeTables of all known libraries
static std::list<TypeTable> typeTables;
bool typeTablesInited = false;


//! an indentifier for wchar_t
TypeInfo TypeInfo::m_wchartType("");
//! the unknown type
TypeInfo TypeInfo::m_unknownType("");


void TypeTable::initTypeLibraries()
{
    if (!typeTablesInited) {
	TypeTable::loadTypeTables();
    }
}

void TypeTable::loadTypeTables()
{
    typeTablesInited = true;

    const QStringList files = KGlobal::dirs()->findAllResources("types", "*.kdbgtt",
			false, true);
    
    if (files.isEmpty()) {
	TRACE("no type tables found");
	return;
    }

    for (QValueListConstIterator<QString> p = files.begin(); p != files.end(); ++p) {
	typeTables.push_back(TypeTable());
	typeTables.back().loadFromFile(*p);
    }
}


TypeTable::TypeTable() :
	m_printQStringDataCmd(0)
{
    m_typeDict.setAutoDelete(true);
    // aliasDict keeps only pointers to items into typeDict
    m_aliasDict.setAutoDelete(false);
}

TypeTable::~TypeTable()
{
    delete[] m_printQStringDataCmd;
    while (!m_templates.empty()) {
	delete m_templates.begin()->second;
	m_templates.erase(m_templates.begin());
    }
}


static const char TypeTableGroup[] = "Type Table";
static const char LibDisplayName[] = "LibDisplayName";
static const char ShlibRE[] = "ShlibRE";
static const char EnableBuiltin[] = "EnableBuiltin";
static const char PrintQStringCmd[] = "PrintQStringCmd";
static const char TypesEntryFmt[] = "Types%d";
static const char DisplayEntry[] = "Display";
static const char AliasEntry[] = "Alias";
static const char TemplateEntry[] = "Template";
static const char ExprEntryFmt[] = "Expr%d";
static const char FunctionGuardEntryFmt[] = "FunctionGuard%d";


void TypeTable::loadFromFile(const QString& fileName)
{
    TRACE("reading file " + fileName);
    KSimpleConfig cf(fileName, true);	/* read-only */

    /*
     * Read library name and properties.
     */
    cf.setGroup(TypeTableGroup);
    m_displayName = cf.readEntry(LibDisplayName);
    if (m_displayName.isEmpty()) {
	// use file name instead
	QFileInfo fi(fileName);
	m_displayName = fi.baseName(true);
    }

    m_shlibNameRE = QRegExp(cf.readEntry(ShlibRE));
    m_enabledBuiltins = cf.readListEntry(EnableBuiltin);

    QString printQString = cf.readEntry(PrintQStringCmd);
    const char* ascii = printQString.ascii();
    if (ascii == 0)
	ascii = "";
    m_printQStringDataCmd = new char[strlen(ascii)+1];
    strcpy(m_printQStringDataCmd, ascii);

    /*
     * Get the types. We search for entries of kind Types1, Types2, etc.
     * because a single entry Types could get rather long for large
     * libraries.
     */
    QString typesEntry;
    for (int i = 1; ; i++) {
	// next bunch of types
	cf.setGroup(TypeTableGroup);
	typesEntry.sprintf(TypesEntryFmt, i);
	if (!cf.hasKey(typesEntry))
	    break;

	QStringList typeNames = cf.readListEntry(typesEntry, ',');

	// now read them
	QString alias;
	for (QStringList::iterator it = typeNames.begin(); it != typeNames.end(); ++it)
	{
	    cf.setGroup(*it);
	    // check if this is an alias
	    alias = cf.readEntry(AliasEntry);
	    if (alias.isEmpty()) {
		readType(cf, *it);
	    } else {
		// look up the alias type and insert it
		TypeInfo* info = m_typeDict[alias];
		if (info == 0) {
		    TRACE(*it + ": alias " + alias + " not found");
		} else {
		    m_aliasDict.insert(alias, info);
		    TRACE(*it + ": alias " + alias);
		}
	    }
	}
    } // for all Types%d
}

void TypeTable::readType(KConfigBase& cf, const QString& type)
{
    // the display string
    QString expr = cf.readEntry(DisplayEntry);

    TypeInfo* info = new TypeInfo(expr);
    if (info->m_numExprs == 0) {
	TRACE("bogus type " + type + ": no %% in Display: " + expr);
	delete info;
	return;
    }

    info->m_templatePattern = cf.readEntry(TemplateEntry);

    // Expr1, Expr2, etc...
    QString exprEntry;
    QString funcGuardEntry;
    for (int j = 0; j < info->m_numExprs; j++) {
	exprEntry.sprintf(ExprEntryFmt, j+1);
	expr = cf.readEntry(exprEntry);
	info->m_exprStrings[j] = expr;

	funcGuardEntry.sprintf(FunctionGuardEntryFmt, j+1);
	expr = cf.readEntry(funcGuardEntry);
	info->m_guardStrings[j] = expr;
    }

    // add the new type
    if (info->m_templatePattern.find('<') < 0)
	m_typeDict.insert(type, info);
    else
	m_templates[type] = info;
    TRACE(type + QString().sprintf(": %d exprs", info->m_numExprs));
}

void TypeTable::copyTypes(QDict<TypeInfo>& dict)
{
    for (QDictIterator<TypeInfo> it = m_typeDict; it != 0; ++it) {
	dict.insert(it.currentKey(), it);
    }
    for (QDictIterator<TypeInfo> it = m_aliasDict; it != 0; ++it) {
	dict.insert(it.currentKey(), it);
    }
}

bool TypeTable::isEnabledBuiltin(const QString& feature) const
{
    return m_enabledBuiltins.find(feature) != m_enabledBuiltins.end();
}

TypeInfo::TypeInfo(const QString& displayString)
{
    // decompose the input into the parts
    int i = 0;
    int startIdx = 0;
    int idx;
    while (i < typeInfoMaxExpr &&
	   (idx = displayString.find('%', startIdx)) >= 0)
    {
	m_displayString[i] = displayString.mid(startIdx, idx-startIdx);
	startIdx = idx+1;
	i++;
    }
    m_numExprs = i;
    /*
     * Remaining string; note that there's one more display string than
     * sub-expressions.
     */
    m_displayString[i] = displayString.right(displayString.length()-startIdx);
}

TypeInfo::~TypeInfo()
{
}


ProgramTypeTable::ProgramTypeTable() :
	m_parseQt2QStrings(false),
	m_QCharIsShort(false),
	m_printQStringDataCmd(0)
{
    m_types.setAutoDelete(false);	/* paranoia */
    m_aliasDict.setAutoDelete(false);	/* paranoia */
}

ProgramTypeTable::~ProgramTypeTable()
{
}

void ProgramTypeTable::loadTypeTable(TypeTable* table)
{
    table->copyTypes(m_types);

    // add templates
    const TypeTable::TypeMap& t = table->templates();
    std::transform(t.begin(), t.end(),
		std::inserter(m_templates, m_templates.begin()),
		std::ptr_fun(template2Info));

    // check whether to enable builtin QString support
    if (!m_parseQt2QStrings) {
	m_parseQt2QStrings = table->isEnabledBuiltin("QString::Data");
    }
    if (!m_QCharIsShort) {
	m_QCharIsShort = table->isEnabledBuiltin("QCharIsShort");
    }
    if (!m_printQStringDataCmd && *table->printQStringDataCmd()) {
	m_printQStringDataCmd = table->printQStringDataCmd();
    }
}

ProgramTypeTable::TemplateMap::value_type
	ProgramTypeTable::template2Info(const TypeTable::TypeMap::value_type& tt)
{
    QStringList args = splitTemplateArgs(tt.second->m_templatePattern);

    TemplateMap::value_type result(args.front(), TemplateInfo());
    result.second.type = tt.second;
    args.pop_front();
    result.second.templateArgs = args;
    return result;
}

/**
 * Splits the name \a t into the template name and its arguments.
 * The first entry of the returned list is the template name, the remaining
 * entries are the arguments.
 */
QStringList ProgramTypeTable::splitTemplateArgs(const QString& t)
{
    QStringList result;
    result.push_back(t);

    int i = t.find('<');
    if (i < 0)
	return result;

    // split off the template name
    result.front().truncate(i);

    i++;	// skip '<'
    // look for the next comma or the closing '>', skipping nested '<>'
    int nest = 0;
    int start = i;
    for (; unsigned(i) < t.length() && nest >= 0; i++)
    {
	if (t[i] == '<')
	    nest++;
	else if (t[i] == '>')
	    nest--;
	else if (nest == 0 && t[i] == ',') {
	    // found end of argument
	    QString arg = t.mid(start, i-start);
	    result.push_back(arg);
	    start = i+1;	// skip ','
	}
    }
    // accept the template only if the closing '>' is the last character
    if (nest < 0 && unsigned(i) == t.length()) {
	QString arg = t.mid(start, i-start-1);
	result.push_back(arg);
    } else {
	result.clear();
	result.push_back(t);
    }
    return result;
}

TypeInfo* ProgramTypeTable::lookup(QString type)
{
    /*
     * Registered aliases contain the complete template parameter list.
     * Check for an alias first so that this case is out of the way.
     */
    if (TypeInfo* result = m_aliasDict[type])
	return result;

    /*
     * Check for a normal type. Even if type is a template instance,
     * it could have been registered as a normal type instead of a pattern.
     */
    if (TypeInfo* result = m_types[type])
	return result;

    /*
     * The hard part: Look up a template.
     */
    QStringList parts = splitTemplateArgs(type);
    if (parts.size() == 1)
	return 0;	// not a template

    // We can have several patterns for the same template name.
    std::pair<TemplateMap::const_iterator, TemplateMap::const_iterator> range =
	m_templates.equal_range(parts.front());
    // We pick the one that has the wildcards in the later parameters.
    unsigned minPenalty = ~0U;
    TypeInfo* result = 0;
    parts.pop_front();

    for (TemplateMap::const_iterator i = range.first; i != range.second; ++i)
    {
	const QStringList& pat = i->second.templateArgs;
	if (parts.size() < pat.size())
	    continue;	// too few arguments

	// a "*" in the last position of the pattern matches all arguments
	// at the end of the template's arguments
	if (parts.size() > pat.size() && pat.back() != "*")
	    continue;	// too many arguments and no wildcard

	QStringList::const_iterator t = parts.begin();
	QStringList::const_iterator p = pat.begin();
	unsigned accumPenalty = 0;
	bool equal = true;
	unsigned penalty = ~(~0U>>1);	// 1 in the leading bit
	while (equal && p != pat.end())
	{
	    if (*p == "*")
		accumPenalty |= penalty;	// penalize wildcards
	    else
	    	equal = *p == *t;
	    ++p, ++t, penalty >>= 1;
	}
	if (equal)
	{
	    if (accumPenalty == 0)
		return i->second.type;
	    if (accumPenalty < minPenalty) {
		result = i->second.type;
		minPenalty = accumPenalty;
	    }
	}
    }
    return result;
}

void ProgramTypeTable::registerAlias(const QString& name, TypeInfo* type)
{
    ASSERT(lookup(name) == 0 || lookup(name) == type);
    m_aliasDict.insert(name, type);
}

void ProgramTypeTable::loadLibTypes(const QStringList& libs)
{
    for (QStringList::const_iterator it = libs.begin(); it != libs.end(); ++it)
    {
	// look up the library
	for (std::list<TypeTable>::iterator t = typeTables.begin(); t != typeTables.end(); ++t)
	{
	    if (t->matchFileName(*it))
	    {
		TRACE("adding types for " + *it);
		loadTypeTable(&*t);
	    }
	}
    }
}
