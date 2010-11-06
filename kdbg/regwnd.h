/*
 * Copyright Max Judin, Johannes Sixt
 * This file is licensed under the GNU General Public License Version 2.
 * See the file COPYING in the toplevel directory of the source directory.
 */

#ifndef REGWND_H
#define REGWND_H
#include <QTreeWidget>

#include <list>
#include <map>

class QMenu;
class RegisterViewItem;
class GroupingViewItem;
class QContextMenuEvent;
struct RegisterInfo;


class RegisterView : public QTreeWidget
{
    Q_OBJECT
public:
    RegisterView(QWidget* parent);
    ~RegisterView();

protected:
    void contextMenuEvent(QContextMenuEvent*);

protected slots:
    void slotModeChange(QAction*);
    void updateRegisters(const std::list<RegisterInfo>&);

private:
    void paletteChange(const QPalette& oldPal);
    void updateGroupVisibility();
    GroupingViewItem* findMatchingGroup(const QString& regName);
    GroupingViewItem* findGroup(const QString& groupName);
    QMenu* m_modemenu;
    typedef std::map<QString,RegisterViewItem*> RegMap;
    RegMap m_registers;

friend class RegisterViewItem;
};

#endif // REGWND_H
