// $Id$

// Copyright by Johannes Sixt
// This file is under GPL, the GNU General Public Licence

#ifndef PgmArgs_included
#define PgmArgs_included

#include <qdialog.h>
#include <qlabel.h>
#include <qlined.h>
#include <qpushbt.h>
#include <qlayout.h>

class PgmArgs : public QDialog
{
    Q_OBJECT
public:
    PgmArgs(QWidget* parent, const char* pgm);
    virtual ~PgmArgs();

    void setText(const char* text) { m_programArgs.setText(text); }
    const char* text() const { return m_programArgs.text(); }

public slots:

protected slots:
//    virtual void accept();
//    virtual void reject();

protected:
    QLabel m_label;
    QLineEdit m_programArgs;
    QPushButton m_buttonOK;
    QPushButton m_buttonCancel;
    QVBoxLayout m_layout;
    QHBoxLayout m_buttons;

//    virtual void resizeEvent(QResizeEvent* ev);
};

#endif // PgmArgs_included
