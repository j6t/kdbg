// $Id$

// Copyright by Johannes Sixt
// This file is under GPL, the GNU General Public Licence

#include "pgmargs.h"
#include <kapp.h>

PgmArgs::PgmArgs(QWidget* parent, const char* pgm) :
	QDialog(parent, "pgmargs", true),
	m_label(this, "label"),
	m_programArgs(this, "args"),
	m_buttonOK(this, "ok"),
	m_buttonCancel(this, "cancel"),
	m_layout(this, 8),
	m_buttons(4)
{
    QString title = kapp->getCaption();
    title += i18n(": Program arguments");
    setCaption(title);

    m_label.setMinimumSize(330, 24);
    QString lab;
    lab.sprintf(i18n("Run %s with these arguments:"), pgm);
    m_label.setText(lab);

    m_programArgs.setMinimumSize(330, 24);
    m_programArgs.setMaxLength(10000);
    m_programArgs.setFrame(true);

    m_buttonOK.setMinimumSize(100, 30);
    connect(&m_buttonOK, SIGNAL(clicked()), SLOT(accept()));
    m_buttonOK.setText(i18n("OK"));
    m_buttonOK.setDefault(true);

    m_buttonCancel.setMinimumSize(100, 30);
    connect(&m_buttonCancel, SIGNAL(clicked()), SLOT(reject()));
    m_buttonCancel.setText(i18n("Cancel"));

    m_layout.addWidget(&m_label);
    m_layout.addWidget(&m_programArgs);
    m_layout.addLayout(&m_buttons);
    m_layout.addStretch(10);
    m_buttons.addStretch(10);
    m_buttons.addWidget(&m_buttonOK);
    m_buttons.addSpacing(40);
    m_buttons.addWidget(&m_buttonCancel);
    m_buttons.addStretch(10);

    m_layout.activate();

    m_programArgs.setFocus();
    resize(350, 120);
}

PgmArgs::~PgmArgs()
{
}
