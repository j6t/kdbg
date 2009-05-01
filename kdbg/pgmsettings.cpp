/*
 * Copyright Johannes Sixt
 * This file is licensed under the GNU General Public License Version 2.
 * See the file COPYING in the toplevel directory of the source directory.
 */

#include "pgmsettings.h"
#include <klocale.h>			/* i18n */
#include <kapplication.h>
#include <qfileinfo.h>
#include <qlayout.h>
#include <qlineedit.h>
#include <qlabel.h>
#include <qradiobutton.h>
#include <qbuttongroup.h>
#include "config.h"
#include "mydebug.h"


ChooseDriver::ChooseDriver(QWidget* parent) :
	QWidget(parent, "driver")
{
    QVBoxLayout* layout = new QVBoxLayout(this, 10);

    QLabel* label = new QLabel(this);
    label->setText(i18n("How to invoke &GDB - leave empty to use\n"
			"the default from the global options:"));
    label->setMinimumSize(label->sizeHint());
    layout->addWidget(label);

    m_debuggerCmd = new QLineEdit(this);
    m_debuggerCmd->setMinimumSize(m_debuggerCmd->sizeHint());
    layout->addWidget(m_debuggerCmd);
    label->setBuddy(m_debuggerCmd);

    layout->addStretch(10);
}

void ChooseDriver::setDebuggerCmd(const QString& cmd)
{
    m_debuggerCmd->setText(cmd);
}

QString ChooseDriver::debuggerCmd() const
{
    return m_debuggerCmd->text();
}


OutputSettings::OutputSettings(QWidget* parent) :
	QWidget(parent, "output")
{
    // the group is invisible
    m_group = new QButtonGroup(this);
    m_group->hide();

    QVBoxLayout* layout = new QVBoxLayout(this, 10);

    QRadioButton* btn;

    btn = new QRadioButton(i18n("&No input and output"), this);
    m_group->insert(btn, 0);
    btn->setMinimumSize(btn->sizeHint());
    layout->addWidget(btn);

    btn = new QRadioButton(i18n("&Only output, simple terminal emulation"), this);
    m_group->insert(btn, 1);
    btn->setMinimumSize(btn->sizeHint());
    layout->addWidget(btn);

    btn = new QRadioButton(i18n("&Full terminal emulation"), this);
    m_group->insert(btn, 7);
    btn->setMinimumSize(btn->sizeHint());
    layout->addWidget(btn);

    layout->addStretch(10);

    // there is no simpler way to get to the active button than
    // to connect to a signal
    connect(m_group, SIGNAL(clicked(int)), SLOT(slotLevelChanged(int)));
}

void OutputSettings::setTTYLevel(int l)
{
    m_group->setButton(l);
    m_ttyLevel = l;
}

void OutputSettings::slotLevelChanged(int id)
{
    m_ttyLevel = id;
    TRACE("new ttyLevel: " + QString().setNum(id));
}



ProgramSettings::ProgramSettings(QWidget* parent, QString exeName, bool modal) :
	QTabDialog(parent, "program_settings", modal),
	m_chooseDriver(this),
	m_output(this)
{
    // construct title
    QFileInfo fi(exeName);
    QString cap = kapp->caption();
    QString title = i18n("%1: Settings for %2");
    setCaption(title.arg(cap, fi.fileName()));

    setCancelButton(i18n("Cancel"));
    setOKButton(i18n("OK"));

    addTab(&m_chooseDriver, i18n("&Debugger"));
    addTab(&m_output, i18n("&Output"));
}

#include "pgmsettings.moc"
