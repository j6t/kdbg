// $Id$

// Copyright by Johannes Sixt
// This file is under GPL, the GNU General Public Licence

#include "pgmargs.h"
#include "pgmargs.moc"
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
    setCaption(klocale->translate("KDbg: Program arguments"));

    m_label.setMinimumSize(330, 24);
    QString lab;
    lab.sprintf(klocale->translate("Run %s with these arguments:"), pgm);
    m_label.setText(lab);
    m_label.setAlignment( 33 );
    m_label.setMargin( -1 );

    m_programArgs.setMinimumSize(330, 24);
    m_programArgs.setMaxLength(10000);
//    m_programArgs.setEchoMode(QLineEdit::Normal);
    m_programArgs.setFrame(true);

    m_buttonOK.setMinimumSize(100, 30);
    connect(&m_buttonOK, SIGNAL(clicked()), SLOT(accept()));
    m_buttonOK.setText(klocale->translate("OK"));
//    m_buttonOK.setAutoRepeat( false );
//    m_buttonOK.setAutoResize( false );
    m_buttonOK.setDefault(true);

    m_buttonCancel.setMinimumSize(100, 30);
    connect(&m_buttonCancel, SIGNAL(clicked()), SLOT(reject()));
    m_buttonCancel.setText(klocale->translate("Cancel"));
//    m_buttonCancel.setAutoRepeat( false );
//    m_buttonCancel.setAutoResize( false );

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
    resize( 350, 120 );
}

PgmArgs::~PgmArgs()
{
}

#if 0
void PgmArgs::resizeEvent(QResizeEvent* /*ev*/)
{
    int newWidth = width() - 20;	/* 10 + 10 margin */
    int h;
    // resize edit field
    h = m_programArgs.height();
    m_programArgs.resize(newWidth, h);
    // resize label
    h = m_label.height();
    m_label.resize(newWidth, h);
}
#endif
