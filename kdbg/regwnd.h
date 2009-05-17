/*
 * Copyright Max Judin, Johannes Sixt
 * This file is licensed under the GNU General Public License Version 2.
 * See the file COPYING in the toplevel directory of the source directory.
 */

#ifndef REGWND_H
#define REGWND_H

#include <qlistview.h>
#include <list>
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
    void updateRegisters(const std::list<RegisterInfo>&);

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
