/*
 * Copyright Johannes Sixt
 * This file is licensed under the GNU General Public License Version 2.
 * See the file COPYING in the toplevel directory of the source directory.
 */

#ifndef ProcAttach_included
#define ProcAttach_included

#include "ui_procattachbase.h"
#include <QByteArray>
#include <QDialog>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <vector>


class QProcess;

/*
 * This is the full-featured version of the dialog. It is used when the
 * system features a suitable ps command.
 */

class ProcAttachPS : public QDialog, private Ui::ProcAttachBase
{
    Q_OBJECT
public:
    ProcAttachPS(QWidget* parent);
    ~ProcAttachPS();

    void setFilterText(const QString& text) { filterEdit->setText(text); }
    QString text() const;

protected:
    void runPS();

protected slots:
    void on_buttonRefresh_clicked();
    void on_filterEdit_textChanged(const QString& text);
    void on_processList_currentItemChanged();
    void slotTextReceived();
    void slotPSDone();

protected:
    void pushLine();
    bool setVisibility(QTreeWidgetItem* i, const QString& text);

    QProcess* m_ps;
    // parse state
    int m_pidCol;	//!< The PID column in the ps output
    int m_ppidCol;	//!< The parent-PID column in the ps output
    QByteArray m_token;
    std::vector<QString> m_line;
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

    void setText(const QString& text) { m_processId.setText(text); }
    QString text() const { return m_processId.text(); }

protected:
    QLabel m_label;
    QLineEdit m_processId;
    QPushButton m_buttonOK;
    QPushButton m_buttonCancel;
    QVBoxLayout m_layout;
    QHBoxLayout m_buttons;
};

#endif // ProcAttach_included
