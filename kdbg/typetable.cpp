// $Id$

// Copyright by Johannes Sixt
// This file is under GPL, the GNU General Public Licence

#include <qdir.h>
#include <qlist.h>
#include <kapp.h>
#include <ksimpleconfig.h>
#include "typetable.h"
#include "mydebug.h"

// the TypeTables of all known libraries
static QList<TypeTable> typeTables;
bool typeTablesInited = false;


// the unknown type
TypeInfo TypeTable::m_unknownType("");


void TypeTable::initTypeLibraries()
{
    if (!typeTablesInited) {
	TypeTable::loadTypeTables();
    }
}

void TypeTable::loadTypeTables()
{
    typeTablesInited = true;

    QDir dir(KApplication::kde_datadir() + "/kdbg/types");
    const QStrList* files = dir.entryList("*.kdbgtt");    
    if (files == 0) {
	TRACE("no type tables found");
	return;
    }

    QString fileName;
    for (QListIterator<char> it(*files); it != 0; ++it) {
	fileName = dir.filePath(it.current());
	TypeTable* newTable = new TypeTable;
	newTable->loadFromFile(fileName);
	typeTables.append(newTable);
    }
}


TypeTable::TypeTable()
{
    m_typeDict.setAutoDelete(true);
    // aliasDict keeps only pointers to items into typeDict
    m_aliasDict.setAutoDelete(false);
}

TypeTable::~TypeTable()
{
}


static const char TypeTableGroup[] = "Type Table";
static const char LibDisplayName[] = "LibDisplayName";
static const char ShlibName[] = "ShlibName";
static const char TypesEntryFmt[] = "Types%d";
static const char DisplayEntry[] = "Display";
static const char AliasEntry[] = "Alias";
static const char ExprEntryFmt[] = "Expr%d";


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
	int slash = fileName.findRev('\\');
	m_displayName =
	    slash >= 0 ? fileName.mid(slash+1, fileName.length()) : fileName;
	int dot = m_displayName.findRev('.');
	if (dot > 0) {
	    m_displayName.truncate(dot);
	}
    }

    m_shlibName = cf.readEntry(ShlibName);

    /*
     * Get the types. We search for entries of kind Types1, Types2, etc.
     * because a single entry Types could get rather long for large
     * libraries.
     */
    QStrList typeNames;
    QString typesEntry;
    for (int i = 1; ; i++) {
	// next bunch of types
	cf.setGroup(TypeTableGroup);
	typesEntry.sprintf(TypesEntryFmt, i);
	if (!cf.hasKey(typesEntry))
	    break;
	cf.readListEntry(typesEntry, typeNames, ',');

	// now read them
	QString alias;
	for (QListIterator<char> it(typeNames); it != 0; ++it) {
	    cf.setGroup(it.current());
	    // check if this is an alias
	    alias = cf.readEntry(AliasEntry);
	    if (alias.isEmpty()) {
		readType(cf, it);
	    } else {
		// look up the alias type and insert it
		TypeInfo* info = m_typeDict[alias];
		if (info == 0) {
		    TRACE(QString().sprintf("<%s>: alias %s not found",
					    it.operator char*(), alias.data()));
		} else {
		    m_aliasDict.insert(alias, info);
		    TRACE(QString().sprintf("<%s>: alias <%s>",
					    it.operator char*(), alias.data()));
		}
	    }
	}
    } // for all Types%d
}

void TypeTable::readType(KConfigBase& cf, const char* type)
{
    // the display string
    QString expr = cf.readEntry(DisplayEntry);

    TypeInfo* info = new TypeInfo(expr);
    if (info->m_numExprs == 0) {
	TRACE(QString().sprintf("bogus type %s: no %% in Display: '%s'",
				type, expr.data()));
	delete info;
	return;
    }

    // Expr1, Expr2, etc...
    QString exprEntry;
    for (int j = 0; j < info->m_numExprs; j++) {
	exprEntry.sprintf(ExprEntryFmt, j+1);
	expr = cf.readEntry(exprEntry);
	info->m_exprStrings[j] = expr;
    }

    // add the new type
    m_typeDict.insert(type, info);
    TRACE(QString().sprintf("<%s>: %d exprs", type,
			    info->m_numExprs));
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


ProgramTypeTable::ProgramTypeTable()
{
    m_types.setAutoDelete(false);	/* paranoia */
    m_aliasDict.setAutoDelete(false);	/* paranoia */

    // load all types
    for (QListIterator<TypeTable> it = typeTables; it; ++it) {
	loadTypeTable(it);
    }
}

ProgramTypeTable::~ProgramTypeTable()
{
}

void ProgramTypeTable::clear()
{
    m_types.clear();
}

void ProgramTypeTable::loadTypeTable(TypeTable* table)
{
    table->copyTypes(m_types);
}

TypeInfo* ProgramTypeTable::lookup(const char* type)
{
    TypeInfo* result = m_types[type];
    if (result == 0) {
	result = m_aliasDict[type];
    }
    return result;
}

void ProgramTypeTable::registerAlias(const QString& name, TypeInfo* type)
{
    ASSERT(lookup(name) == 0 || lookup(name) == type);
    m_aliasDict.insert(name, type);
}
