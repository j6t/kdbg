/*
 * Copyright Johannes Sixt
 * This file is licensed under the GNU General Public License Version 2.
 * See the file COPYING in the toplevel directory of the source directory.
 */

#ifndef PgmArgs_included
#define PgmArgs_included

#include "ui_pgmargsbase.h"
#include <QDialog>
#include <QSet>
#include <map>
#include "envvar.h"

class QStringList;

class PgmArgs : public QDialog, private Ui::PgmArgsBase
{
    Q_OBJECT
public:
    PgmArgs(QWidget* parent, const QString& pgm,
	    const std::map<QString,QString>& envVars,
	    const QStringList& allOptions);
    virtual ~PgmArgs();

    void setArgs(const QString& text) { programArgs->setText(text); }
    QString args() const { return programArgs->text(); }
    void setOptions(const QSet<QString>& selectedOptions);
    QSet<QString> options() const;
    void setWd(const QString& wd) { wdEdit->setText(wd); }
    QString wd() const { return wdEdit->text(); }
    const std::map<QString,EnvVar>& envVars() { return m_envVars; }

protected:
    std::map<QString,EnvVar> m_envVars;

    void parseEnvInput(QString& name, QString& value);
    void modifyVar(bool resurrect);
    virtual void accept();

protected slots:
    void on_buttonModify_clicked();
    void on_buttonDelete_clicked();
    void on_envList_currentItemChanged();
    void on_wdBrowse_clicked();
    void on_insertFile_clicked();
    void on_insertDir_clicked();
};

#endif // PgmArgs_included
