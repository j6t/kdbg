/*
 * Copyright Johannes Sixt
 * This file is licensed under the GNU General Public License Version 2.
 * See the file COPYING in the toplevel directory of the source directory.
 */

#include "programconfig.h"
#include <kconfigbackend.h>
#include <qfile.h>
#include <sys/stat.h>
#include <unistd.h>


struct ProgramConfig::MyBackend : KConfigINIBackEnd
{
    MyBackend(KConfigBase* cfg, const QString& fn) :
	KConfigINIBackEnd(cfg, fn, "", false)
    { }
    // need the following public
    using KConfigINIBackEnd::parseSingleConfigFile;
};

ProgramConfig::ProgramConfig(const QString& fileName) :
	m_fileName(fileName)
{
    m_iniBackend = new MyBackend(this, fileName);
    backEnd = m_iniBackend;
    reparseConfiguration();
}

QStringList ProgramConfig::groupList() const
{
    // unused
    return QStringList();
}

QMap<QString, QString> ProgramConfig::entryMap(const QString&) const
{
    // unused
    return QMap<QString, QString>();
}

void ProgramConfig::reparseConfiguration()
{
    m_entryMap.clear();
    
    // add the "default group" marker to the map
    KEntryKey groupKey("<default>", 0);
    m_entryMap.insert(groupKey, KEntry());

    QFile file(m_fileName);
    bool readonly = true;
    bool useit = true;
    if (file.open(IO_ReadWrite)) {	/* don't truncate! */
	readonly = false;
	// the file exists now
    } else if (!file.open(IO_ReadOnly)) {
	/* file does not exist and cannot be created: don't use it */
	useit = false;
    }

    if (useit)
    {
	// Check ownership
	// Important: This must be done using fstat on the opened file
	// to avoid race conditions.
	struct stat s;
	memset(&s, 0, sizeof(s));
	useit =
	    fstat(file.handle(), &s) == 0 &&
	    s.st_uid == getuid();
    }

    if (useit)
    {
	m_iniBackend->parseSingleConfigFile(file, 0, false, false);
    }
    else
    {
	/*
	 * The program specific config file is not ours, so we do not trust
	 * it for the following reason: Should the debuggee be located in a
	 * world-writable directory somebody else may have created it where
	 * the entry DebuggerCmdStr contains a malicious command, or may
	 * have created a (possibly huge) file containing nonsense, which
	 * leads to a DoS.
	 */
    }

    // don't write the file if we don't own it
    setReadOnly(readonly || !useit);
}

KEntryMap ProgramConfig::internalEntryMap(const QString& group) const
{
    QCString group_utf = group.utf8();
    KEntryMap tmpEntryMap;

    // copy the whole group starting at the special group marker
    KEntryKey key(group_utf, 0);
    KEntryMapConstIterator it = m_entryMap.find(key);
    for (; it != m_entryMap.end() && it.key().mGroup == group_utf; ++it)
    {
	tmpEntryMap.insert(it.key(), *it);
    }

    return tmpEntryMap;
}

KEntryMap ProgramConfig::internalEntryMap() const
{
    return m_entryMap;
}

void ProgramConfig::putData(const KEntryKey& key, const KEntry& data, bool checkGroup)
{
    if (checkGroup)
    {
	// make sure the special group marker is present
	m_entryMap[KEntryKey(key.mGroup, 0)];
    }
    m_entryMap[key] = data;
}

KEntry ProgramConfig::lookupData(const KEntryKey& key) const
{
    KEntryMapConstIterator it;

    it = m_entryMap.find(key);
    if (it != m_entryMap.end())
    {
	const KEntry& entry = *it;
	if (entry.bDeleted)
	    return KEntry();
	else
	    return entry;
    }
    else {
	return KEntry();
    }
}

bool ProgramConfig::internalHasGroup(const QCString&) const
{
    // unused
    return false;
}
