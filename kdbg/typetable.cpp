// $Id$

// Copyright by Johannes Sixt
// This file is under GPL, the GNU General Public Licence

#include <qdir.h>
#include <qlist.h>
#include <kapp.h>
#include <ksimpleconfig.h>
#include "typetable.h"
#include "mydebug.h"

// the one and only
TypeTable* theTypeTable = 0;

// the unknown type
TypeInfo TypeTable::m_unknownType("");


TypeTable::TypeTable()
{
    m_typeDict.setAutoDelete(true);
    // aliasDict keeps only pointers to items into typeDict
    m_aliasDict.setAutoDelete(false);
}

TypeTable::~TypeTable()
{
}

void TypeTable::loadTable()
{
    QDir dir(KApplication::kde_datadir() + "/kdbg/types");
    const QStrList* files = dir.entryList("*.kdbgtt");
    if (files == 0) {
	TRACE("no type tables found");
	return;
    }

    QString fileName;
    for (QListIterator<char> it(*files); it != 0; ++it) {
	fileName = dir.filePath(it);
	loadOneFile(fileName);
    }
}

static const char TypeTableGroup[] = "Type Table";
static const char TypesEntryFmt[] = "Types%d";
static const char DisplayEntry[] = "Display";
static const char AliasEntry[] = "Alias";
static const char ExprEntryFmt[] = "Expr%d";


void TypeTable::loadOneFile(const char* fileName)
{
    TRACE(QString("reading file ") + fileName);
    KSimpleConfig cf(fileName, true);	/* read-only */

    /*
     * Get the types. We search for entries of kind Types1, Types2, etc.
     * because a single entry Types could get rather long for large
     * libraries.
     */
    QStrList typeNames;
    QString typesEntry(sizeof(TypesEntryFmt)+20);
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
	    cf.setGroup(it);
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
    QString exprEntry(sizeof(ExprEntryFmt)+20);
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

TypeInfo* TypeTable::operator[](const char* type)
{
    TypeInfo* result = m_typeDict[type];
    if (result == 0) {
	result = m_aliasDict[type];
    }
    return result;
}

void TypeTable::registerAlias(const char* type, TypeInfo* info)
{
    ASSERT((*this)[type] == 0 || (*this)[type] == info);
    m_aliasDict.insert(type, info);
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
