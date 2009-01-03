/*
 * Copyright Johannes Sixt
 * This file is licensed under the GNU General Public License Version 2.
 * See the file COPYING in the toplevel directory of the source directory.
 */

#include "threadlist.h"
#include "dbgdriver.h"
#include <klocale.h>
#include <kiconloader.h>
#include <qbitmap.h>
#include <qpainter.h>


class ThreadEntry : public QListViewItem, public ThreadInfo
{
public:
    ThreadEntry(QListView* parent, const ThreadInfo& thread);
    void setFunction(const QString& func);

    bool m_delete;			/* used for updating the list */
};

ThreadEntry::ThreadEntry(QListView* parent, const ThreadInfo& thread) :
	QListViewItem(parent, thread.threadName, thread.function),
	ThreadInfo(thread),
	m_delete(false)
{
}

void ThreadEntry::setFunction(const QString& func)
{
    function = func;
    setText(1, function);
}


ThreadList::ThreadList(QWidget* parent, const char* name) :
	QListView(parent, name)
{
    addColumn(i18n("Thread ID"), 150);
    addColumn(i18n("Location"));

    // load pixmaps
    m_focusIcon = UserIcon("pcinner");
    makeNoFocusIcon();

    connect(this, SIGNAL(currentChanged(QListViewItem*)),
	    this, SLOT(slotCurrentChanged(QListViewItem*)));
}

ThreadList::~ThreadList()
{
}

void ThreadList::updateThreads(const std::list<ThreadInfo>& threads)
{
    // reset flag in all items
    for (QListViewItem* e = firstChild(); e != 0; e = e->nextSibling()) {
	static_cast<ThreadEntry*>(e)->m_delete = true;
    }

    for (std::list<ThreadInfo>::const_iterator i = threads.begin(); i != threads.end(); ++i)
    {
	// look up this thread by id
	ThreadEntry* te = threadById(i->id);
	if (te == 0) {
	    te = new ThreadEntry(this, *i);
	} else {
	    te->m_delete = false;
	    te->setFunction(i->function);
	}
	// set focus icon
	te->hasFocus = i->hasFocus;
	te->setPixmap(0, i->hasFocus  ?  m_focusIcon  :  m_noFocusIcon);
    }

    // delete all entries that have not been seen
    for (QListViewItem* e = firstChild(); e != 0;) {
	ThreadEntry* te = static_cast<ThreadEntry*>(e);
	e = e->nextSibling();		/* step ahead before deleting it ;-) */
	if (te->m_delete) {
	    delete te;
	}
    }
}

ThreadEntry* ThreadList::threadById(int id)
{
    for (QListViewItem* e = firstChild(); e != 0; e = e->nextSibling()) {
	ThreadEntry* te = static_cast<ThreadEntry*>(e);
	if (te->id == id) {
	    return te;
	}
    }
    return 0;
}

/*
 * Creates an icon of the same size as the m_focusIcon, but which is
 * totally transparent.
 */
void ThreadList::makeNoFocusIcon()
{
    m_noFocusIcon = m_focusIcon;
    {
	QPainter p(&m_noFocusIcon);
	p.fillRect(0,0, m_noFocusIcon.width(),m_noFocusIcon.height(), QColor(white));
    }
    m_noFocusIcon.setMask(m_noFocusIcon.createHeuristicMask());
}

void ThreadList::slotCurrentChanged(QListViewItem* newItem)
{
    if (newItem == 0)
	return;

    ThreadEntry* te = static_cast<ThreadEntry*>(newItem);

    // change the focused thread
    if (te->hasFocus)
	return;

    emit setThread(te->id);
}


#include "threadlist.moc"
