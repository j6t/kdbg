// $Id$

// Copyright by Johannes Sixt
// This file is under GPL, the GNU General Public Licence

#ifndef UPDATEUI_H
#define UPDATEUI_H

#include <qobject.h>

// forward declarations
class QPopupMenu;
class KToolBar;

class UpdateUI : public QObject
{
    Q_OBJECT
public:
    virtual void setCheck(bool check) = 0;
    virtual void enable(bool enable) = 0;
    virtual void setText(const QString& text) = 0;
    
    int id;
signals:
    void updateUIItem(UpdateUI*);
protected:
    virtual ~UpdateUI();
};

class UpdateMenuUI : public UpdateUI
{
public:
    UpdateMenuUI(QPopupMenu* m, QObject* receiver, const char* slotUpdateItem);
    virtual ~UpdateMenuUI();
    virtual void setCheck(bool check);
    virtual void enable(bool enable);
    virtual void setText(const QString& text);
    
    int index;
    QPopupMenu* menu;

    void iterateMenu();
};

class UpdateToolbarUI : public UpdateUI
{
public:
    UpdateToolbarUI(KToolBar* t, QObject* receiver, const char* slotUpdateItem,
		    const int* idl, int c);
    virtual void setCheck(bool check);
    virtual void enable(bool enable);
    virtual void setText(const QString& text);
    
    int index;
    KToolBar* toolbar;
    int count;
    const int* idlist;

    void iterateToolbar();
};

#endif // UPDATEUI_H
