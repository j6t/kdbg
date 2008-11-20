/*
 * Copyright Johannes Sixt
 * This file is licensed under the GNU General Public License Version 2.
 * See the file COPYING in the toplevel directory of the source directory.
 */

#include <qdir.h>
#include <qptrlist.h>
#include <kglobal.h>
#include <kstddirs.h>
#include <ksimpleconfig.h>
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include "typetable.h"
#include "mydebug.h"

//! the TypeTables of all known libraries
static QList<TypeTable> typeTables;
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

    QString fileName;
    for (QValueListConstIterator<QString> p = files.begin(); p != files.end(); ++p) {
	fileName = *p;
	TypeTable* newTable = new TypeTable;
	newTable->loadFromFile(fileName);
	typeTables.append(newTable);
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
}


static const char TypeTableGroup[] = "Type Table";
static const char LibDisplayName[] = "LibDisplayName";
static const char ShlibRE[] = "ShlibRE";
static const char EnableBuiltin[] = "EnableBuiltin";
static const char PrintQStringCmd[] = "PrintQStringCmd";
static const char TypesEntryFmt[] = "Types%d";
static const char DisplayEntry[] = "Display";
static const char AliasEntry[] = "Alias";
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
    m_typeDict.insert(type, info);
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

TypeInfo* ProgramTypeTable::lookup(QString type)
{
    /*
     * Registered aliases contain the complete template parameter list.
     * Check for an alias first so that this case is out of the way.
     */
    if (TypeInfo* result = m_aliasDict[type])
	return result;

    // compress any template types to '<*>'
    int templ = type.find('<');
    if (templ > 0) {
	return m_types[type.left(templ) + "<*>"];
    }
    return m_types[type];
}

void ProgramTypeTable::registerAlias(const QString& name, TypeInfo* type)
{
    ASSERT(lookup(name) == 0 || lookup(name) == type);
    m_aliasDict.insert(name, type);
}

void ProgramTypeTable::loadLibTypes(const QStrList& libs)
{
    QStrListIterator it = libs;

    /*
     * We use a copy of the list of known libraries, from which we delete
     * those libs that we already have added. This way we avoid to load a
     * library twice.
     */
    QList<TypeTable> allTables = typeTables;	/* shallow copy! */
    allTables.setAutoDelete(false);	/* important! */

    for (; it && allTables.count() > 0; ++it)
    {
	// look up the library
    repeatLookup:;
	for (TypeTable* t = allTables.first(); t != 0; t = allTables.next())
	{
	    if (t->matchFileName(it))
	    {
		TRACE("adding types for " + QString(it));
		loadTypeTable(t);
		// remove the table
		allTables.remove();
		/*
		 * continue the search (due to remove's unpredictable
		 * behavior of setting the current item we simply go
		 * through the whole list again)
		 */
		goto repeatLookup;
	    }
	}
    }
}
