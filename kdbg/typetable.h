/*
 * Copyright Johannes Sixt
 * This file is licensed under the GNU General Public License Version 2.
 * See the file COPYING in the toplevel directory of the source directory.
 */

#include <QString>
#include <QRegExp>
#include <QStringList>
#include <map>

class KConfigGroup;

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
     * This is a list of partial expressions. Each contains one or more \%s,
     * which will be replaced by the parent expression. The results are
     * substituted for the percent signs in m_displayString.
     */
    QString m_exprStrings[typeInfoMaxExpr];
    /**
     * This is a list of guard expressions. Each contains one or more \%s,
     * which will be replaced by the parent expression, or is empty. If the
     * evaluation of the resulting expression returns an error, the
     * corresponding expression from m_exprStrings is not evaluated. (This
     * is used to guard function calls.)
     */
    QString m_guardStrings[typeInfoMaxExpr];
    /**
     * This is the type name including template arguments that contain a
     * pattern: A single '*' as template parameter matches one template
     * argument, except that a '*' as the last template parameter matches
     * all remaining template argument.
     */
    QString m_templatePattern;
    /**
     * Returns a pointer to a TypeInfo that identifies wchar_t
     */
    static TypeInfo* wchartType() { return &m_wchartType; }
    /**
     * Gets a pointer to a TypeInfo that means: "I don't know the type"
     */
    static TypeInfo* unknownType() { return &m_unknownType; }

protected:
    static TypeInfo m_wchartType;
    static TypeInfo m_unknownType;
};

class TypeTable
{
public:
    TypeTable();
    ~TypeTable();

    typedef std::map<QString,TypeInfo> TypeInfoMap;
    typedef std::map<QString,const TypeInfo*> TypeInfoRefMap;

    /**
     * Load all known type libraries.
     */
    static void initTypeLibraries();

    /**
     * Copy type infos to the specified dictionary.
     */
    void copyTypes(TypeInfoRefMap& dict);

    /**
     * Returns the template types
     */
    const TypeInfoMap& templates() const { return m_templates; }

    /**
     * Does the file name match this library?
     */
    bool matchFileName(const QString& fileName) const {
	return m_shlibNameRE.indexIn(fileName) >= 0;
    }

    /**
     * Is the specified builtin feature enabled in this type library?
     */
    bool isEnabledBuiltin(const QString& feature) const;

    /**
     * Returns the command to print the QString data.
     */
    const char* printQStringDataCmd() const { return m_printQStringDataCmd.constData(); }
    
protected:
    /**
     * Loads the structure type information from the configuration files.
     */
    static void loadTypeTables();
    void loadFromFile(const QString& fileName);
    void readType(const KConfigGroup& cf, const QString& type);
    TypeInfoMap m_typeDict;
    TypeInfoRefMap m_aliasDict;
    TypeInfoMap m_templates;
    QString m_displayName;
    QRegExp m_shlibNameRE;
    QStringList m_enabledBuiltins;
    QByteArray m_printQStringDataCmd;
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
    void loadLibTypes(const QStringList& libs);

    /**
     * Load types belonging to the specified type table
     */
    void loadTypeTable(TypeTable* table);

    /**
     * Lookup a structure type.
     * 
     * If the type is unknown, 0 is returned.
     */
    const TypeInfo* lookup(QString type);

    /**
     * Adds a new alias for a type name.
     */
    void registerAlias(const QString& name, const TypeInfo* type);

    /**
     * Tells whether we use built-in support to understand QStrings.
     */
    bool parseQt2QStrings() const { return m_parseQt2QStrings; }

    /**
     * Tells whether QChar are defined like in Qt3.
     */
    bool qCharIsShort() const { return m_QCharIsShort; }

    /**
     * Returns the command to print the QString data.
     */
    const char* printQStringDataCmd() const { return m_printQStringDataCmd; }

protected:
    TypeTable::TypeInfoRefMap m_types;
    TypeTable::TypeInfoRefMap m_aliasDict;
    struct TemplateInfo {
	QStringList templateArgs;
	const TypeInfo* type;
    };
    typedef std::multimap<QString, TemplateInfo> TemplateMap;
    TemplateMap m_templates;	//!< one or more template patterns per template name
    static TemplateMap::value_type
		template2Info(const TypeTable::TypeInfoMap::value_type& tt);
    static QStringList splitTemplateArgs(const QString& t);
    bool m_parseQt2QStrings;
    bool m_QCharIsShort;
    QByteArray m_printQStringDataCmd;
};
