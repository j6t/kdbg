// $Id$

// Copyright by Judin Max, Johannes Sixt
// This file is under GPL, the GNU General Public Licence

#ifndef REGWND_H
#define REGWND_H

#include <qlistview.h>
#include <map>

class QPopupMenu;
class RegisterViewItem;
class GroupingViewItem;
struct RegisterInfo;


class RegisterView : public QListView
{
    Q_OBJECT
public:
    RegisterView(QWidget* parent, const char *name = 0L);
    ~RegisterView();

protected slots:
    void rightButtonClicked(QListViewItem*, const QPoint&, int);
    void slotModeChange(int);
    void updateRegisters(QList<RegisterInfo>&);

private:
    void paletteChange(const QPalette& oldPal);
    void updateGroupVisibility();
    GroupingViewItem* findMatchingGroup(const QString& regName);
    GroupingViewItem* findGroup(const QString& groupName);
    QPopupMenu* m_modemenu;
    typedef std::map<QString,RegisterViewItem*> RegMap;
    RegMap m_registers;

friend class RegisterViewItem;
};

#endif // REGWND_H
