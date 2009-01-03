/*
 * Copyright Johannes Sixt
 * This file is licensed under the GNU General Public License Version 2.
 * See the file COPYING in the toplevel directory of the source directory.
 */

#ifndef THREADLIST_H
#define THREADLIST_H

#include <qlistview.h>
#include <qpixmap.h>
#include <list>

class ThreadInfo;
class ThreadEntry;

class ThreadList : public QListView
{
    Q_OBJECT
public:
    ThreadList(QWidget* parent, const char* name);
    ~ThreadList();

public slots:
    void updateThreads(const std::list<ThreadInfo>&);
    void slotCurrentChanged(QListViewItem*);

signals:
    void setThread(int);

protected:
    ThreadEntry* threadById(int id);
    void makeNoFocusIcon();

    QPixmap m_focusIcon;
    QPixmap m_noFocusIcon;
};

#endif // THREADLIST_H
