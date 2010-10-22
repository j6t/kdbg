/*
 * Copyright Johannes Sixt
 * This file is licensed under the GNU General Public License Version 2.
 * See the file COPYING in the toplevel directory of the source directory.
 */

#include "procattach.h"
#include <QHeaderView>
#include <QProcess>
#include <QTreeWidget>
#include <ctype.h>
#include <kglobal.h>
#include <kiconloader.h>
#include <klocale.h>			/* i18n */
#include "config.h"


ProcAttachPS::ProcAttachPS(QWidget* parent) :
	QDialog(parent),
	m_pidCol(-1),
	m_ppidCol(-1)
{
    setupUi(this);
    on_processList_currentItemChanged();	// update OK button state

    m_ps = new QProcess;
    connect(m_ps, SIGNAL(readyReadStandardOutput()),
	    this, SLOT(slotTextReceived()));
    connect(m_ps, SIGNAL(finished(int,QProcess::ExitStatus)),
	    this, SLOT(slotPSDone()));

    processList->setColumnWidth(0, 300);
    processList->header()->setResizeMode(0, QHeaderView::Interactive);
    processList->header()->setResizeMode(1, QHeaderView::ResizeToContents);
    processList->header()->setResizeMode(2, QHeaderView::ResizeToContents);
    processList->headerItem()->setTextAlignment(1, Qt::AlignRight);
    processList->headerItem()->setTextAlignment(2, Qt::AlignRight);
    processList->header()->setStretchLastSection(false);

    runPS();
}

ProcAttachPS::~ProcAttachPS()
{
    delete m_ps;	// kills a running ps
}

void ProcAttachPS::runPS()
{
    // clear the parse state from previous runs
    m_token.clear();
    m_line.clear();
    m_pidCol = -1;
    m_ppidCol = -1;

    // set the command line
    const char* const psCommand[] = {
#ifdef PS_COMMAND
	PS_COMMAND,
#else
	"/bin/false",
#endif
	0
    };
    QStringList args;
    for (int i = 1; psCommand[i] != 0; i++) {
	args.push_back(psCommand[i]);
    }

    m_ps->start(psCommand[0], args);
}

void ProcAttachPS::slotTextReceived()
{
    QByteArray data = m_ps->readAllStandardOutput();
    char* buffer = data.data();
    char* end = buffer + data.size();

    // avoid expensive updates while items are inserted
    processList->setUpdatesEnabled(false);

    while (buffer < end)
    {
	// check new line
	if (*buffer == '\n')
	{
	    // push a tokens onto the line
	    if (!m_token.isEmpty()) {
		m_line.push_back(QString::fromLatin1(m_token));
		m_token.clear();
	    }
	    // and insert the line in the list
	    pushLine();
	    m_line.clear();
	    ++buffer;
	}
	// blanks: the last column gets the rest of the line, including blanks
	else if ((m_pidCol < 0 || int(m_line.size()) < processList->columnCount()-1) &&
		 isspace(*buffer))
	{
	    // push a token onto the line
	    if (!m_token.isEmpty()) {
		m_line.push_back(QString::fromLatin1(m_token));
		m_token.clear();
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
	    m_token += QByteArray(start, buffer-start);
	}
    }
    processList->setUpdatesEnabled(true);
}

void ProcAttachPS::pushLine()
{
    if (m_line.size() < 3)	// we need the PID, PPID, and COMMAND columns
	return;

    if (m_pidCol < 0)
    {
	// create columns if we don't have them yet
	bool allocate =	processList->columnCount() == 3;

	// we assume that the last column is the command
	m_line.pop_back();

	if (allocate)
	    processList->setColumnCount(1 + m_line.size());

	for (size_t i = 0; i < m_line.size(); i++)
	{
	    // we don't allocate the PID and PPID columns,
	    // but we need to know where in the ps output they are
	    if (m_line[i] == "PID") {
		m_pidCol = i;
	    } else if (m_line[i] == "PPID") {
		m_ppidCol = i;
	    } else if (allocate) {
		processList->headerItem()->setText(i+1, m_line[i]);
		// these columns are normally numbers
		processList->headerItem()->setTextAlignment(i+1,
					Qt::AlignRight);
		processList->header()->setResizeMode(i+1,
					QHeaderView::ResizeToContents);
	    }
	}
    }
    else
    {
	// insert a line
	// find the parent process
	QTreeWidgetItem* parent = 0;
	if (m_ppidCol >= 0 && m_ppidCol < int(m_line.size())) {
	    QList<QTreeWidgetItem*> items =
	    	processList->findItems(m_line[m_ppidCol], Qt::MatchFixedString|Qt::MatchRecursive, 1);
	    if (!items.isEmpty())
		parent = items.front();
	}

	// we assume that the last column is the command
	QTreeWidgetItem* item;
	if (parent == 0) {
	    item = new QTreeWidgetItem(processList, QStringList(m_line.back()));
	} else {
	    item = new QTreeWidgetItem(parent, QStringList(m_line.back()));
	}
	item->setExpanded(true);
	m_line.pop_back();
	int k = 3;
	for (size_t i = 0; i < m_line.size(); i++)
	{
	    // display the pid and ppid columns' contents in columns 1 and 2
	    int col;
	    if (int(i) == m_pidCol)
		col = 1;
	    else if (int(i) == m_ppidCol)
		col = 2;
	    else
		col = k++;
	    item->setText(col, m_line[i]);
	    item->setTextAlignment(col, Qt::AlignRight);
	}

	if (m_ppidCol >= 0 && m_pidCol >= 0) {	// need PID & PPID for this
	    /*
	     * It could have happened that a process was earlier inserted,
	     * whose parent process is the current process. Such processes
	     * were placed at the root. Here we go through all root items
	     * and check whether we must reparent them.
	     */
	    int i = 0;
	    while (i < processList->topLevelItemCount())
	    {
		QTreeWidgetItem* it = processList->topLevelItem(i);
		if (it->text(2) == m_line[m_pidCol]) {
		    processList->takeTopLevelItem(i);
		    item->addChild(it);
		} else
		    ++i;
	    }
	}
    }
}

void ProcAttachPS::slotPSDone()
{
    on_filterEdit_textChanged(filterEdit->text());
}

QString ProcAttachPS::text() const
{
    QTreeWidgetItem* item = processList->currentItem();

    if (item == 0)
	return QString();

    return item->text(1);
}

void ProcAttachPS::on_buttonRefresh_clicked()
{
    if (m_ps->state() == QProcess::NotRunning)
    {
	processList->clear();
	dialogButtons->button(QDialogButtonBox::Ok)->setEnabled(false);	// selection was cleared
	runPS();
    }
}

void ProcAttachPS::on_filterEdit_textChanged(const QString& text)
{
    for (int i = 0; i < processList->topLevelItemCount(); i++)
	setVisibility(processList->topLevelItem(i), text);
}

/**
 * Sets the visibility of \a i and
 * returns whether it was made visible.
 */
bool ProcAttachPS::setVisibility(QTreeWidgetItem* i, const QString& text)
{
    i->setHidden(false);
    bool visible = false;
    for (int j = 0; j < i->childCount(); j++)
    {
	if (setVisibility(i->child(j), text))
	    visible = true;
    }
    // look for text in the process name and in the PID
    visible = visible || text.isEmpty() ||
	i->text(0).indexOf(text, 0, Qt::CaseInsensitive) >= 0 ||
	i->text(1).indexOf(text) >= 0;

    if (!visible)
	i->setHidden(true);

    // disable the OK button if the selected item becomes invisible
    if (i->isSelected())
	dialogButtons->button(QDialogButtonBox::Ok)->setEnabled(visible);

    return visible;
}

void ProcAttachPS::on_processList_currentItemChanged()
{
    dialogButtons->button(QDialogButtonBox::Ok)->setEnabled(processList->currentItem() != 0);
}


ProcAttach::ProcAttach(QWidget* parent) :
	QDialog(parent),
	m_label(this),
	m_processId(this),
	m_buttonOK(this),
	m_buttonCancel(this),
	m_layout(this),
	m_buttons()
{
    QString title = KGlobal::caption();
    title += i18n(": Attach to process");
    setWindowTitle(title);

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


#include "procattach.moc"
