/*
 * Copyright Johannes Sixt
 * This file is licensed under the GNU General Public License Version 2.
 * See the file COPYING in the toplevel directory of the source directory.
 */
#include <QDebug>

#include "srcfileswindow.h"
#include <QStringList>
#include <QStringList>
#include <QDir>
#include <QFileInfo>
#include <QVariant>
#include <QMimeDatabase>
#include <QMimeType>
#include <QMap>
#include <QSortFilterProxyModel>
#include <klocalizedstring.h>
#include <kconfigbase.h>
#include <kconfiggroup.h>
#include "debugger.h"

const int COL_ADDR = 0;
const int COL_DUMP_ASCII = 9;
const int MAX_COL = 10;

QMimeDatabase mime_database;

class SrcFilesSortProxyModel : public QSortFilterProxyModel {
public:
    SrcFilesSortProxyModel(QObject *parent = nullptr) : QSortFilterProxyModel(parent) {}

protected:
    bool lessThan(const QModelIndex &left, const QModelIndex &right) const override {
        QVariant leftData = sourceModel()->data(left);
        QVariant rightData = sourceModel()->data(right);

        // Custom sorting logic
        QString leftText = leftData.toString();
        QString rightText = rightData.toString();

        // Example: Sort directories first
        bool leftIsDir = left.model()->hasChildren(left); 
        bool rightIsDir = right.model()->hasChildren(right);

        if (leftIsDir != rightIsDir) {
            return leftIsDir < rightIsDir; // Directories first
        }

        return leftText.toLower() > rightText.toLower(); // Default alphabetic sorting
    }
};

QString getCommonPath(const QStringList &paths) {
    if (paths.isEmpty()) {
        return QString();
    }

    QString common = QFileInfo(paths[0]).absolutePath();
    for (const QString &path : paths) {
        QFileInfo fileInfo(path);
        QString absolutePath = fileInfo.absolutePath();
        while (!absolutePath.startsWith(common)) {
            common = QFileInfo(common).absolutePath();
            common = common.left(common.lastIndexOf(QDir::separator()));
            if (common.isEmpty()) {
                return QString();
            }
        }
    }
    return common;
}

QIcon iconForFilename(const QString &filename)
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

void SrcFilesWindow::fileClicked(const QModelIndex &index)
{
    QModelIndex sourceIndex = dynamic_cast<SrcFilesSortProxyModel*>(m_srctree.model())->mapToSource(index);
    QStandardItem *item = m_srcfiles_model.itemFromIndex(sourceIndex);
    if (!item->data().isNull()) {
        qDebug() << item->data();
        emit activateFileLine(item->data().toString(), 0, DbgAddr());
    }
}

SrcFilesWindow::SrcFilesWindow(QWidget* parent) :
	QWidget(parent),
	m_srcfiles_model(this),
	m_srctree(this),
	m_layout(QBoxLayout::TopToBottom, this)
{
    SrcFilesSortProxyModel *proxyModel = new SrcFilesSortProxyModel(this);
    m_srctree.setHeaderHidden(true);
    proxyModel->setSourceModel(&m_srcfiles_model);
    m_srctree.setModel(proxyModel);
    m_srctree.setSortingEnabled(true);
    connect(&m_srctree, &QTreeView::clicked, this, &SrcFilesWindow::fileClicked);
    m_layout.addWidget(&m_srctree, 0);
    m_layout.activate();
}

SrcFilesWindow::~SrcFilesWindow()
{
}


void SrcFilesWindow::sourceFiles(QString &files) {
    QStandardItem *item; 
    QStringList filenamesRaw = files.split(",");
    QStringList fileNames;

    m_srcfiles_model.clear(); 

    /* Sanitize filenames and skip system libraries */
    for (const auto& f: filenamesRaw) {
        QString filename = f.trimmed();
        if (filename.endsWith("<built-in>", Qt::CaseSensitive) ||
            filename.startsWith("/usr/", Qt::CaseSensitive) ||
            filename.startsWith("/lib/", Qt::CaseSensitive) ||
            filename.contains("/.cargo/", Qt::CaseSensitive) ||
            filename.startsWith("/rustc/", Qt::CaseSensitive) ||
            filename.startsWith("/cargo/", Qt::CaseSensitive)) {
            continue;
        }
        fileNames.append(filename);
    }

    /* Get common path to remove it */
    QString commonPath = getCommonPath(fileNames);


    QMap<QString, QStandardItem *> mapTree;
    for (const auto& f: fileNames) {
        QFileInfo fi(f);
        QStringList pathList = fi.path().remove(commonPath).split(QDir::separator());
        QStandardItem *parentItem = m_srcfiles_model.invisibleRootItem();

        /* Create folder and files tree from file paths a/b/c/f/f1.c a/b/e/f2.c
         * |---a/
         *     |---b/
         *         |---c/
         *             |---f/
         *                 |---f1.c
         *     |---e/
         *         |---f2.c
         *
         * Removing common path
         * 
         * |---b/
         *     |---c/
         *         |---f/
         *             |---f1.c
         * |---e/
         *     |---f2.c
         */

        /* TODO check if file exists in file system???? should exists... */
        for (int i = 0; i < pathList.size(); i++) {
            auto p = fi.path().section(QDir::separator(), 0, i);
            auto k_it = mapTree.find(p);
            if (p == "" || p == ".") {
                continue;
            }
            if (k_it != mapTree.end() ) {
                /* Directory already exists in tree */
                parentItem = k_it.value();
            } else {
                /* Directory not exists create it */
                item = new QStandardItem(QIcon::fromTheme("folder"), pathList.at(i));
                item->setEditable(false);
                /* Save path of directory created */
                parentItem->insertRow(0, item);
                parentItem->sortChildren(0);
                parentItem = item;
                mapTree.insert(p, item);
            }
        }   

        item = new QStandardItem(iconForFilename(fi.fileName()), fi.fileName());
        item->setEditable(false);
        item->setData(QVariant(fi.filePath()));
        parentItem->appendRow(item);
        mapTree.insert(fi.filePath(), item);
    }

}


