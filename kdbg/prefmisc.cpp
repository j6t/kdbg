/*
 * Copyright Johannes Sixt
 * This file is licensed under the GNU General Public License Version 2.
 * See the file COPYING in the toplevel directory of the source directory.
 */

#include "prefmisc.h"
#include <klocale.h>			/* i18n */

PrefMisc::PrefMisc(QWidget* parent) :
	QWidget(parent, "debugger"),
	m_grid(this, 6, 2, 10),
	m_popForeground(this, "pop_fore"),
	m_backTimeoutLabel(this, "back_to_lab"),
	m_backTimeout(this, "back_to"),
	m_tabWidthLabel(this, "tabwidth_lab"),
	m_tabWidth(this, "tabwidth"),
	m_sourceFilterLabel(this, "sourcefilter_lab"),
	m_sourceFilter(this, "sourcefilter"),
	m_headerFilterLabel(this, "headerfilter_lab"),
	m_headerFilter(this, "headerfilter")
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

    setupEditGroup(i18n("&Tabstop every (characters):"),
		   m_tabWidthLabel, m_tabWidth, 2);
    setupEditGroup(i18n("File filter for &source files:"),
		   m_sourceFilterLabel, m_sourceFilter, 3);
    setupEditGroup(i18n("File filter for &header files:"),
		   m_headerFilterLabel, m_headerFilter, 4);

    m_grid.setColStretch(1, 10);
    // last (empty) row gets all the vertical stretch
    m_grid.setRowStretch(5, 10);
}

void PrefMisc::setupEditGroup(const QString& label, QLabel& labWidget, QLineEdit& edit, int row)
{
    labWidget.setText(label);
    labWidget.setMinimumSize(labWidget.sizeHint());
    labWidget.setBuddy(&edit);
    edit.setMinimumSize(edit.sizeHint());
    m_grid.addWidget(&labWidget, row, 0);
    m_grid.addWidget(&edit, row, 1);
}

static int readNumeric(const QLineEdit& edit)
{
    QString str = edit.text();
    return str.toInt();
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
