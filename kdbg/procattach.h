// $Id$

// Copyright by Johannes Sixt
// This file is under GPL, the GNU General Public Licence

#ifndef ProcAttach_included
#define ProcAttach_included

#include "procattachbase.h"
#include <qvaluevector.h>
#include <qdialog.h>
#include <qlabel.h>
#include <qlineedit.h>
#include <qpushbutton.h>
#include <qlayout.h>


class KProcess;

/*
 * This is the full-featured version of the dialog. It is used when the
 * system features a suitable ps command.
 */

class ProcAttachPS : public ProcAttachBase
{
    Q_OBJECT
public:
    ProcAttachPS(QWidget* parent);
    ~ProcAttachPS();

    QString text() const { return pidEdit->text(); }

protected:
    void runPS();
    virtual void processSelected(QListViewItem*);
    virtual void refresh();
    virtual void pidEdited(const QString& text);

protected slots:
    void slotTextReceived(KProcess* proc, char* buffer, int buflen);

protected:
    void pushLine();

    KProcess* m_ps;
    // parse state
    int m_pidCol;	//!< The PID column in the ps output
    QCString m_token;
    QValueVector<QString> m_line;
};


/*
 * This is an extremely stripped down version of the dialog. It is used
 * when there is no suitable ps command.
 */

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
