/*
 * Copyright Johannes Sixt
 * This file is licensed under the GNU General Public License Version 2.
 * See the file COPYING in the toplevel directory of the source directory.
 */

#include "pgmargs.h"
#include <kfiledialog.h>
#include <klocale.h>			/* i18n */
#include "mydebug.h"

PgmArgs::PgmArgs(QWidget* parent, const QString& pgm,
		 const std::map<QString,QString>& envVars,
		 const QStringList& allOptions) :
	QDialog(parent)
{
    setupUi(this);

    {
	QFileInfo fi = pgm;
	QString newText = labelArgs->text().arg(fi.fileName());
	labelArgs->setText(newText);
    }

    // add options only if the option list is non-empty
    if (!allOptions.isEmpty()) 
    {
	xsldbgOptions->addItems(allOptions);
    }
    else
    {
	delete xsldbgOptionsPage;
	xsldbgOptionsPage = 0;
    }

    EnvVar val;
    val.status = EnvVar::EVclean;
    for (std::map<QString,QString>::const_iterator i = envVars.begin(); i != envVars.end(); ++i)
    {
	val.value = i->second;
	val.item = new QTreeWidgetItem(envList, QStringList() << i->first << i->second);
	m_envVars[i->first] = val;
    }

    envList->setAllColumnsShowFocus(true);
    buttonDelete->setEnabled(envList->currentItem() != 0);
}

PgmArgs::~PgmArgs()
{
}

// initializes the selected options
void PgmArgs::setOptions(const QSet<QString>& selectedOptions)
{
    if (xsldbgOptionsPage == 0)
	return;

    for (int i = 0; i < xsldbgOptions->count(); i++) {
	if (selectedOptions.contains(xsldbgOptions->item(i)->text()))
	{
	    xsldbgOptions->item(i)->setSelected(true);
	}
    }
}

// returns the selected options
QSet<QString> PgmArgs::options() const
{
    QSet<QString> sel;
    if (xsldbgOptionsPage != 0)
    {
	for (int i = 0; i < xsldbgOptions->count(); i++) {
	    if (xsldbgOptions->item(i)->isSelected())
		sel.insert(xsldbgOptions->item(i)->text());
	}
    }
    return sel;
}

// this is a slot
void PgmArgs::on_buttonModify_clicked()
{
    modifyVar(true);	// re-add deleted entries
}

void PgmArgs::modifyVar(bool resurrect)
{
    QString name, value;
    parseEnvInput(name, value);
    if (name.isEmpty() || name.indexOf(' ') >= 0)	// disallow spaces in names
	return;

    // lookup the value in the dictionary
    EnvVar* val;
    std::map<QString,EnvVar>::iterator i = m_envVars.find(name);
    if (i != m_envVars.end()) {
	val = &i->second;
	// see if this is a zombie
	if (val->status == EnvVar::EVdeleted) {
	    // resurrect
	    if (resurrect)
	    {
		val->value = value;
		val->status = EnvVar::EVdirty;
		val->item = new QTreeWidgetItem(envList, QStringList() << name << value);
	    }
	} else if (value != val->value) {
	    // change the value
	    val->value = value;
	    val->status = EnvVar::EVdirty;
	    val->item->setText(1, value);
	}
    } else {
	// add the value
	val = &m_envVars[name];
	val->value = value;
	val->status = EnvVar::EVnew;
	val->item = new QTreeWidgetItem(envList, QStringList() << name << value);
    }
    envList->setCurrentItem(val->item);
    buttonDelete->setEnabled(true);
}

// delete the selected item
void PgmArgs::on_buttonDelete_clicked()
{
    QTreeWidgetItem* item = envList->currentItem();
    if (item == 0)
	return;
    QString name = item->text(0);

    // lookup the value in the dictionary
    std::map<QString,EnvVar>::iterator i = m_envVars.find(name);
    if (i != m_envVars.end())
    {
	EnvVar* val = &i->second;
	// delete from list
	val->item = 0;
	// if this is a new item, delete it completely, otherwise zombie-ize it
	if (val->status == EnvVar::EVnew) {
	    m_envVars.erase(i);
	} else {
	    // mark value deleted
	    val->status = EnvVar::EVdeleted;
	}
    }
    delete item;
    // there is no selected item anymore
    buttonDelete->setEnabled(false);
}

void PgmArgs::parseEnvInput(QString& name, QString& value)
{
    // parse input from edit field
    QString input = envVar->text();
    int equalSign = input.indexOf('=');
    if (equalSign >= 0) {
	name = input.left(equalSign).trimmed();
	value = input.mid(equalSign+1);
    } else {
	name = input.trimmed();
	value = QString();		/* value is empty */
    }
}

void PgmArgs::on_envList_currentItemChanged()
{
    QTreeWidgetItem* item = envList->currentItem();
    buttonDelete->setEnabled(item != 0);
    if (item == 0)
	return;

    // must get name from list box
    QString name = item->text(0);
    envVar->setText(name + "=" + m_envVars[name].value);
}

void PgmArgs::accept()
{
    // simulate that the Modify button was pressed, but don't revive
    // dead entries even if the user changed the edit box
    modifyVar(false);
    QDialog::accept();
}

void PgmArgs::on_wdBrowse_clicked()
{
    // browse for the working directory
    QString newDir = KFileDialog::getExistingDirectory(wd(), this);
    if (!newDir.isEmpty()) {
	setWd(newDir);
    }
}

void PgmArgs::on_insertFile_clicked()
{
    QString caption = i18n("Select a file name to insert as program argument");

    // use the selection as default
    QString f = programArgs->selectedText();
    f = KFileDialog::getSaveFileName(f, QString::null,
				     this, caption);
    // don't clear the selection if no file was selected
    if (!f.isEmpty()) {
	programArgs->insert(f);
    }
}

void PgmArgs::on_insertDir_clicked()
{
    QString caption = i18n("Select a directory to insert as program argument");

    // use the selection as default
    QString f = programArgs->selectedText();
    f = KFileDialog::getExistingDirectory(f, this, caption);
    // don't clear the selection if no file was selected
    if (!f.isEmpty()) {
	programArgs->insert(f);
    }
}

#include "pgmargs.moc"
