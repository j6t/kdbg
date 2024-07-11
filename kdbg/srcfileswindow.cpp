/*
 * Copyright Johannes Sixt
 * This file is licensed under the GNU General Public License Version 2.
 * See the file COPYING in the toplevel directory of the source directory.
 */

#include "srcfileswindow.h"
#include <QStringList>
#include <QFileInfo>
#include <QVariant>
#include <QMimeDatabase>
#include <QMimeType>
#include <klocalizedstring.h>
#include <kconfigbase.h>
#include <kconfiggroup.h>
#include "debugger.h"

const int COL_ADDR = 0;
const int COL_DUMP_ASCII = 9;
const int MAX_COL = 10;

QMimeDatabase mime_database;

QIcon icon_for_filename(const QString &filename)
{
  QIcon icon;
  QList<QMimeType> mime_types = mime_database.mimeTypesForFileName(filename);
  for (int i=0; i < mime_types.count() && icon.isNull(); i++)
    icon = QIcon::fromTheme(mime_types[i].iconName());

  if (icon.isNull()) {
    icon = QIcon::fromTheme("file");
  }
  return icon;
}

void SrcFilesWindow::file_clicked(const QModelIndex &index)
{
    QStandardItem *item = m_srcfiles_model.itemFromIndex(index);
    if (!item->data().isNull()) {
        emit activateFileLine(item->data().toString(), 0, DbgAddr());
    }
}


SrcFilesWindow::SrcFilesWindow(QWidget* parent) :
	QWidget(parent),
	m_srcfiles_model(this),
	m_srctree(this),
	m_layout(QBoxLayout::TopToBottom, this)
{
    m_srctree.setHeaderHidden(true);
    m_srctree.setModel(&m_srcfiles_model);
    connect(&m_srctree, &QTreeView::clicked, this, &SrcFilesWindow::file_clicked);
    m_layout.addWidget(&m_srctree, 0);
    m_layout.activate();
}

SrcFilesWindow::~SrcFilesWindow()
{
}

void SrcFilesWindow::sourceFiles(QString &files) {
    QStandardItem *item; 
    QStandardItem *parentItem;
    QStringList fileNames = files.split(",");

    m_srcfiles_model.clear(); 

    auto map_tree = new QMap<QString, QStandardItem *>;
    for (const auto& filename : fileNames) {
        QFileInfo *fi = new QFileInfo(filename.trimmed());
        QStringList path_list = fi->path().split("/");
        parentItem = m_srcfiles_model.invisibleRootItem();

        /* Create folder and files tree from file paths a/b/c/f/f1.c a/b/e/f2.c
         * |---a/
         *     |---b/
         *         |---c/
         *             |---f/
         *                 |---f1.c
         *     |---e/
         *         |---f2.c
         */

        /* TODO check if file exists in file system???? should exists... */
        for (int i = 0; i < path_list.size(); i++) {
            auto p = fi->path().section("/", 0, i);
            auto k_it = map_tree->find(p);
            if (p == "" || p == ".") {
                continue;
            }
            if (k_it != map_tree->end() ) {
                /* Directory already exists in tree */
                parentItem = k_it.value();
            } else {
                /* Directory not exists create it */
                item = new QStandardItem(QIcon::fromTheme("folder"), path_list.at(i));
                item->setEditable(false);
                /* Save path of directory created */
                parentItem->insertRow(0, item);
                parentItem->sortChildren(0);
                parentItem = item;
                map_tree->insert(p, item);
            }
        }   

        item = new QStandardItem(icon_for_filename(fi->fileName()), fi->fileName());
        item->setEditable(false);
        item->setData(QVariant(fi->filePath()));
        parentItem->appendRow(item);
        parentItem->sortChildren(0);
        map_tree->insert(fi->filePath(), item);
        delete fi;
    }
    delete map_tree;
}


