// $Id$

// Copyright by Judin Max, Johannes Sixt
// This file is under GPL, the GNU General Public Licence

#ifndef REGWND_H
#define REGWND_H

#include <qlistview.h>

class QPopupMenu;
class RegisterViewItem;
struct RegisterInfo;

class RegisterView : public QListView
{
    Q_OBJECT
public:
    RegisterView(QWidget* parent, const char *name = 0L);
    ~RegisterView();

protected slots:
    void doubleClicked(QListViewItem*);
    void rightButtonClicked(QListViewItem*, const QPoint&, int);
    void slotSetFont();
    void slotModeChange(int);
    void updateRegisters(QList<RegisterInfo>&);

private:
    QListViewItem* m_lastItem;
    QPopupMenu* m_menu;
    QPopupMenu* m_modemenu;
    int m_mode;

friend class RegisterViewItem;
};

#endif // REGWND_H
