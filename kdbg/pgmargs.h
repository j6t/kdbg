// $Id$

// Copyright by Johannes Sixt
// This file is under GPL, the GNU General Public Licence

#ifndef PgmArgs_included
#define PgmArgs_included

#include <qdialog.h>
#include <qlabel.h>
#include <qlineedit.h>
#include <qpushbutton.h>
#include <qlayout.h>
#include <ktablistbox.h>
#include "envvar.h"

class PgmArgs : public QDialog
{
    Q_OBJECT
public:
    PgmArgs(QWidget* parent, const QString& pgm, QDict<EnvVar>& envVars);
    virtual ~PgmArgs();

    void setArgs(const QString& text) { m_programArgs.setText(text); }
    const char* args() const { return m_programArgs.text(); }
    void setWd(const QString& wd) { m_wd.setText(wd); }
    QString wd() const { return m_wd.text(); }
    QDict<EnvVar>& envVars() { return m_envVars; }

protected:
    QDict<EnvVar> m_envVars;

    void initEnvList();
    void parseEnvInput(QString& name, QString& value);

    QLabel m_label;
    QLineEdit m_programArgs;
    QLabel m_wdLabel;
    QLineEdit m_wd;
    QLabel m_envLabel;
    QLineEdit m_envVar;
    KTabListBox m_envList;
    QPushButton m_buttonOK;
    QPushButton m_buttonCancel;
    QPushButton m_buttonModify;
    QPushButton m_buttonDelete;
    QHBoxLayout m_layout;
    QVBoxLayout m_edits;
    QVBoxLayout m_buttons;

protected slots:
    void modifyVar();
    void deleteVar();
    void envListHighlighted(int,int);
    void accept();
};

#endif // PgmArgs_included
