// $Id$

// Copyright by Johannes Sixt
// This file is under GPL, the GNU General Public Licence

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
}

TypeTable::~TypeTable()
{
}

void TypeTable::loadTable()
{
    QString fileName = KApplication::kde_configdir() + "/kdbgrc";
    TRACE(fileName);
    loadOneFile(fileName);
}

void TypeTable::loadOneFile(const char* fileName)
{
    KSimpleConfig cf(fileName, true);

    /*
     * Get the types. Here we assume that the names pointers to the keys
     * don't change while we iterate over them.
     */
    KGroupIterator* it = cf.groupIterator();
    QArray<const char*> typeNames;
    for (; it->current(); ++(*it)) {
	if (qstrcmp(it->currentKey(), "<default>") == 0)
	    continue;
	int n = typeNames.size();
	typeNames.resize(n+1);
	typeNames[n] = it->currentKey();
    }
    delete it;

    // now read them all
    QString exprEntry;
    QString expr;
    for (int i = typeNames.size()-1; i >= 0; i--) {
	cf.setGroup(typeNames[i]);
	// the display string
	expr = cf.readEntry("Display");

	TypeInfo* info = new TypeInfo(expr);
	if (info->m_numExprs == 0) {
	    TRACE(QString().sprintf("bogus type %s: no %% in Display: '%s'",
				    typeNames[i], expr.data()));
	    delete info;
	    continue;
	}

	// Expr1, Expr2, etc...
	for (int j = 0; j < info->m_numExprs; j++) {
	    exprEntry.sprintf("Expr%d", j+1);
	    expr = cf.readEntry(exprEntry);
	    info->m_exprStrings[j] = expr;
	}

	// add the new type
	m_typeDict.insert(typeNames[i], info);
	TRACE(QString().sprintf("<%s>: %s%%%s by %s", typeNames[i],
				info->m_displayString[0].data(),
				info->m_displayString[1].data(),
				info->m_exprStrings[0].data()));
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
