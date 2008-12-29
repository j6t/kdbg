/*
 * Copyright Johannes Sixt
 * This file is licensed under the GNU General Public License Version 2.
 * See the file COPYING in the toplevel directory of the source directory.
 */

#include "procattach.h"
#include <qlistview.h>
#include <qtoolbutton.h>
#include <qlineedit.h>
#include <kprocess.h>
#include <ctype.h>
#include <kapplication.h>
#include <kiconloader.h>
#include <klocale.h>			/* i18n */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif


ProcAttachPS::ProcAttachPS(QWidget* parent) :
	ProcAttachBase(parent),
	m_pidCol(-1),
	m_ppidCol(-1)
{
    m_ps = new KProcess;
    connect(m_ps, SIGNAL(receivedStdout(KProcess*, char*, int)),
	    this, SLOT(slotTextReceived(KProcess*, char*, int)));
    connect(m_ps, SIGNAL(processExited(KProcess*)),
	    this, SLOT(slotPSDone()));

    QIconSet icon = SmallIconSet("clear_left");
    filterClear->setIconSet(icon);

    processList->setColumnWidth(0, 300);
    processList->setColumnWidthMode(0, QListView::Manual);
    processList->setColumnAlignment(1, Qt::AlignRight);
    processList->setColumnAlignment(2, Qt::AlignRight);

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
    m_pidCol = -1;
    m_ppidCol = -1;

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
	else if ((m_pidCol < 0 || int(m_line.size()) < processList->columns()-1) &&
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
    if (m_line.size() < 3)	// we need the PID, PPID, and COMMAND columns
	return;

    if (m_pidCol < 0)
    {
	// create columns if we don't have them yet
	bool allocate =	processList->columns() == 3;

	// we assume that the last column is the command
	m_line.pop_back();

	for (uint i = 0; i < m_line.size(); i++) {
	    // we don't allocate the PID and PPID columns,
	    // but we need to know where in the ps output they are
	    if (m_line[i] == "PID") {
		m_pidCol = i;
	    } else if (m_line[i] == "PPID") {
		m_ppidCol = i;
	    } else if (allocate) {
		processList->addColumn(m_line[i]);
		// these columns are normally numbers
		processList->setColumnAlignment(processList->columns()-1,
						Qt::AlignRight);
	    }
	}
    }
    else
    {
	// insert a line
	// find the parent process
	QListViewItem* parent = 0;
	if (m_ppidCol >= 0 && m_ppidCol < int(m_line.size())) {
	    parent = processList->findItem(m_line[m_ppidCol], 1);
	}

	// we assume that the last column is the command
	QListViewItem* item;
	if (parent == 0) {
	    item = new QListViewItem(processList, m_line.back());
	} else {
	    item = new QListViewItem(parent, m_line.back());
	}
	item->setOpen(true);
	m_line.pop_back();
	int k = 3;
	for (uint i = 0; i < m_line.size(); i++)
	{
	    // display the pid and ppid columns' contents in columns 1 and 2
	    if (int(i) == m_pidCol)
		item->setText(1, m_line[i]);
	    else if (int(i) == m_ppidCol)
		item->setText(2, m_line[i]);
	    else
		item->setText(k++, m_line[i]);
	}

	if (m_ppidCol >= 0 && m_pidCol >= 0) {	// need PID & PPID for this
	    /*
	     * It could have happened that a process was earlier inserted,
	     * whose parent process is the current process. Such processes
	     * were placed at the root. Here we go through all root items
	     * and check whether we must reparent them.
	     */
	    QListViewItem* i = processList->firstChild();
	    while (i != 0)
	    {
		// advance before we reparent the item
		QListViewItem* it = i;
		i = i->nextSibling();
		if (it->text(2) == m_line[m_pidCol]) {
		    processList->takeItem(it);
		    item->insertItem(it);
		}
	    }
	}
    }
}

void ProcAttachPS::slotPSDone()
{
    filterEdited(filterEdit->text());
}

QString ProcAttachPS::text() const
{
    QListViewItem* item = processList->selectedItem();

    if (item == 0)
	return QString();

    return item->text(1);
}

void ProcAttachPS::refresh()
{
    if (!m_ps->isRunning())
    {
	processList->clear();
	buttonOk->setEnabled(false);	// selection was cleared
	runPS();
    }
}

void ProcAttachPS::filterEdited(const QString& text)
{
    QListViewItem* i = processList->firstChild();
    if (i) {
	setVisibility(i, text);
    }
}

/**
 * Sets the visibility of \a i and
 * returns whether it was made visible.
 */
bool ProcAttachPS::setVisibility(QListViewItem* i, const QString& text)
{
    bool visible = false;
    for (QListViewItem* j = i->firstChild(); j; j = j->nextSibling())
    {
	if (setVisibility(j, text))
	    visible = true;
    }
    // look for text in the process name and in the PID
    visible = visible || text.isEmpty() ||
	i->text(0).find(text, 0, false) >= 0 ||
	i->text(1).find(text) >= 0;

    i->setVisible(visible);

    // disable the OK button if the selected item becomes invisible
    if (i->isSelected())
	buttonOk->setEnabled(visible);

    return visible;
}

void ProcAttachPS::selectedChanged()
{
    buttonOk->setEnabled(processList->selectedItem() != 0);
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
