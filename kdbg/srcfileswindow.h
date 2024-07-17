/*
 * Copyright Johannes Sixt
 * This file is licensed under the GNU General Public License Version 2.
 * See the file COPYING in the toplevel directory of the source directory.
 */

#ifndef SRCFILESWINDOW_H
#define SRCFILESWINDOW_H

#include <QBoxLayout>  
#include <QModelIndex>
#include <QTreeView>
#include <QStandardItemModel>
#include "dbgdriver.h"

class KDebugger;
class KConfigBase;

class SrcFilesWindow : public QWidget
{
    Q_OBJECT
public:
    SrcFilesWindow(QWidget* parent);
    ~SrcFilesWindow();
signals:
    void activateFileLine(const QString& file, int lineNo, const DbgAddr& address);

protected:
    QStandardItemModel m_srcfiles_model;
    QTreeView m_srctree;
    QBoxLayout m_layout;

    void fileClicked(const QModelIndex &index);
public slots:
    void sourceFiles(QString&);
};

#endif // SRCFILESWINDOW_H 
