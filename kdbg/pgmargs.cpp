// $Id$

// Copyright by Johannes Sixt
// This file is under GPL, the GNU General Public Licence

#include "pgmargs.h"
#include <kapp.h>
#if QT_VERSION >= 200
#include <klocale.h>			/* i18n */
#endif
#include "mydebug.h"

PgmArgs::PgmArgs(QWidget* parent, const QString& pgm, QDict<EnvVar>& envVars) :
	QDialog(parent, "pgmargs", true),
	m_envVars(envVars),
	m_label(this, "label"),
	m_programArgs(this, "args"),
	m_wdLabel(this, "wd_label"),
	m_wd(this, "wd"),
	m_envLabel(this, "env_label"),
	m_envVar(this, "env_var"),
	m_envList(this, "env_list", 2),
	m_buttonOK(this, "ok"),
	m_buttonCancel(this, "cancel"),
	m_buttonModify(this, "modify"),
	m_buttonDelete(this, "delete"),
	m_layout(this, 8),
	m_edits(8),
	m_buttons(8)
{
    m_envVars.setAutoDelete(false);

    QString title = kapp->getCaption();
    title += i18n(": Program arguments");
    setCaption(title);

    QString fmt = i18n("Run %s with these arguments:");
#if QT_VERSION < 200
    QString lab(fmt.length() + pgm.length());
#else
    QString lab;
#endif
    lab.sprintf(fmt, pgm.data());
    m_label.setText(lab);
    QSize s = m_label.sizeHint();
    /* don't make the label too wide if pgm name is very long */
    if (s.width() > 450)
	s.setWidth(450);
    m_label.setMinimumSize(s);

    s = m_programArgs.sizeHint();
    m_programArgs.setMinimumSize(s);
    m_programArgs.setMaxLength(10000);

    m_wdLabel.setText(i18n("Working directory:"));
    s = m_wdLabel.sizeHint();
    m_wdLabel.setMinimumSize(s);
    int btnSpace = s.height();

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
    m_envList.setColumn(0, i18n("Name"), 100);
    m_envList.setColumn(1, i18n("Value"), 260);
    /* work around a non-feature of KTabListBox: */
    m_envList.setFocusPolicy(QWidget::StrongFocus);
    m_envList.setSeparator('\n');
    connect(&m_envList, SIGNAL(highlighted(int,int)),
	    SLOT(envListHighlighted(int,int)));
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
    m_edits.addWidget(&m_programArgs);
    m_edits.addWidget(&m_wdLabel);
    m_edits.addWidget(&m_wd);
    m_edits.addWidget(&m_envLabel);
    m_edits.addWidget(&m_envVar);
    m_edits.addWidget(&m_envList, 10);
    m_buttons.addWidget(&m_buttonOK);
    m_buttons.addWidget(&m_buttonCancel);
    m_buttons.addSpacing(btnSpace + 2*m_edits.defaultBorder());
    m_buttons.addWidget(&m_buttonModify);
    m_buttons.addWidget(&m_buttonDelete);
    m_buttons.addStretch(10);

    m_layout.activate();

    m_programArgs.setFocus();
    resize(300, 300);
}

PgmArgs::~PgmArgs()
{
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
	    val->idx = m_envList.count();
	    m_envList.appendItem(name + "\n" + value);
	    m_envVars.insert(name, val);
	} else if (value != val->value) {
	    // change the value
	    val->value = value;
	    val->status = EnvVar::EVdirty;
	    m_envList.changeItemPart(value, val->idx, 1);
	}
    } else {
	// add the value
	val = new EnvVar;
	val->value = value;
	val->status = EnvVar::EVnew;
	val->idx = m_envList.count();
	m_envList.appendItem(name + "\n" + value);
	m_envVars.insert(name, val);
    }
}

void PgmArgs::deleteVar()
{
    QString name, value;
    parseEnvInput(name, value);

    // lookup the value in the dictionary
    EnvVar* val = m_envVars[name];
    if (val == 0)
	return;

    // delete from list
    int idx = val->idx;
    m_envList.removeItem(idx);
    // if this is a new item, delete it completely, otherwise zombie-ize it
    if (val->status == EnvVar::EVnew) {
	m_envVars.remove(name);
	delete val;
    } else {
	// mark value deleted
	val->idx = -1;
	val->status = EnvVar::EVdeleted;
	// update the indices of the remaining items
	QDictIterator<EnvVar> it = m_envVars;
	while ((val = it) != 0) {
	    if (val->idx > idx) {
		--val->idx;
	    }
	    ++it;
	}
    }
    // clear the input
    m_envVar.setText("");
}

void PgmArgs::parseEnvInput(QString& name, QString& value)
{
    // parse input from edit field
    QString input = m_envVar.text();
    int equalSign = input.find('=');
    if (equalSign >= 0) {
	name = input.left(equalSign).stripWhiteSpace();
#if QT_VERSION < 200
	value = input.mid(equalSign+1,input.length());
#else
	value = input.mid(equalSign+1);
#endif
    } else {
	name = input.stripWhiteSpace();
	value = QString();		/* value is empty */
    }
}

void PgmArgs::initEnvList()
{
    QDictIterator<EnvVar> it = m_envVars;
    EnvVar* val;
    int i = 0;
    QString name;
    for (; (val = it) != 0; ++it) {
	val->idx = i++;
	val->status = EnvVar::EVclean;
	name = it.currentKey();
	m_envList.appendItem(name + "\n" + val->value);
    }
}

void PgmArgs::envListHighlighted(int idx, int /*col*/)
{
    // must get name from list box
    QString name = m_envList.text(idx, 0);
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

#include "pgmargs.moc"
