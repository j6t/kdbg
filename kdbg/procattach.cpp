// $Id$

// Copyright by Johannes Sixt
// This file is under GPL, the GNU General Public Licence

#include "procattach.h"
#include <qlistview.h>
#include <kprocess.h>
#include <ctype.h>
#include <kapp.h>
#include <klocale.h>			/* i18n */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif


ProcAttachPS::ProcAttachPS(QWidget* parent) :
	ProcAttachBase(parent),
	m_firstLine(true)
{
    m_ps = new KProcess;
    connect(m_ps, SIGNAL(receivedStdout(KProcess*, char*, int)),
	    this, SLOT(slotTextReceived(KProcess*, char*, int)));

    // set the command line
    static const char* const psCommand[] = {
#ifdef PS_COMMAND
	PS_COMMAND,
#else
	"/bin/false",
#endif
	0
    };
    for (int i = 0; psCommand[i] != 0; i++) {
	*m_ps << psCommand[i];
    }

    runPS();
}

ProcAttachPS::~ProcAttachPS()
{
    delete m_ps;	// kills a running ps
}

void ProcAttachPS::runPS()
{
    // clear the parse state from previous runs
    m_token = "";
    m_line.clear();
    m_firstLine = true;

    m_ps->start(KProcess::NotifyOnExit, KProcess::Stdout);
}

void ProcAttachPS::slotTextReceived(KProcess*, char* buffer, int buflen)
{
    const char* end = buffer+buflen;
    while (buffer < end)
    {
	// check new line
	if (*buffer == '\n')
	{
	    // push a tokens onto the line
	    if (!m_token.isEmpty()) {
		m_line.push_back(QString::fromLatin1(m_token));
		m_token = "";
	    }
	    // and insert the line in the list
	    pushLine();
	    m_line.clear();
	    ++buffer;
	}
	// blanks: the last column gets the rest of the line, including blanks
	else if ((m_firstLine || int(m_line.size()) < processList->columns()-1) &&
		 isspace(*buffer))
	{
	    // push a token onto the line
	    if (!m_token.isEmpty()) {
		m_line.push_back(QString::fromLatin1(m_token));
		m_token = "";
	    }
	    do {
		++buffer;
	    } while (buffer < end && isspace(*buffer));
	}
	// tokens
	else
	{
	    const char* start = buffer;
	    do {
		++buffer;
	    } while (buffer < end && !isspace(*buffer));
	    // append to the current token
	    m_token += QCString(start, buffer-start+1);	// must count the '\0'
	}
    }
}

void ProcAttachPS::pushLine()
{
    if (m_line.size() == 0)
	return;

    if (m_firstLine)
    {
	m_firstLine = false;
	// create columns if we don't have them yet
	if (processList->columns() == 0) {
	    for (uint i = 0; i < m_line.size(); i++) {
		processList->addColumn(m_line[i]);
	    }
	}
    }
    else
    {
	// insert a line
	QListViewItem* item = new QListViewItem(processList, m_line[0]);
	for (uint i = 1; i < m_line.size(); i++) {
	    item->setText(i, m_line[i]);
	}
    }
}

void ProcAttachPS::processSelected(QListViewItem* item)
{
    pidEdit->setText(item->text(0));
}

void ProcAttachPS::refresh()
{
    if (!m_ps->isRunning())
    {
	processList->clear();
	runPS();
    }
}


ProcAttach::ProcAttach(QWidget* parent) :
	QDialog(parent, "procattach", true),
	m_label(this, "label"),
	m_processId(this, "procid"),
	m_buttonOK(this, "ok"),
	m_buttonCancel(this, "cancel"),
	m_layout(this, 8),
	m_buttons(4)
{
    QString title = kapp->caption();
    title += i18n(": Attach to process");
    setCaption(title);

    m_label.setMinimumSize(330, 24);
    m_label.setText(i18n("Specify the process number to attach to:"));

    m_processId.setMinimumSize(330, 24);
    m_processId.setMaxLength(100);
    m_processId.setFrame(true);

    m_buttonOK.setMinimumSize(100, 30);
    connect(&m_buttonOK, SIGNAL(clicked()), SLOT(accept()));
    m_buttonOK.setText(i18n("OK"));
    m_buttonOK.setDefault(true);

    m_buttonCancel.setMinimumSize(100, 30);
    connect(&m_buttonCancel, SIGNAL(clicked()), SLOT(reject()));
    m_buttonCancel.setText(i18n("Cancel"));

    m_layout.addWidget(&m_label);
    m_layout.addWidget(&m_processId);
    m_layout.addLayout(&m_buttons);
    m_layout.addStretch(10);
    m_buttons.addStretch(10);
    m_buttons.addWidget(&m_buttonOK);
    m_buttons.addSpacing(40);
    m_buttons.addWidget(&m_buttonCancel);
    m_buttons.addStretch(10);

    m_layout.activate();

    m_processId.setFocus();
    resize(350, 120);
}

ProcAttach::~ProcAttach()
{
}
