// $Id$

// Copyright by Johannes Sixt
// This file is under GPL, the GNU General Public Licence

#include "prefmisc.h"
#if QT_VERSION >= 200
#include <klocale.h>			/* i18n */
#else
#include <kapp.h>			/* i18n */
#endif
#include <stdlib.h>			/* atoi */

PrefMisc::PrefMisc(QWidget* parent) :
	QWidget(parent, "debugger"),
	m_grid(this, 4, 2, 10),
	m_popForeground(this, "pop_fore"),
	m_backTimeoutLabel(this, "back_to_lab"),
	m_backTimeout(this, "back_to"),
	m_tabWidthLabel(this, "tabwidth_lab"),
	m_tabWidth(this, "tabwidth")
{
    m_popForeground.setText(i18n("&Pop into foreground when program stops"));
    m_popForeground.setMinimumSize(m_popForeground.sizeHint());
    m_grid.addMultiCellWidget(&m_popForeground, 0, 0, 0, 1);
    m_grid.addRowSpacing(0, m_popForeground.sizeHint().height());

    m_backTimeoutLabel.setText(i18n("Time until window goes &back (in milliseconds):"));
    m_backTimeoutLabel.setMinimumSize(m_backTimeoutLabel.sizeHint());
    m_backTimeoutLabel.setBuddy(&m_backTimeout);
    m_backTimeout.setMinimumSize(m_backTimeout.sizeHint());
    m_grid.addWidget(&m_backTimeoutLabel, 1, 0);
    m_grid.addWidget(&m_backTimeout, 1, 1);

    m_tabWidthLabel.setText(i18n("&Tabstop every (characters):"));
    m_tabWidthLabel.setMinimumSize(m_tabWidthLabel.sizeHint());
    m_tabWidthLabel.setBuddy(&m_tabWidth);
    m_tabWidth.setMinimumSize(m_tabWidth.sizeHint());
    m_grid.addWidget(&m_tabWidthLabel, 2, 0);
    m_grid.addWidget(&m_tabWidth, 2, 1);

    m_grid.setColStretch(0, 10);
    // last (empty) row gets all the vertical stretch
    m_grid.setRowStretch(3, 10);
}

static int readNumeric(const QLineEdit& edit)
{
    QString str = edit.text();
#if QT_VERSION < 200
    const char* s = str;
#else
    const char* s = str.latin1();
#endif
    return atoi(s);
}

int PrefMisc::backTimeout() const
{
    return readNumeric(m_backTimeout);
}

void PrefMisc::setBackTimeout(int to)
{
    QString str;
    str.setNum(to);
    m_backTimeout.setText(str);
}

int PrefMisc::tabWidth() const
{
    return readNumeric(m_tabWidth);
}

void PrefMisc::setTabWidth(int tw)
{
    QString str;
    str.setNum(tw);
    m_tabWidth.setText(str);
}
