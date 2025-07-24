/*
 * Copyright Johannes Sixt
 * This file is licensed under the GNU General Public License Version 2.
 * See the file COPYING in the toplevel directory of the source directory.
 */

#include "threadlist.h"
#include "dbgdriver.h"
#include <klocalizedstring.h>
#include <kiconloader.h>
#include <QBitmap>
#include <QHeaderView>
#include <QPainter>
#include <QStringList>


class ThreadEntry : public QTreeWidgetItem, public ThreadInfo
{
public:
    ThreadEntry(QTreeWidget* parent, const ThreadInfo& thread);
    void setFunction(const QString& func);

    bool m_delete;			/* used for updating the list */
};

ThreadEntry::ThreadEntry(QTreeWidget* parent, const ThreadInfo& thread) :
	QTreeWidgetItem(parent, QStringList() << thread.threadName << thread.function),
	ThreadInfo(thread),
	m_delete(false)
{
}

void ThreadEntry::setFunction(const QString& func)
{
    function = func;
    setText(1, function);
}


ThreadList::ThreadList(QWidget* parent) :
	QTreeWidget(parent)
{
    setHeaderLabels(QStringList() << i18n("Thread ID") << i18n("Location"));
    header()->setSectionResizeMode(1, QHeaderView::Interactive);
    setRootIsDecorated(false);

    // load pixmaps
    m_focusIcon = KIconLoader::global()->loadIcon("pcinner", KIconLoader::User);
    makeNoFocusIcon();

    connect(this, SIGNAL(currentItemChanged(QTreeWidgetItem*, QTreeWidgetItem*)),
	    this, SLOT(slotCurrentChanged(QTreeWidgetItem*)));
}

ThreadList::~ThreadList()
{
}

void ThreadList::updateThreads(const std::list<ThreadInfo>& threads)
{
    // reset flag in all items
    for (QTreeWidgetItemIterator i(this); *i; ++i)
	static_cast<ThreadEntry*>(*i)->m_delete = true;

    for (std::list<ThreadInfo>::const_iterator i = threads.begin(); i != threads.end(); ++i)
    {
	// look up this thread by id
	ThreadEntry* te = threadById(i->id);
	if (!te) {
	    te = new ThreadEntry(this, *i);
	} else {
	    te->m_delete = false;
	    te->setFunction(i->function);
	}
	// set focus icon
	te->hasFocus = i->hasFocus;
	te->setIcon(0, i->hasFocus  ?  QIcon(m_focusIcon)  :  QIcon(m_noFocusIcon));
    }

    // delete all entries that have not been seen
    for (QTreeWidgetItemIterator i(this); *i;)
    {
	ThreadEntry* te = static_cast<ThreadEntry*>(*i);
	++i;		// step ahead before deleting it ;-)
	if (te->m_delete) {
	    delete te;
	}
    }
}

ThreadEntry* ThreadList::threadById(int id)
{
    for (QTreeWidgetItemIterator i(this); *i; ++i)
    {
	ThreadEntry* te = static_cast<ThreadEntry*>(*i);
	if (te->id == id) {
	    return te;
	}
    }
    return nullptr;
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
	p.fillRect(0,0, m_noFocusIcon.width(),m_noFocusIcon.height(), QColor(Qt::white));
    }
    m_noFocusIcon.setMask(m_noFocusIcon.createHeuristicMask());
}

void ThreadList::slotCurrentChanged(QTreeWidgetItem* newItem)
{
    if (!newItem)
	return;

    ThreadEntry* te = static_cast<ThreadEntry*>(newItem);

    // change the focused thread
    if (te->hasFocus)
	return;

    Q_EMIT setThread(te->id);
}
