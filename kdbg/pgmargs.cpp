/*
 * Copyright Johannes Sixt
 * This file is licensed under the GNU General Public License Version 2.
 * See the file COPYING in the toplevel directory of the source directory.
 */

#include "pgmargs.h"
#include <qlabel.h>
#include <qpushbutton.h>
#include <qlistview.h>
#include <qlistbox.h>
#include <qtabwidget.h>
#include <kapplication.h>
#include <kfiledialog.h>
#include <klocale.h>			/* i18n */
#include "config.h"
#include "mydebug.h"

PgmArgs::PgmArgs(QWidget* parent, const QString& pgm, QDict<EnvVar>& envVars,
		 const QStringList& allOptions) :
	PgmArgsBase(parent, "pgmargs", true),
	m_envVars(envVars)
{
    m_envVars.setAutoDelete(false);

    {
	QFileInfo fi = pgm;
	QString newText = labelArgs->text().arg(fi.fileName());
	labelArgs->setText(newText);
    }

    // add options only if the option list is non-empty
    if (!allOptions.isEmpty()) 
    {
	xsldbgOptions->insertStringList(allOptions);
    }
    else
    {
	delete xsldbgOptionsPage;
	xsldbgOptionsPage = 0;
    }

    initEnvList();
}

PgmArgs::~PgmArgs()
{
}

// initializes the selected options
void PgmArgs::setOptions(const QStringList& selectedOptions)
{
    QStringList::ConstIterator it;
    for (it = selectedOptions.begin(); it != selectedOptions.end(); ++it) {
	for (uint i = 0; i < xsldbgOptions->count(); i++) {
	    if (xsldbgOptions->text(i) == *it) {
		xsldbgOptions->setSelected(i, true);
		break;
	    }
	}
    }
}

// returns the selected options
QStringList PgmArgs::options() const
{
    QStringList sel;
    if (xsldbgOptionsPage != 0)
    {
	for (uint i = 0; i < xsldbgOptions->count(); i++) {
	    if (xsldbgOptions->isSelected(i))
		sel.append(xsldbgOptions->text(i));
	}
    }
    return sel;
}

// this is a slot
void PgmArgs::modifyVar()
{
    modifyVar(true);	// re-add deleted entries
}

void PgmArgs::modifyVar(bool resurrect)
{
    QString name, value;
    parseEnvInput(name, value);
    if (name.isEmpty() || name.find(' ') >= 0)	/* disallow spaces in names */
	return;

    // lookup the value in the dictionary
    EnvVar* val = m_envVars[name];
    if (val != 0) {
	// see if this is a zombie
	if (val->status == EnvVar::EVdeleted) {
	    // resurrect
	    if (resurrect)
	    {
		val->value = value;
		val->status = EnvVar::EVdirty;
		val->item = new QListViewItem(envList, name, value);	// inserts itself
		m_envVars.insert(name, val);
	    }
	} else if (value != val->value) {
	    // change the value
	    val->value = value;
	    val->status = EnvVar::EVdirty;
	    val->item->setText(1, value);
	}
    } else {
	// add the value
	val = new EnvVar;
	val->value = value;
	val->status = EnvVar::EVnew;
	val->item = new QListViewItem(envList, name, value);	// inserts itself
	m_envVars.insert(name, val);
    }
    envList->setSelected(val->item, true);
    buttonDelete->setEnabled(true);
}

// delete the selected item
void PgmArgs::deleteVar()
{
    QListViewItem* item = envList->selectedItem();
    if (item == 0)
	return;
    QString name = item->text(0);

    // lookup the value in the dictionary
    EnvVar* val = m_envVars[name];
    if (val != 0)
    {
	// delete from list
	val->item = 0;
	// if this is a new item, delete it completely, otherwise zombie-ize it
	if (val->status == EnvVar::EVnew) {
	    m_envVars.remove(name);
	    delete val;
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
    int equalSign = input.find('=');
    if (equalSign >= 0) {
	name = input.left(equalSign).stripWhiteSpace();
	value = input.mid(equalSign+1);
    } else {
	name = input.stripWhiteSpace();
	value = QString();		/* value is empty */
    }
}

void PgmArgs::initEnvList()
{
    QDictIterator<EnvVar> it = m_envVars;
    EnvVar* val;
    QString name;
    for (; (val = it) != 0; ++it) {
	val->status = EnvVar::EVclean;
	name = it.currentKey();
	val->item = new QListViewItem(envList, name, val->value);	// inserts itself
    }

    envList->setAllColumnsShowFocus(true);
    buttonDelete->setEnabled(envList->selectedItem() != 0);
}

void PgmArgs::envListCurrentChanged()
{
    QListViewItem* item = envList->selectedItem();
    buttonDelete->setEnabled(item != 0);
    if (item == 0)
	return;

    // must get name from list box
    QString name = item->text(0);
    EnvVar* val = m_envVars[name];
    ASSERT(val != 0);
    if (val != 0) {
	envVar->setText(name + "=" + val->value);
    } else {
	envVar->setText(name);
    }
}

void PgmArgs::accept()
{
    // simulate that the Modify button was pressed, but don't revive
    // dead entries even if the user changed the edit box
    modifyVar(false);
    QDialog::accept();
}

void PgmArgs::browseWd()
{
    // browse for the working directory
    QString newDir = KFileDialog::getExistingDirectory(wd(), this);
    if (!newDir.isEmpty()) {
	setWd(newDir);
    }
}

void PgmArgs::browseArgFile()
{
    QString caption = i18n("Select a file name to insert as program argument");

    // use the selection as default
    QString f = programArgs->markedText();
    f = KFileDialog::getSaveFileName(f, QString::null,
				     this, caption);
    // don't clear the selection if no file was selected
    if (!f.isEmpty()) {
	programArgs->insert(f);
    }
}

void PgmArgs::browseArgDir()
{
    QString caption = i18n("Select a directory to insert as program argument");

    // use the selection as default
    QString f = programArgs->markedText();
    f = KFileDialog::getExistingDirectory(f, this, caption);
    // don't clear the selection if no file was selected
    if (!f.isEmpty()) {
	programArgs->insert(f);
    }
}

void PgmArgs::invokeHelp()
{
    kapp->invokeHTMLHelp("kdbg/argspwdenv.html");
}

#include "pgmargs.moc"
