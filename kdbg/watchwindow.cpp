/*
 * Copyright Johannes Sixt
 * This file is licensed under the GNU General Public License Version 2.
 * See the file COPYING in the toplevel directory of the source directory.
 */

#include <klocale.h>			/* i18n */
#include <qdragobject.h>
#include "watchwindow.h"

WatchWindow::WatchWindow(QWidget* parent, const char* name) :
	QWidget(parent, name),
	m_watchEdit(this, "watch_edit"),
	m_watchAdd(i18n(" Add "), this, "watch_add"),
	m_watchDelete(i18n(" Del "), this, "watch_delete"),
	m_watchVariables(this, i18n("Expression"), "watch_variables"),
	m_watchV(this, 0),
	m_watchH(0)
{
    // setup the layout
    m_watchAdd.setMinimumSize(m_watchAdd.sizeHint());
    m_watchDelete.setMinimumSize(m_watchDelete.sizeHint());
    m_watchV.addLayout(&m_watchH, 0);
    m_watchV.addWidget(&m_watchVariables, 10);
    m_watchH.addWidget(&m_watchEdit, 10);
    m_watchH.addWidget(&m_watchAdd, 0);
    m_watchH.addWidget(&m_watchDelete, 0);

    connect(&m_watchEdit, SIGNAL(returnPressed()), SIGNAL(addWatch()));
    connect(&m_watchAdd, SIGNAL(clicked()), SIGNAL(addWatch()));
    connect(&m_watchDelete, SIGNAL(clicked()), SIGNAL(deleteWatch()));
    connect(&m_watchVariables, SIGNAL(currentChanged(QListViewItem*)),
	    SLOT(slotWatchHighlighted()));

    m_watchVariables.installEventFilter(this);
    setAcceptDrops(true);
}

WatchWindow::~WatchWindow()
{
}

bool WatchWindow::eventFilter(QObject*, QEvent* ev)
{
    if (ev->type() == QEvent::KeyPress)
    {
	QKeyEvent* kev = static_cast<QKeyEvent*>(ev);
	if (kev->key() == Key_Delete) {
	    emit deleteWatch();
	    return true;
	}
    }
    return false;
}

void WatchWindow::dragEnterEvent(QDragEnterEvent* event)
{
    event->accept(QTextDrag::canDecode(event));
}

void WatchWindow::dropEvent(QDropEvent* event)
{
    QString text;
    if (QTextDrag::decode(event, text))
    {
	// pick only the first line
	text = text.stripWhiteSpace();
	int pos = text.find('\n');
	if (pos > 0)
	    text.truncate(pos);
	text = text.stripWhiteSpace();
	if (!text.isEmpty())
	    emit textDropped(text);
    }
}


// place the text of the hightlighted watch expr in the edit field
void WatchWindow::slotWatchHighlighted()
{
    VarTree* expr = m_watchVariables.selectedItem();
    QString text = expr ? expr->computeExpr() : QString();
    m_watchEdit.setText(text);
}

#include "watchwindow.moc"
