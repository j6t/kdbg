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
     * Load all known type libraries.
     */
    static void initTypeLibraries();

    /**
     * Copy type infos to the specified dictionary.
     */
    void copyTypes(QDict<TypeInfo>& dict);

    /**
     * Gets a pointer to a TypeInfo that means: "I don't know the type"
     */
    static TypeInfo* unknownType() { return &m_unknownType; }

protected:
    /**
     * Loads the structure type information from the configuration files.
     */
    static void loadTypeTables();
    void loadFromFile(const QString& fileName);
    void readType(KConfigBase& cf, const char* type);
    QDict<TypeInfo> m_typeDict;
    QDict<TypeInfo> m_aliasDict;
    QString m_displayName;
    QString m_shlibName;

    static TypeInfo m_unknownType;
};


/**
 * This table keeps only references to the global type table. It is set up
 * once per program.
 */
class ProgramTypeTable
{
public:
    ProgramTypeTable();
    ~ProgramTypeTable();

    /**
     * Load types belonging to the specified libraries.
     */
    void loadLibTypes(const QArray<char>& libs);

    /**
     * Load types belonging to the specified type table
     */
    void loadTypeTable(TypeTable* table);

    /**
     * Clears that types and starts over (e.g. for a new program).
     */
    void clear();

    /**
     * Lookup a structure type.
     * 
     * A type is looked up in the following manner:
     * 
     * - If the type is unknown, 0 is returned.
     * 
     * - If the type is known and it belongs to a shared library and that
     * shared library was loaded, the type is returned such that isNew()
     * returns true.
     * 
     * - Otherwise the type is returned such that isNew() returns true.
     */
    TypeInfo* lookup(const char* type);

    /**
     * Adds a new alias for a type name.
     */
    void registerAlias(const QString& name, TypeInfo* type);

protected:
    QDict<TypeInfo> m_types;
    QDict<TypeInfo> m_aliasDict;
};
