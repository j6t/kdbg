// $Id$

// Copyright by Johannes Sixt
// This file is under GPL, the GNU General Public Licence

#ifndef PREFMISC_H
#define PREFMISC_H

#include <qlayout.h>
#include <qcheckbox.h>
#include <qlabel.h>
#include <qlineedit.h>

class PrefMisc : public QWidget
{
public:
    PrefMisc(QWidget* parent);

    QGridLayout m_grid;

protected:
    QCheckBox m_popForeground;

    QLabel m_backTimeoutLabel;
    QLineEdit m_backTimeout;

    QLabel m_tabWidthLabel;
    QLineEdit m_tabWidth;
public:
    bool popIntoForeground() const { return m_popForeground.isChecked(); }
    void setPopIntoForeground(bool pop) { m_popForeground.setChecked(pop); }
    int backTimeout() const;
    void setBackTimeout(int to);
    int tabWidth() const;
    void setTabWidth(int tw);
};

#endif // PREFMISC_H
