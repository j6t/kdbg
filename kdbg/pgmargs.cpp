// $Id$

// Copyright by Johannes Sixt
// This file is under GPL, the GNU General Public Licence

#include "pgmargs.h"
#include <kapp.h>
#include <kfiledialog.h>
#include <klocale.h>			/* i18n */
#include "config.h"
#include "mydebug.h"

PgmArgs::PgmArgs(QWidget* parent, const QString& pgm, QDict<EnvVar>& envVars,
		 const QStringList& allOptions) :
	QDialog(parent, "pgmargs", true),
	m_envVars(envVars),
	m_label(this, "label"),
	m_programArgs(this, "args"),
	m_fileBrowse(this, "args_browse"),
	m_optionsLabel(this, "options_label"),
	m_options(this, "options"),
	m_wdLabel(this, "wd_label"),
	m_wd(this, "wd"),
	m_wdBrowse(this, "wd_browse"),
	m_envLabel(this, "env_label"),
	m_envVar(this, "env_var"),
	m_envList(this, "env_list"),
	m_buttonOK(this, "ok"),
	m_buttonCancel(this, "cancel"),
	m_buttonModify(this, "modify"),
	m_buttonDelete(this, "delete"),
	m_layout(this, 8),
	m_edits(8),
	m_buttons(8),
	m_pgmArgsEdit(0),
	m_wdEdit(0)
{
    m_envVars.setAutoDelete(false);

    QString title = kapp->caption();
    title += i18n(": Program arguments");
    setCaption(title);

    QString lab = i18n("Run %1 with these arguments:");
    {
	QFileInfo fi = pgm;
	m_label.setText(lab.arg(fi.fileName()));
    }
    QSize s = m_label.sizeHint();
    /* don't make the label too wide if pgm name is very long */
    if (s.width() > 450)
	s.setWidth(450);
    m_label.setMinimumSize(s);

    s = m_programArgs.sizeHint();
    m_programArgs.setMinimumSize(s);
    m_programArgs.setMaxLength(10000);

    m_fileBrowse.setText("...");
    s = m_fileBrowse.sizeHint();
    m_fileBrowse.setMinimumSize(s);
    connect(&m_fileBrowse, SIGNAL(clicked()), SLOT(browseArgs()));

    int btnSpace = 0, numSpace = 0;    /* used to shift env buttons down */

    // add options only if the option list is non-empty
    if (!allOptions.isEmpty()) 
    {
	m_optionsLabel.setText(i18n("Options:"));
	s = m_optionsLabel.sizeHint();
	m_optionsLabel.setMinimumSize(s);
	btnSpace += s.height();

	m_options.setSelectionMode(QListBox::Multi);

	// add 5 entries, then get size hint, then add the rest
	QStringList rest = allOptions;
	for (int i = 0; i < 5 && !rest.isEmpty(); i++) {
	    m_options.insertItem(rest.first());
	    rest.remove(rest.begin());
	}
	s = m_options.sizeHint();
	m_options.insertStringList(rest);

	m_options.setMinimumSize(s);
	m_options.setMaximumHeight(s.height());
	btnSpace += s.height();
	numSpace += 2;
    }

    m_wdLabel.setText(i18n("Working directory:"));
    s = m_wdLabel.sizeHint();
    m_wdLabel.setMinimumSize(s);
    btnSpace += s.height();

    m_wdBrowse.setText("...");
    s = m_wdBrowse.sizeHint();
    m_wdBrowse.setMinimumSize(s);
    connect(&m_wdBrowse, SIGNAL(clicked()), SLOT(browseWd()));

    s = m_wd.sizeHint();
    m_wd.setMinimumSize(s);
    m_wd.setMaxLength(10000);
    btnSpace += s.height();

    m_envLabel.setText(i18n("Environment variables (NAME=value):"));
    s = m_envLabel.sizeHint();
    m_envLabel.setMinimumSize(s);

    s = m_envVar.sizeHint();
    m_envVar.setMinimumSize(s);
    m_envVar.setMaxLength(10000);

    m_envList.setMinimumSize(330, 40);
    m_envList.addColumn(i18n("Name"), 100);
    m_envList.addColumn(i18n("Value"), 260);
    connect(&m_envList, SIGNAL(selectionChanged(QListViewItem*)),
	    SLOT(envListCurrentChanged(QListViewItem*)));
    initEnvList();

    m_buttonOK.setText(i18n("OK"));
    m_buttonOK.setDefault(true);
    s = m_buttonOK.sizeHint();
    m_buttonOK.setMinimumSize(s);
    connect(&m_buttonOK, SIGNAL(clicked()), SLOT(accept()));

    m_buttonCancel.setText(i18n("Cancel"));
    s = m_buttonCancel.sizeHint();
    m_buttonCancel.setMinimumSize(s);
    connect(&m_buttonCancel, SIGNAL(clicked()), SLOT(reject()));

    m_buttonModify.setText(i18n("&Modify"));
    s = m_buttonModify.sizeHint();
    m_buttonModify.setMinimumSize(s);
    connect(&m_buttonModify, SIGNAL(clicked()), SLOT(modifyVar()));

    m_buttonDelete.setText(i18n("&Delete"));
    s = m_buttonDelete.sizeHint();
    m_buttonDelete.setMinimumSize(s);
    connect(&m_buttonDelete, SIGNAL(clicked()), SLOT(deleteVar()));

    m_layout.addLayout(&m_edits, 10);
    m_layout.addLayout(&m_buttons);
    m_edits.addWidget(&m_label);
    m_edits.addLayout(&m_pgmArgsEdit);
    if (!allOptions.isEmpty()) {
	m_edits.addWidget(&m_optionsLabel);
	m_edits.addWidget(&m_options);
    } else {
	m_optionsLabel.hide();
	m_options.hide();
	m_options.setEnabled(false);
    }
    m_edits.addWidget(&m_wdLabel);
    m_edits.addLayout(&m_wdEdit);
    m_edits.addWidget(&m_envLabel);
    m_edits.addWidget(&m_envVar);
    m_edits.addWidget(&m_envList, 10);
    m_buttons.addWidget(&m_buttonOK);
    m_buttons.addWidget(&m_buttonCancel);
    m_buttons.addSpacing(btnSpace + numSpace*m_edits.defaultBorder());
    m_buttons.addWidget(&m_buttonModify);
    m_buttons.addWidget(&m_buttonDelete);
    m_buttons.addStretch(10);
    m_pgmArgsEdit.addWidget(&m_programArgs, 10);
    m_pgmArgsEdit.addWidget(&m_fileBrowse);
    m_wdEdit.addWidget(&m_wd, 10);
    m_wdEdit.addWidget(&m_wdBrowse);

    m_programArgs.setFocus();
}

PgmArgs::~PgmArgs()
{
}

// initializes the selected options
void PgmArgs::setOptions(const QStringList& selectedOptions)
{
    QStringList::ConstIterator it;
    for (it = selectedOptions.begin(); it != selectedOptions.end(); ++it) {
	for (uint i = 0; i < m_options.count(); i++) {
	    if (m_options.text(i) == *it) {
		m_options.setSelected(i, true);
		break;
	    }
	}
    }
}

// returns the selected options
QStringList PgmArgs::options() const
{
    QStringList sel;
    for (uint i = 0; i < m_options.count(); i++) {
	if (m_options.isSelected(i))
	    sel.append(m_options.text(i));
    }
    return sel;
}

void PgmArgs::modifyVar()
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
	    val->value = value;
	    val->status = EnvVar::EVdirty;
	    val->item = new QListViewItem(&m_envList, name, value);	// inserts itself
	    m_envVars.insert(name, val);
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
	val->item = new QListViewItem(&m_envList, name, value);	// inserts itself
	m_envVars.insert(name, val);
    }
    m_envList.setSelected(val->item, true);
    m_buttonDelete.setEnabled(true);
}

// delete the selected item
void PgmArgs::deleteVar()
{
    QListViewItem* item = m_envList.selectedItem();
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
    m_buttonDelete.setEnabled(false);
}

void PgmArgs::parseEnvInput(QString& name, QString& value)
{
    // parse input from edit field
    QString input = m_envVar.text();
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
	val->item = new QListViewItem(&m_envList, name, val->value);	// inserts itself
    }

    m_envList.setAllColumnsShowFocus(true);
    m_buttonDelete.setEnabled(m_envList.selectedItem() != 0);
}

void PgmArgs::envListCurrentChanged(QListViewItem* item)
{
    m_buttonDelete.setEnabled(item != 0);
    if (item == 0)
	return;

    // must get name from list box
    QString name = item->text(0);
    EnvVar* val = m_envVars[name];
    ASSERT(val != 0);
    if (val != 0) {
	m_envVar.setText(name + "=" + val->value);
    } else {
	m_envVar.setText(name);
    }
}

void PgmArgs::accept()
{
    modifyVar();
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

void PgmArgs::browseArgs()
{
    QString caption = i18n("Select a file name to insert as program argument");

    // use the selection as default
    QString f = m_programArgs.markedText();
    f = KFileDialog::getSaveFileName(f, QString::null,
				     this, caption);
    // don't clear the selection of no file was selected
    if (!f.isEmpty()) {
	m_programArgs.insert(f);
    }
}

#include "pgmargs.moc"
