// $Id$

// Copyright by Johannes Sixt
// This file is under GPL, the GNU General Public Licence

#include "updateui.h"
#include "updateui.moc"
#include <qpopmenu.h>
#include <ktoolbar.h>
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include <kdebug.h>

UpdateUI::~UpdateUI()
{
    disconnect(this);
}


UpdateMenuUI::UpdateMenuUI(QPopupMenu* m, QObject* receiver, const char* slotUpdateItem) :
	menu(m)
{
    ASSERT(menu != 0);
    connect(this, SIGNAL(updateUIItem(UpdateUI*)), receiver, slotUpdateItem);
}

UpdateMenuUI::~UpdateMenuUI()
{
}

void UpdateMenuUI::iterateMenu()
{
    ASSERT(menu != 0);

    for (int i = menu->count()-1; i >= 0; i--) {
	index = i;
	id = menu->idAt(i);
	emit updateUIItem(this);
    }
}

void UpdateMenuUI::setCheck(bool check)
{
    menu->setItemChecked(id, check);
}

void UpdateMenuUI::enable(bool enable)
{
    menu->setItemEnabled(id, enable);
}

void UpdateMenuUI::setText(const QString& text)
{
    menu->changeItem(text, id);
}

UpdateToolbarUI::UpdateToolbarUI(KToolBar* t, QObject* receiver, const char* slotUpdateItem,
				 const int* idl, int c) :
	toolbar(t),
	count(c),
	idlist(idl)
{
    ASSERT(toolbar != 0);
    connect(this, SIGNAL(updateUIItem(UpdateUI*)), receiver, slotUpdateItem);
}

void UpdateToolbarUI::iterateToolbar()
{
    ASSERT(toolbar != 0);

    for (int i = count-1; i >= 0; i--) {
	index = i;
	id = idlist[i];
	emit updateUIItem(this);
    }
}

void UpdateToolbarUI::setCheck(bool /*check*/)
{
    // checking a toolbar button is not done via UpdateUI
}

void UpdateToolbarUI::enable(bool enable)
{
    toolbar->setItemEnabled(id, enable);
}

void UpdateToolbarUI::setText(const QString& /*text*/)
{
}
