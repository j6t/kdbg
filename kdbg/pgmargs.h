// $Id$

// Copyright by Johannes Sixt
// This file is under GPL, the GNU General Public Licence

#ifndef PgmArgs_included
#define PgmArgs_included

#include "pgmargsbase.h"
#include <qlineedit.h>
#include <qdict.h>
#include "envvar.h"

class QStringList;

class PgmArgs : public PgmArgsBase
{
    Q_OBJECT
public:
    PgmArgs(QWidget* parent, const QString& pgm, QDict<EnvVar>& envVars,
	    const QStringList& allOptions);
    virtual ~PgmArgs();

    void setArgs(const QString& text) { programArgs->setText(text); }
    QString args() const { return programArgs->text(); }
    void setOptions(const QStringList& selectedOptions);
    QStringList options() const;
    void setWd(const QString& wd) { wdEdit->setText(wd); }
    QString wd() const { return wdEdit->text(); }
    QDict<EnvVar>& envVars() { return m_envVars; }

protected:
    QDict<EnvVar> m_envVars;

    void initEnvList();
    void parseEnvInput(QString& name, QString& value);
    void modifyVar(bool resurrect);

protected slots:
    void modifyVar();
    void deleteVar();
    void envListCurrentChanged();
    void accept();
    void browseWd();
    void browseArgs();
};

#endif // PgmArgs_included
