// $Id$

// Copyright by Johannes Sixt
// This file is under GPL, the GNU General Public Licence

#if QT_VERSION >= 200
#include <klocale.h>			/* i18n */
#else
#include <kapp.h>			/* i18n */
#endif
#include <stdlib.h>			/* atoi */
#include "prefmisc.h"

PrefMisc::PrefMisc(QWidget* parent) :
	QWidget(parent, "debugger"),
	m_grid(this, 3, 2, 10),
	m_popForeground(this, "pop_fore"),
	m_backTimeoutLabel(this, "back_to_lab"),
	m_backTimeout(this, "back_to")
{
    m_popForeground.setText(i18n("&Pop into foreground when program stops"));
    m_popForeground.setMinimumSize(m_popForeground.sizeHint());
    m_grid.addMultiCellWidget(&m_popForeground, 0, 0, 0, 1);
    m_grid.addRowSpacing(0, m_popForeground.sizeHint().height());

    m_backTimeoutLabel.setText(i18n("&Time until window goes back (in milliseconds):"));
    m_backTimeoutLabel.setMinimumSize(m_backTimeoutLabel.sizeHint());
    m_backTimeoutLabel.setBuddy(&m_backTimeout);
    m_backTimeout.setMinimumSize(m_backTimeout.sizeHint());
    m_grid.addWidget(&m_backTimeoutLabel, 1, 0);
    m_grid.addWidget(&m_backTimeout, 1, 1);

    m_grid.setColStretch(0, 10);
    // last (empty) row gets all the vertical stretch
    m_grid.setRowStretch(2, 10);
}

int PrefMisc::backTimeout() const
{
    QString str = m_backTimeout.text();
#if QT_VERSION < 200
    const char* s = str;
#else
    const char* s = str.latin1();
#endif
    return atoi(s);
}

void PrefMisc::setBackTimeout(int to)
{
    QString str;
    str.sprintf("%d", to);
    m_backTimeout.setText(str);
}
