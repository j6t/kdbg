// $Id$

// Copyright by Johannes Sixt
// This file is under GPL, the GNU General Public Licence

#ifndef PgmArgs_included
#define PgmArgs_included

#include <qdialog.h>
#include <qlabel.h>
#include <qlined.h>
#include <qpushbt.h>
#include <qlayout.h>
#include <ktablistbox.h>
#include "envvar.h"

class PgmArgs : public QDialog
{
    Q_OBJECT
public:
    PgmArgs(QWidget* parent, const char* pgm, QDict<EnvVar>& envVars);
    virtual ~PgmArgs();

    void setText(const char* text) { m_programArgs.setText(text); }
    const char* text() const { return m_programArgs.text(); }
    QDict<EnvVar>& envVars() { return m_envVars; }

protected:
    QDict<EnvVar> m_envVars;

    void initEnvList();
    void parseEnvInput(QString& name, QString& value);

    QLabel m_label;
    QLineEdit m_programArgs;
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
