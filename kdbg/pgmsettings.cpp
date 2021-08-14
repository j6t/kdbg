/*
 * Copyright Johannes Sixt
 * This file is licensed under the GNU General Public License Version 2.
 * See the file COPYING in the toplevel directory of the source directory.
 */

#include "pgmsettings.h"
#include <klocalizedstring.h>		/* i18n */
#include <QComboBox>
#include <QFileInfo>
#include <QLineEdit>
#include <QLabel>
#include <QRadioButton>
#include <QButtonGroup>
#include <QVBoxLayout>
#include "mydebug.h"


ChooseDriver::ChooseDriver(QWidget* parent) :
	QWidget(parent)
{
    QVBoxLayout* layout = new QVBoxLayout(this);
    QGridLayout* gridLayout = new QGridLayout;

    QLabel* label = new QLabel(this);
    label->setText(i18n("How to invoke &GDB - leave empty to use\n"
			"the default from the global options:"));
    label->setMinimumSize(label->sizeHint());
    layout->addWidget(label);

    m_debuggerCmd = new QLineEdit(this);
    m_debuggerCmd->setMinimumSize(m_debuggerCmd->sizeHint());
    layout->addWidget(m_debuggerCmd);
    label->setBuddy(m_debuggerCmd);

    // setup flavor-related functionality
    layout->addLayout(gridLayout, 1);

    m_disassComboBox = new QComboBox(this);
    m_disassComboBox->addItem(i18n("Use default"), "");
    m_disassComboBox->addItem(i18n("ATT"), "att");
    m_disassComboBox->addItem(i18n("Intel"), "intel");

    QLabel* disassLabel = new QLabel(i18n("Disassembly flavor:"), this);
    disassLabel->setMinimumSize(disassLabel->sizeHint());
    m_disassComboBox->setMinimumSize(m_disassComboBox->sizeHint());
    disassLabel->setBuddy(m_disassComboBox);
    gridLayout->addWidget(disassLabel, 0, 0);
    gridLayout->addWidget(m_disassComboBox, 0, 1);

    layout->addStretch();
    this->setLayout(layout);
}

void ChooseDriver::setDebuggerCmd(const QString& cmd)
{
    m_debuggerCmd->setText(cmd);
}

QString ChooseDriver::debuggerCmd() const
{
    return m_debuggerCmd->text();
}

QString ChooseDriver::disassemblyFlavor() const
{
    return m_disassComboBox->currentData().toString();
}

void ChooseDriver::setDisassemblyFlavor(const QString& flavor)
{
    int i = m_disassComboBox->findData(flavor);
    m_disassComboBox->setCurrentIndex(i < 0 ? 0 : i);
}

OutputSettings::OutputSettings(QWidget* parent) :
	QWidget(parent)
{
    m_group = new QButtonGroup(this);

    QVBoxLayout* layout = new QVBoxLayout(this);

    QRadioButton* btn;

    btn = new QRadioButton(i18n("&No input and output"), this);
    m_group->addButton(btn, 0);
    layout->addWidget(btn);

    btn = new QRadioButton(i18n("&Only output, simple terminal emulation"), this);
    m_group->addButton(btn, 1);
    layout->addWidget(btn);

    btn = new QRadioButton(i18n("&Full terminal emulation"), this);
    m_group->addButton(btn, 7);
    layout->addWidget(btn);

    layout->addStretch();

    this->setLayout(layout);

    // there is no simpler way to get to the active button than
    // to connect to a signal
    connect(m_group, SIGNAL(buttonClicked(int)), SLOT(slotLevelChanged(int)));
}

void OutputSettings::setTTYLevel(int l)
{
    QAbstractButton* button = m_group->button(l);
    Q_ASSERT(button);
    button->setChecked(true);
    m_ttyLevel = l;
}

void OutputSettings::slotLevelChanged(int id)
{
    m_ttyLevel = id;
    TRACE("new ttyLevel: " + QString().setNum(id));
}



ProgramSettings::ProgramSettings(QWidget* parent, QString exeName) :
	KPageDialog(parent),
	m_chooseDriver(this),
	m_output(this)
{
    // construct title
    QFileInfo fi(exeName);
    QString title = i18n("Settings for %1");
    setWindowTitle(title.arg(fi.fileName()));

    addPage(&m_chooseDriver, i18n("Debugger"));
    addPage(&m_output, i18n("Output"));
}

#include "pgmsettings.moc"
