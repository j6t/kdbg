/*
 * Copyright Max Judin, Johannes Sixt
 * This file is licensed under the GNU General Public License Version 2.
 * See the file COPYING in the toplevel directory of the source directory.
 */

#ifndef REGWND_H
#define REGWND_H

#include <Q3ListView>
#include <list>
#include <map>

class Q3PopupMenu;
class RegisterViewItem;
class GroupingViewItem;
struct RegisterInfo;


class RegisterView : public Q3ListView
{
    Q_OBJECT
public:
    RegisterView(QWidget* parent);
    ~RegisterView();

protected slots:
    void rightButtonClicked(Q3ListViewItem*, const QPoint&, int);
    void slotModeChange(int);
    void updateRegisters(const std::list<RegisterInfo>&);

private:
    void paletteChange(const QPalette& oldPal);
    void updateGroupVisibility();
    GroupingViewItem* findMatchingGroup(const QString& regName);
    GroupingViewItem* findGroup(const QString& groupName);
    Q3PopupMenu* m_modemenu;
    typedef std::map<QString,RegisterViewItem*> RegMap;
    RegMap m_registers;

friend class RegisterViewItem;
};

#endif // REGWND_H
