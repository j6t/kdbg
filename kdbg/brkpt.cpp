// $Id$

// Copyright by Johannes Sixt
// This file is under GPL, the GNU General Public Licence

#include <kapp.h>			/* i18n */
#if QT_VERSION >= 200
#include <klocale.h>			/* i18n */
#endif
#include <kiconloader.h>
#include <ksimpleconfig.h>
#include <qkeycode.h>
#include <qpainter.h>
#include <qlabel.h>
#include <qbitmap.h>
#include "debugger.h"			/* #includes brkpt.h */
#include "dbgdriver.h"
#include "brkpt.moc"
#include <ctype.h>
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include "mydebug.h"


BreakpointTable::BreakpointTable(KDebugger& deb) :
	QDialog(0, "breakpoints"),
	m_debugger(deb),
	m_bpEdit(this, "bpedit"),
	m_list(this, "bptable"),
	m_btAdd(this, "add"),
	m_btRemove(this, "remove"),
	m_btViewCode(this, "view"),
	m_btConditional(this, "conditional"),
	m_btClose(this, "close"),
	m_layout(this, 8),
	m_listandedit(8),
	m_buttons(8)
{
    m_bpEdit.setMinimumSize(m_bpEdit.sizeHint());
    connect(&m_bpEdit, SIGNAL(returnPressed()), this, SLOT(addBP()));

    // double click on item is same as View code
    connect(&m_list, SIGNAL(doubleClicked(QListViewItem*)), this, SLOT(viewBP()));

    m_btAdd.setText(i18n("&Add"));
    m_btAdd.setMinimumSize(m_btAdd.sizeHint());
    connect(&m_btAdd, SIGNAL(clicked()), this, SLOT(addBP()));

    m_btRemove.setText(i18n("&Remove"));
    m_btRemove.setMinimumSize(m_btRemove.sizeHint());
    connect(&m_btRemove, SIGNAL(clicked()), this, SLOT(removeBP()));

    m_btViewCode.setText(i18n("&View Code"));
    m_btViewCode.setMinimumSize(m_btViewCode.sizeHint());
    connect(&m_btViewCode, SIGNAL(clicked()), this, SLOT(viewBP()));

    m_btConditional.setText(i18n("&Conditional..."));
    m_btConditional.setMinimumSize(m_btConditional.sizeHint());
    connect(&m_btConditional, SIGNAL(clicked()), this, SLOT(conditionalBP()));

    m_btClose.setText(i18n("Close"));
    m_btClose.setMinimumSize(m_btClose.sizeHint());
    connect(&m_btClose, SIGNAL(clicked()), this, SLOT(hide()));

    m_layout.addLayout(&m_listandedit, 10);
    m_layout.addLayout(&m_buttons);
    m_listandedit.addWidget(&m_bpEdit);
    m_listandedit.addWidget(&m_list, 10);
    m_buttons.addWidget(&m_btAdd);
    m_buttons.addWidget(&m_btRemove);
    m_buttons.addWidget(&m_btViewCode);
    m_buttons.addWidget(&m_btConditional);
    m_buttons.addWidget(&m_btClose);
    m_buttons.addStretch(10);

    m_layout.activate();

    resize(350, 300);

    m_bpEdit.setFocus();
}

BreakpointTable::~BreakpointTable()
{
    // delete breakpoint objects
    for (int i = m_brkpts.size()-1; i >= 0; i--) {
	delete m_brkpts[i];
    }
}

void BreakpointTable::updateBreakList(const char* output)
{
    int i;

    // mark all breakpoints as being updated
    for (i = m_brkpts.size()-1; i >= 0; i--) {
	m_brkpts[i]->del = true;
    }

    // get the new list
    QList<Breakpoint> brks;
    brks.setAutoDelete(true);
    m_debugger.driver()->parseBreakList(output, brks);
    for (Breakpoint* bp = brks.first(); bp != 0; bp = brks.next()) {
	insertBreakpoint(bp->id, bp->temporary, bp->enabled, bp->location,
			 0, -1, bp->hitCount, bp->ignoreCount, bp->condition);
    }

    // delete all untouched breakpoints
    for (i = m_brkpts.size()-1; i >= 0; i--) {
	if (m_brkpts[i]->del) {
	    WndBreakpoint* bp = m_brkpts[i];
	    // delete the pointer and the object
	    for (uint j = i+1; j < m_brkpts.size(); j++) {
		m_brkpts[j-1] = m_brkpts[j];
	    }
	    m_brkpts.resize(m_brkpts.size()-1);
	    delete bp->listItem;
	    delete bp;
	}
    }
}

void BreakpointTable::insertBreakpoint(int num, const QString& fileName, int lineNo)
{
    SIZED_QString(str,fileName.length()+20);
    str.sprintf("%s:%d", fileName.data(), lineNo+1);
    insertBreakpoint(num, false, true,	/* keep, enabled */
		     str, fileName, lineNo);
}

void BreakpointTable::insertBreakpoint(int num, bool temp, bool enabled, QString location,
				       QString fileName, int lineNo,
				       int hits, uint ignoreCount, QString condition)
{
    assert(lineNo != 0);		/* line numbers are 1-based */
    TRACE("insert bp " + QString().setNum(num));
    int numBreaks = m_brkpts.size();
    WndBreakpoint* bp = breakpointById(num);
    if (bp == 0) {
	// not found, insert a new one
	bp = new WndBreakpoint;
	m_brkpts.resize(numBreaks+1);
	m_brkpts[numBreaks] = bp;
	bp->id = num;
	bp->lineNo = -1;
	bp->listItem = new QListViewItem(&m_list);
    }
    bp->temporary = temp;
    bp->enabled = enabled;
    bp->location = location;
    if (!fileName.isEmpty())
	bp->fileName = fileName;
    if (lineNo > 0)
	bp->lineNo = lineNo;
    bp->hitCount = hits;
    bp->ignoreCount = ignoreCount;
    bp->condition = condition;
    if (bp->conditionInput.isEmpty())
	bp->conditionInput = condition;
    bp->del = false;
    // update list
    m_list.changeItem(bp);
}

void BreakpointTable::closeEvent(QCloseEvent* ev)
{
    QDialog::closeEvent(ev);
    emit closed();
}

void BreakpointTable::hide()
{
    QDialog::hide();
    emit closed();
}

void BreakpointTable::addBP()
{
    // set a breakpoint at the specified text
    QString bpText = m_bpEdit.text();
    bpText = bpText.stripWhiteSpace();
    if (m_debugger.isReady()) {
	m_debugger.driver()->executeCmd(DCbreaktext, bpText);
	// clear text if successfully set
	m_bpEdit.setText("");
    }
}

void BreakpointTable::removeBP()
{
    WndBreakpoint* bp = breakpointByItem(m_list.currentItem());
    if (bp == 0)
	return;

    m_debugger.driver()->executeCmd(DCdelete, bp->id);
}

void BreakpointTable::viewBP()
{
    WndBreakpoint* bp = breakpointByItem(m_list.currentItem());
    if (bp == 0)
	return;

    emit activateFileLine(bp->fileName, bp->lineNo);
}

void BreakpointTable::updateUI()
{
    bool enableChkpt = m_debugger.canChangeBreakpoints();
    m_btAdd.setEnabled(enableChkpt);
    m_btRemove.setEnabled(enableChkpt);
    m_btConditional.setEnabled(enableChkpt);
}

class ConditionalDlg : public QDialog
{
public:
    ConditionalDlg(QWidget* parent);
    ~ConditionalDlg();

    void setCondition(const char* text) { m_condition.setText(text); }
    const char* condition() { return m_condition.text(); }
    void setIgnoreCount(uint count);
    uint ignoreCount();

protected:
    QLabel m_conditionLabel;
    QLineEdit m_condition;
    QLabel m_ignoreLabel;
    QLineEdit m_ignoreCount;
    QPushButton m_buttonOK;
    QPushButton m_buttonCancel;
    QVBoxLayout m_layout;
    QGridLayout m_inputs;
    QHBoxLayout m_buttons;
};

void BreakpointTable::conditionalBP()
{
    WndBreakpoint* bp = breakpointByItem(m_list.currentItem());
    if (bp == 0)
	return;

    /*
     * Important: we must not keep a pointer to the Breakpoint around,
     * since it may vanish while the modal dialog is open through other
     * user interactions (like clicking at the breakpoint in the source
     * window)!
     */
    int id = bp->id;

    ConditionalDlg dlg(this);
    dlg.setCondition(bp->conditionInput);
    dlg.setIgnoreCount(bp->ignoreCount);

    if (dlg.exec() != QDialog::Accepted)
	return;

    QString conditionInput = dlg.condition();
    int ignoreCount = dlg.ignoreCount();
    updateBreakpointCondition(id, conditionInput.simplifyWhiteSpace(),
			      ignoreCount);
}

// this handles the menu entry: toggles breakpoint on and off
void BreakpointTable::doBreakpoint(QString file, int lineNo, bool temporary)
{
    WndBreakpoint* bp = breakpointByFilePos(file, lineNo);
    if (bp == 0)
    {
	// no such breakpoint, so set a new one
	// strip off directory part of file name
#if QT_VERSION < 200
	file.detach();
#endif
	int offset = file.findRev("/");
	if (offset >= 0) {
	    file.remove(0, offset+1);
	}
	m_debugger.driver()->executeCmd(temporary ? DCtbreakline : DCbreakline,
					file, lineNo);
    }
    else
    {
	/*
	 * If the breakpoint is disabled, enable it; if it's enabled,
	 * delete that breakpoint.
	 */
	if (bp->enabled) {
	    m_debugger.driver()->executeCmd(DCdelete, bp->id);
	} else {
	    m_debugger.driver()->executeCmd(DCenable, bp->id);
	}
    }
}

void BreakpointTable::doEnableDisableBreakpoint(const QString& file, int lineNo)
{
    WndBreakpoint* bp = breakpointByFilePos(file, lineNo);
    if (bp == 0)
	return;

    // toggle enabled/disabled state
    if (bp->enabled) {
	m_debugger.driver()->executeCmd(DCdisable, bp->id);
    } else {
	m_debugger.driver()->executeCmd(DCenable, bp->id);
    }
}

void BreakpointTable::updateBreakpointCondition(int id,
						const QString& condition,
						int ignoreCount)
{
    WndBreakpoint* bp = breakpointById(id);
    if (bp == 0)
	return;				/* breakpoint no longer exists */

    bool changed = false;

    if (bp->conditionInput != condition) {
	// change condition
	m_debugger.driver()->executeCmd(DCcondition, condition, bp->id);
	bp->conditionInput = condition;
	changed = true;
    }
    if (bp->ignoreCount != ignoreCount) {
	// change ignore count
	m_debugger.driver()->executeCmd(DCignore, bp->id, ignoreCount);
	changed = true;
    }
    if (changed) {
	// get the changes
	m_debugger.driver()->queueCmd(DCinfobreak, DebuggerDriver::QMoverride);
    }
}



WndBreakpoint* BreakpointTable::breakpointById(int id)
{
    for (int i = m_brkpts.size()-1; i >= 0; i--) {
	if (m_brkpts[i]->id == id) {
	    return m_brkpts[i];
	}
    }
    return 0;
}

WndBreakpoint* BreakpointTable::breakpointByItem(QListViewItem* item)
{
    if (item == 0)
	return 0;

    for (int i = m_brkpts.size()-1; i >= 0; i--) {
	if (m_brkpts[i]->listItem == item) {
	    return m_brkpts[i];
	}
    }
    return 0;
}

WndBreakpoint* BreakpointTable::breakpointByFilePos(QString file, int lineNo)
{
    // look for exact file name match
    int i;
    for (i = m_brkpts.size()-1; i >= 0; i--) {
	if (m_brkpts[i]->lineNo == lineNo &&
	    m_brkpts[i]->fileName == file)
	{
	    return m_brkpts[i];
	}
    }
    // not found, so try basename
    // strip off directory part of file name
    int offset = file.findRev("/");
    if (offset < 0) {
	// that was already the basename, no need to scan the list again
	return 0;
    }
#if QT_VERSION < 200
    file.detach();
#endif
    file.remove(0, offset+1);

    for (i = m_brkpts.size()-1; i >= 0; i--) {
	if (m_brkpts[i]->lineNo == lineNo &&
	    m_brkpts[i]->fileName == file)
	{
	    return m_brkpts[i];
	}
    }

    // not found
    return 0;
}

// look if there is at least one temporary breakpoint
bool BreakpointTable::haveTemporaryBP() const
{
    for (int i = m_brkpts.size()-1; i >= 0; i--) {
	if (m_brkpts[i]->temporary)
	    return true;
    }
    return false;
}

/*
 * Breakpoints are saved one per group.
 */
const char BPGroup[] = "Breakpoint %d";
const char File[] = "File";
const char Line[] = "Line";
const char Temporary[] = "Temporary";
const char Enabled[] = "Enabled";
const char Condition[] = "Condition";

void BreakpointTable::saveBreakpoints(KSimpleConfig* config)
{
    QString groupName;
    int i;
    for (i = 0; uint(i) < m_brkpts.size(); i++) {
	groupName.sprintf(BPGroup, i);
	config->setGroup(groupName);
	WndBreakpoint* bp = m_brkpts[i];
	config->writeEntry(File, bp->fileName);
	config->writeEntry(Line, bp->lineNo);
	config->writeEntry(Temporary, bp->temporary);
	config->writeEntry(Enabled, bp->enabled);
	if (bp->condition.isEmpty())
	    config->deleteEntry(Condition, false);
	else
	    config->writeEntry(Condition, bp->condition);
	// we do not save the ignore count
    }
    // delete remaining groups
    // we recognize that a group is present if there is an Enabled entry
    for (;; i++) {
	groupName.sprintf(BPGroup, i);
	config->setGroup(groupName);
	if (!config->hasKey(Enabled)) {
	    /* group not present, assume that we've hit them all */
	    break;
	}
	config->deleteGroup(groupName);
    }
}

void BreakpointTable::restoreBreakpoints(KSimpleConfig* config)
{
    QString groupName;
    QString fileName;
    int lineNo;
    bool enabled, temporary;
    QString condition;
    /*
     * We recognize the end of the list if there is no Enabled entry
     * present.
     */
    for (int i = 0;; i++) {
	groupName.sprintf(BPGroup, i);
	config->setGroup(groupName);
	if (!config->hasKey(Enabled)) {
	    /* group not present, assume that we've hit them all */
	    break;
	}
	fileName = config->readEntry(File);
	lineNo = config->readNumEntry(Line, -1);
	if (lineNo < 0 || fileName.isEmpty())
	    continue;
	enabled = config->readBoolEntry(Enabled, true);
	temporary = config->readBoolEntry(Temporary, false);
	condition = config->readEntry(Condition);
	/*
	 * Add the breakpoint. We assume that we have started a new
	 * instance of gdb, because we assign the breakpoint ids ourselves,
	 * starting with 1. Then we use this id to disable the breakpoint,
	 * if necessary. If this assignment of ids doesn't work, (maybe
	 * because this isn't a fresh gdb at all), we disable the wrong
	 * breakpoint! Oh well... for now it works.
	 */
	m_debugger.driver()->executeCmd(temporary ? DCtbreakline : DCbreakline,
					fileName, lineNo);
	if (!enabled) {
	    m_debugger.driver()->executeCmd(DCdisable, i+1);
	}
	if (!condition.isEmpty()) {
	    m_debugger.driver()->executeCmd(DCcondition, condition, i+1);
	}

	SIZED_QString(location, fileName.length()+30);
	location.sprintf("%s:%d", fileName.data(), lineNo);
	insertBreakpoint(i+1, temporary, enabled,
			 location, fileName, lineNo-1, 0, 0, condition);
    }
}


BreakpointListBox::BreakpointListBox(QWidget* parent, const char* name) :
	QListView(parent, name)
{
    addColumn(i18n("Location"), 300);
    addColumn(i18n("Hits"), 30);
    addColumn(i18n("Ignore"), 30);
    addColumn(i18n("Condition"), 200);

    setMinimumSize(200, 100);
    /* work around a non-feature of KTabListBox: */
    setFocusPolicy(QWidget::StrongFocus);

    setSorting(-1);

    // add pixmaps
#if QT_VERSION < 200
    KIconLoader* loader = kapp->getIconLoader();
    QPixmap brkena = loader->loadIcon("brkena.xpm");
    QPixmap brkdis = loader->loadIcon("brkdis.xpm");
    QPixmap brktmp = loader->loadIcon("brktmp.xpm");
    QPixmap brkcond = loader->loadIcon("brkcond.xpm");
#else
    QPixmap brkena = BarIcon("brkena.xpm");
    QPixmap brkdis = BarIcon("brkdis.xpm");
    QPixmap brktmp = BarIcon("brktmp.xpm");
    QPixmap brkcond = BarIcon("brkcond.xpm");
#endif
    /*
     * There are 8 different pixmaps: The basic enabled or disabled
     * breakpoint, plus an optional overlaid brktmp icon plus an optional
     * overlaid brkcond icon.
     */
    m_icons.setSize(8);
    QPixmap canvas(16,16);

    for (int i = 0; i < 8; i++) {
	{
	    QPainter p(&canvas);
	    // clear canvas
	    p.fillRect(0,0, canvas.width(),canvas.height(), cyan);
	    // basic icon
	    if (i & 1) {
		p.drawPixmap(1,1, brkena);
	    } else {
		p.drawPixmap(1,1, brkdis);
	    }
	    // temporary overlay
	    if (i & 2) {
		p.drawPixmap(1,1, brktmp);
	    }
	    // conditional overlay
	    if (i & 4) {
		p.drawPixmap(1,1, brkcond);
	    }
	}
	canvas.setMask(canvas.createHeuristicMask());
	m_icons[i] = canvas;
    }
}

BreakpointListBox::~BreakpointListBox()
{
}

void BreakpointListBox::changeItem(WndBreakpoint* bp)
{
    /* breakpoint icon code; keep order the same as in this class's constructor */
    int code = bp->enabled ? 1 : 0;
    if (bp->temporary)
	code += 2;
    if (!bp->condition.isEmpty() || bp->ignoreCount > 0)
	code += 4;
    bp->listItem->setPixmap(0, m_icons[code]);

    // more breakpoint info
    bp->listItem->setText(0, bp->location);
    QString tmp;
    if (bp->hitCount == 0) {
	bp->listItem->setText(1, "");
    } else {
	tmp.setNum(bp->hitCount);
	bp->listItem->setText(1, tmp);
    }
    if (bp->ignoreCount == 0) {
	bp->listItem->setText(2, "");
    } else {
	tmp.setNum(bp->ignoreCount);
	bp->listItem->setText(2, tmp);
    }
    if (bp->condition.isEmpty()) {
	bp->listItem->setText(3, "");
    } else {
	bp->listItem->setText(3, bp->condition);
    }
}


ConditionalDlg::ConditionalDlg(QWidget* parent) :
	QDialog(parent, "conditional", true),
	m_conditionLabel(this, "condLabel"),
	m_condition(this, "condition"),
	m_ignoreLabel(this, "ignoreLabel"),
	m_ignoreCount(this, "ignoreCount"),
	m_buttonOK(this, "ok"),
	m_buttonCancel(this, "cancel"),
	m_layout(this, 10),
	m_inputs(2, 2, 10),
	m_buttons(4)
{
    QString title = kapp->getCaption();
    title += i18n(": Conditional breakpoint");
    setCaption(title);

    m_conditionLabel.setText(i18n("&Condition:"));
    m_conditionLabel.setMinimumSize(m_conditionLabel.sizeHint());
    m_ignoreLabel.setText(i18n("Ignore &next hits:"));
    m_ignoreLabel.setMinimumSize(m_ignoreLabel.sizeHint());

    m_condition.setMinimumSize(150, 24);
    m_condition.setMaxLength(10000);
    m_condition.setFrame(true);
    m_ignoreCount.setMinimumSize(150, 24);
    m_ignoreCount.setMaxLength(10000);
    m_ignoreCount.setFrame(true);

    m_conditionLabel.setBuddy(&m_condition);
    m_ignoreLabel.setBuddy(&m_ignoreCount);

    m_buttonOK.setMinimumSize(100, 30);
    connect(&m_buttonOK, SIGNAL(clicked()), SLOT(accept()));
    m_buttonOK.setText(i18n("OK"));
    m_buttonOK.setDefault(true);

    m_buttonCancel.setMinimumSize(100, 30);
    connect(&m_buttonCancel, SIGNAL(clicked()), SLOT(reject()));
    m_buttonCancel.setText(i18n("Cancel"));

    m_layout.addLayout(&m_inputs);
    m_inputs.addWidget(&m_conditionLabel, 0, 0);
    m_inputs.addWidget(&m_condition, 0, 1);
    m_inputs.addWidget(&m_ignoreLabel, 1, 0);
    m_inputs.addWidget(&m_ignoreCount, 1, 1);
    m_inputs.setColStretch(1, 10);
    m_layout.addLayout(&m_buttons);
    m_layout.addStretch(10);
    m_buttons.addStretch(10);
    m_buttons.addWidget(&m_buttonOK);
    m_buttons.addSpacing(40);
    m_buttons.addWidget(&m_buttonCancel);
    m_buttons.addStretch(10);

    m_layout.activate();

    m_condition.setFocus();
    resize(400, 100);
}

ConditionalDlg::~ConditionalDlg()
{
}

uint ConditionalDlg::ignoreCount()
{
    bool ok;
    QString input = m_ignoreCount.text();
    uint result = input.toUInt(&ok);
    return ok ? result : 0;
}

void ConditionalDlg::setIgnoreCount(uint count)
{
    QString text;
    // set empty if ignore count is zero
    if (count > 0) {
	text.setNum(count);
    }
    m_ignoreCount.setText(text);
}
