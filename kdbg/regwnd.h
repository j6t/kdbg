// $Id$

// Copyright by Judin Max, Johannes Sixt
// This file is under GPL, the GNU General Public Licence

#ifndef REGWND_H
#define REGWND_H

#include <qlistview.h>

class QPopupMenu;
class RegisterView;

class RegisterViewItem : public QListViewItem
{
public:
    RegisterViewItem( RegisterView* parent, QString reg, QString value);
    ~RegisterViewItem();

    void setValue(QString value);

    QString m_reg;
    QString m_value;
    bool m_changes;
    bool m_found;

protected:
    virtual void paintCell(QPainter*, const QColorGroup& cg,
			   int column, int width, int alignment);

};

class RegisterView : public QListView
{
    Q_OBJECT
public:
    RegisterView(QWidget* parent, const char *name = 0L);
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

friend class RegisterViewItem;
};

#endif // REGWND_H
