// $Id$

// Copyright by Johannes Sixt
// This file is under GPL, the GNU General Public Licence

#include <qdict.h>
#include <qstring.h>

class KConfigBase;

/**
 * The maximum number of sub-expressions that may appear in a single struct
 * value.
 */
const int typeInfoMaxExpr = 5;

struct TypeInfo
{
    TypeInfo(const QString& displayString);
    ~TypeInfo();

    /**
     * The number of sub-expressions that need to be evaluated to get the
     * struct value.
     */
    int m_numExprs;
    /**
     * This array contains the various parts which glue together the
     * sub-expressions. The entries in this array are the parts that appear
     * between the percent signs '%' of the display expression; hence,
     * there is one part more than there are sub-expressions.
     */
    QString m_displayString[typeInfoMaxExpr+1];
    /**
     * This is a list of partial expressions. Each contains exactly one %s,
     * which will be replaced by the parent expression. The results are
     * substituted for the percent signs in m_displayString.
     */
    QString m_exprStrings[typeInfoMaxExpr];
};

class TypeTable
{
public:
    TypeTable();
    ~TypeTable();

    /**
     * Loads the structure type information from the configuration files.
     */
    void loadTable();

    /**
     * Lookup a structure type.
     */
    TypeInfo* operator[](const char* type);

    /**
     * Adds a new alias for a type name.
     */
    void registerAlias(const char* type, TypeInfo* typeInfo);

    /**
     * Gets a pointer to a TypeInfo that means: "I don't know the type"
     */
    static TypeInfo* unknownType() { return &m_unknownType; }

protected:
    void loadOneFile(const char* fileName);
    void readType(KConfigBase& cf, const char* type);
    QDict<TypeInfo> m_typeDict;
    QDict<TypeInfo> m_aliasDict;

    static TypeInfo m_unknownType;
};


// the one and only TypeTable
extern TypeTable* theTypeTable;
inline TypeTable& typeTable() { return *theTypeTable; }
