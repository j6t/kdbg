// $Id$

// Copyright by Johannes Sixt
// This file is under GPL, the GNU General Public Licence

#ifndef ProcAttach_included
#define ProcAttach_included

#include <qdialog.h>
#include <qlabel.h>
#include <qlined.h>
#include <qpushbt.h>
#include <qlayout.h>

class ProcAttach : public QDialog
{
public:
    ProcAttach(QWidget* parent);
    virtual ~ProcAttach();

    void setText(const char* text) { m_processId.setText(text); }
    const char* text() const { return m_processId.text(); }

protected:
    QLabel m_label;
    QLineEdit m_processId;
    QPushButton m_buttonOK;
    QPushButton m_buttonCancel;
    QVBoxLayout m_layout;
    QHBoxLayout m_buttons;
};

#endif // ProcAttach_included
