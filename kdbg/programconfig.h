/*
 * Copyright Johannes Sixt
 * This file is licensed under the GNU General Public License Version 2.
 * See the file COPYING in the toplevel directory of the source directory.
 */

#include <kconfigbase.h>

class KConfigINIBackEnd;

class ProgramConfig : public KConfigBase
{
public:
    ProgramConfig(const QString &fileName);
    virtual QStringList groupList() const;
    virtual QMap<QString, QString> entryMap(const QString &group) const;
    virtual void reparseConfiguration();
    virtual KEntryMap internalEntryMap( const QString& pGroup ) const;
    virtual KEntryMap internalEntryMap() const;
    virtual void putData(const KEntryKey &_key, const KEntry &_data, bool _checkGroup = true);
    virtual KEntry lookupData(const KEntryKey &_key) const;
    virtual bool internalHasGroup(const QCString &group) const;

protected:
    /**
     * Contains all key,value entries, as well as some "special"
     * keys which indicate the start of a group of entries.
     *
     * These special keys will have the .key portion of their @ref KEntryKey
     * set to QString::null.
     */
    KEntryMap m_entryMap;
    QString m_fileName;
    // this is defined out-of-line
    struct MyBackend;
    MyBackend* m_iniBackend;
};
