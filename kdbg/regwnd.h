// $Id$

// Copyright by Judin Max, Johannes Sixt
// This file is under GPL, the GNU General Public Licence

#ifndef REGWND_H
#define REGWND_H

#include <qlistview.h>

class QPopupMenu;
class RegisterViewItem;
class DebuggerDriver;

class RegisterView : public QListView
{
    Q_OBJECT
public:
    RegisterView(QWidget* parent, DebuggerDriver* driver, const char *name = 0L);
    ~RegisterView();

    /** Parses the output from the info all-registers command */
    void updateRegisters(const char* output);

protected slots:
    void doubleClicked(QListViewItem*);
    void rightButtonClicked(QListViewItem*, const QPoint&, int);
    void slotSetFont();
    void slotModeChange(int);

private:
    QListViewItem* m_lastInsert;
    QPopupMenu* m_menu;
    QPopupMenu* m_modemenu;
    int m_mode;
    DebuggerDriver* m_d;

friend class RegisterViewItem;
};

#endif // REGWND_H
