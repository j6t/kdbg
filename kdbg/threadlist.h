// $Id$

// Copyright by Johannes Sixt
// This file is under GPL, the GNU General Public Licence

#ifndef THREADLIST_H
#define THREADLIST_H

#include <qlistview.h>
#include <qlist.h>
#include <qpixmap.h>

class ThreadInfo;
class ThreadEntry;

class ThreadList : public QListView
{
    Q_OBJECT
public:
    ThreadList(QWidget* parent, const char* name);
    ~ThreadList();

public slots:
    void updateThreads(QList<ThreadInfo>&);
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
