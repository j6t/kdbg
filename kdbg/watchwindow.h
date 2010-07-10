/*
 * Copyright Johannes Sixt
 * This file is licensed under the GNU General Public License Version 2.
 * See the file COPYING in the toplevel directory of the source directory.
 */

#ifndef WATCHWINDOW_H
#define WATCHWINDOW_H

#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include "exprwnd.h"

class WatchWindow : public QWidget
{
    Q_OBJECT
public:
    WatchWindow(QWidget* parent);
    ~WatchWindow();
    ExprWnd* watchVariables() { return &m_watchVariables; }
    QString watchText() const { return m_watchEdit.text(); }
    int columnWidth(int i) const { return m_watchVariables.columnWidth(i); }
    void setColumnWidth(int i, int w) { m_watchVariables.setColumnWidth(i, w); }

protected:
    QLineEdit m_watchEdit;
    QPushButton m_watchAdd;
    QPushButton m_watchDelete;
    ExprWnd m_watchVariables;
    QVBoxLayout m_watchV;
    QHBoxLayout m_watchH;

    virtual bool eventFilter(QObject* ob, QEvent* ev);
    virtual void dragEnterEvent(QDragEnterEvent* event);
    virtual void dropEvent(QDropEvent* event);

signals:
    void addWatch();
    void deleteWatch();
    void textDropped(const QString& text);

protected slots:
    void slotWatchHighlighted();
};

#endif // WATCHWINDOW_H
