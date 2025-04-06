/*
 * Copyright Johannes Sixt
 * This file is licensed under the GNU General Public License Version 2.
 * See the file COPYING in the toplevel directory of the source directory.
 */

#ifndef PREFMISC_H
#define PREFMISC_H

#include <QCheckBox>
#include <QLabel>
#include <QLineEdit>
#include <QGridLayout>

class PrefMisc : public QWidget
{
    Q_OBJECT
public:
    PrefMisc(QWidget* parent);

    QGridLayout m_grid;

protected:
    QCheckBox m_popForeground;
    QCheckBox m_lowerWindow;
    QLineEdit m_backTimeout;

    QLabel m_tabWidthLabel;
    QLineEdit m_tabWidth;

    QLabel m_sourceFilterLabel;
    QLineEdit m_sourceFilter;
    QLabel m_headerFilterLabel;
    QLineEdit m_headerFilter;

    void setupEditGroup(const QString& label, QLabel& labWidget, QLineEdit& edit, int row);

public:
    bool popIntoForeground() const { return m_popForeground.isChecked(); }
    bool isLowerWindowRqed() const { return m_lowerWindow.isChecked(); }
    int backTimeout() const;
    void setBackTimeout(int to);
    int tabWidth() const;
    void setTabWidth(int tw);
    QString sourceFilter() const { return m_sourceFilter.text(); }
    void setSourceFilter(const QString& f) { m_sourceFilter.setText(f); }
    QString headerFilter() const { return m_headerFilter.text(); }
    void setHeaderFilter(const QString& f) { m_headerFilter.setText(f); }

public slots:
    void setPopIntoForeground(bool pop)
    {
        m_popForeground.setChecked(pop);
        m_lowerWindow.setEnabled(pop);
        if ( isLowerWindowRqed() )
        {
            m_backTimeout.setEnabled(pop);
        }
    }
    void setLowerWindow(bool is)
    {
        m_lowerWindow.setChecked(is);
        m_backTimeout.setEnabled(is);
    }
};

#endif // PREFMISC_H
