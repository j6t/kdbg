/*
 * Copyright Johannes Sixt
 * This file is licensed under the GNU General Public License Version 2.
 * See the file COPYING in the toplevel directory of the source directory.
 */

#include <kapplication.h>
#include <klocale.h>			/* i18n */
#include <kiconloader.h>
#include <ksimpleconfig.h>
#include <qdialog.h>
#include <qkeycode.h>
#include <qpainter.h>
#include <qlabel.h>
#include <qbitmap.h>
#include "debugger.h"
#include "brkpt.h"
#include "dbgdriver.h"
#include <ctype.h>
#include <list>
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include "mydebug.h"


class BreakpointItem : public QListViewItem, public Breakpoint
{
public:
    BreakpointItem(QListView* list, const Breakpoint& bp);
    void updateFrom(const Breakpoint& bp);
    void display();			/* sets icon and visible texts */
    bool enabled() const { return Breakpoint::enabled; }
};


BreakpointTable::BreakpointTable(QWidget* parent, const char* name) :
	QWidget(parent, name),
	m_debugger(0),
	m_bpEdit(this, "bpedit"),
	m_list(this, "bptable"),
	m_btAddBP(this, "addbp"),
	m_btAddWP(this, "addwp"),
	m_btRemove(this, "remove"),
	m_btEnaDis(this, "enadis"),
	m_btViewCode(this, "view"),
	m_btConditional(this, "conditional"),
	m_layout(this, 8),
	m_listandedit(8),
	m_buttons(8)
{
    m_bpEdit.setMinimumSize(m_bpEdit.sizeHint());
    connect(&m_bpEdit, SIGNAL(returnPressed()), this, SLOT(addBP()));

    initListAndIcons();
    connect(&m_list, SIGNAL(currentChanged(QListViewItem*)), SLOT(updateUI()));
    // double click on item is same as View code
    connect(&m_list, SIGNAL(doubleClicked(QListViewItem*)), this, SLOT(viewBP()));

    // need mouse button events
    m_list.viewport()->installEventFilter(this);

    m_btAddBP.setText(i18n("Add &Breakpoint"));
    m_btAddBP.setMinimumSize(m_btAddBP.sizeHint());
    connect(&m_btAddBP, SIGNAL(clicked()), this, SLOT(addBP()));

    m_btAddWP.setText(i18n("Add &Watchpoint"));
    m_btAddWP.setMinimumSize(m_btAddWP.sizeHint());
    connect(&m_btAddWP, SIGNAL(clicked()), this, SLOT(addWP()));

    m_btRemove.setText(i18n("&Remove"));
    m_btRemove.setMinimumSize(m_btRemove.sizeHint());
    connect(&m_btRemove, SIGNAL(clicked()), this, SLOT(removeBP()));

    // the Enable/Disable button changes its label
    m_btEnaDis.setText(i18n("&Disable"));
    // make a dummy button to get the size of the alternate label
    {
	QSize size = m_btEnaDis.sizeHint();
	QPushButton dummy(this);
	dummy.setText(i18n("&Enable"));
	QSize sizeAlt = dummy.sizeHint();
	if (sizeAlt.width() > size.width())
	    size.setWidth(sizeAlt.width());
	if (sizeAlt.height() > size.height())
	    size.setHeight(sizeAlt.height());
	m_btEnaDis.setMinimumSize(size);
    }
    connect(&m_btEnaDis, SIGNAL(clicked()), this, SLOT(enadisBP()));

    m_btViewCode.setText(i18n("&View Code"));
    m_btViewCode.setMinimumSize(m_btViewCode.sizeHint());
    connect(&m_btViewCode, SIGNAL(clicked()), this, SLOT(viewBP()));

    m_btConditional.setText(i18n("&Conditional..."));
    m_btConditional.setMinimumSize(m_btConditional.sizeHint());
    connect(&m_btConditional, SIGNAL(clicked()), this, SLOT(conditionalBP()));

    m_layout.addLayout(&m_listandedit, 10);
    m_layout.addLayout(&m_buttons);
    m_listandedit.addWidget(&m_bpEdit);
    m_listandedit.addWidget(&m_list, 10);
    m_buttons.addWidget(&m_btAddBP);
    m_buttons.addWidget(&m_btAddWP);
    m_buttons.addWidget(&m_btRemove);
    m_buttons.addWidget(&m_btEnaDis);
    m_buttons.addWidget(&m_btViewCode);
    m_buttons.addWidget(&m_btConditional);
    m_buttons.addStretch(10);

    m_layout.activate();

    resize(350, 300);

    m_bpEdit.setFocus();
}

BreakpointTable::~BreakpointTable()
{
}

void BreakpointTable::updateBreakList()
{
    std::list<BreakpointItem*> deletedItems;

    for (QListViewItem* it = m_list.firstChild(); it != 0; it = it->nextSibling()) {
	deletedItems.push_back(static_cast<BreakpointItem*>(it));
    }

    // get the new list
    for (KDebugger::BrkptROIterator bp = m_debugger->breakpointsBegin(); bp != m_debugger->breakpointsEnd(); ++bp)
    {
	// look up this item
	for (std::list<BreakpointItem*>::iterator o = deletedItems.begin(); o != deletedItems.end(); ++o)
	{
	    if ((*o)->id == bp->id) {
		(*o)->updateFrom(*bp);
		deletedItems.erase(o);	/* don't delete */
		goto nextItem;
	    }
	}
	// not in the list; add it
	new BreakpointItem(&m_list, *bp);
nextItem:;
    }

    // delete all untouched breakpoints
    while (!deletedItems.empty()) {
	delete deletedItems.front();
	deletedItems.pop_front();
    }
}

BreakpointItem::BreakpointItem(QListView* list, const Breakpoint& bp) :
	QListViewItem(list),
	Breakpoint(bp)
{
    display();
}

void BreakpointItem::updateFrom(const Breakpoint& bp)
{
    Breakpoint::operator=(bp);		/* assign new values */
    display();
}

void BreakpointTable::addBP()
{
    // set a breakpoint at the specified text
    QString bpText = m_bpEdit.text();
    bpText = bpText.stripWhiteSpace();
    if (m_debugger->isReady())
    {
	Breakpoint* bp = new Breakpoint;
	bp->text = bpText;

	m_debugger->setBreakpoint(bp, false);
    }
}

void BreakpointTable::addWP()
{
    // set a watchpoint for the specified expression
    QString wpExpr = m_bpEdit.text();
    wpExpr = wpExpr.stripWhiteSpace();
    if (m_debugger->isReady()) {
	Breakpoint* bp = new Breakpoint;
	bp->type = Breakpoint::watchpoint;
	bp->text = wpExpr;

	m_debugger->setBreakpoint(bp, false);
    }
}

void BreakpointTable::removeBP()
{
    BreakpointItem* bp = static_cast<BreakpointItem*>(m_list.currentItem());
    if (bp != 0) {
	m_debugger->deleteBreakpoint(bp->id);
	// note that bp may be deleted by now
	// (if bp was an orphaned breakpoint)
    }
}

void BreakpointTable::enadisBP()
{
    BreakpointItem* bp = static_cast<BreakpointItem*>(m_list.currentItem());
    if (bp != 0) {
	m_debugger->enableDisableBreakpoint(bp->id);
    }
}

void BreakpointTable::viewBP()
{
    BreakpointItem* bp = static_cast<BreakpointItem*>(m_list.currentItem());
    if (bp == 0)
	return;

    emit activateFileLine(bp->fileName, bp->lineNo, bp->address);
}

void BreakpointTable::updateUI()
{
    bool enableChkpt = m_debugger->canChangeBreakpoints();
    m_btAddBP.setEnabled(enableChkpt);
    m_btAddWP.setEnabled(enableChkpt);

    BreakpointItem* bp = static_cast<BreakpointItem*>(m_list.currentItem());
    m_btViewCode.setEnabled(bp != 0);

    if (bp == 0) {
	enableChkpt = false;
    } else {
	if (bp->enabled()) {
	    m_btEnaDis.setText(i18n("&Disable"));
	} else {
	    m_btEnaDis.setText(i18n("&Enable"));
	}
    }
    m_btRemove.setEnabled(enableChkpt);
    m_btEnaDis.setEnabled(enableChkpt);
    m_btConditional.setEnabled(enableChkpt);
}

bool BreakpointTable::eventFilter(QObject* ob, QEvent* ev)
{
    if (ev->type() == QEvent::MouseButtonPress)
    {
	QMouseEvent* mev = static_cast<QMouseEvent*>(ev);
	if (mev->button() == MidButton) {
	    // enable or disable the clicked-on item
	    BreakpointItem* bp =
		static_cast<BreakpointItem*>(m_list.itemAt(mev->pos()));
	    if (bp != 0)
	    {
		m_debugger->enableDisableBreakpoint(bp->id);
	    }
	    return true;
	}
    }
    return QWidget::eventFilter(ob, ev);
}

class ConditionalDlg : public QDialog
{
public:
    ConditionalDlg(QWidget* parent);
    ~ConditionalDlg();

    void setCondition(const QString& text) { m_condition.setText(text); }
    QString condition() { return m_condition.text(); }
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
    BreakpointItem* bp = static_cast<BreakpointItem*>(m_list.currentItem());
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
    dlg.setCondition(bp->condition);
    dlg.setIgnoreCount(bp->ignoreCount);

    if (dlg.exec() != QDialog::Accepted)
	return;

    QString conditionInput = dlg.condition();
    int ignoreCount = dlg.ignoreCount();
    m_debugger->conditionalBreakpoint(id, conditionInput, ignoreCount);
}


void BreakpointTable::initListAndIcons()
{
    m_list.addColumn(i18n("Location"), 220);
    m_list.addColumn(i18n("Address"), 65);
    m_list.addColumn(i18n("Hits"), 30);
    m_list.addColumn(i18n("Ignore"), 30);
    m_list.addColumn(i18n("Condition"), 200);

    m_list.setMinimumSize(200, 100);

    m_list.setSorting(-1);

    // add pixmaps
    QPixmap brkena = UserIcon("brkena.xpm");
    QPixmap brkdis = UserIcon("brkdis.xpm");
    QPixmap watchena = UserIcon("watchena.xpm");
    QPixmap watchdis = UserIcon("watchdis.xpm");
    QPixmap brktmp = UserIcon("brktmp.xpm");
    QPixmap brkcond = UserIcon("brkcond.xpm");
    QPixmap brkorph = UserIcon("brkorph.xpm");

    /*
     * There are 32 different pixmaps: The basic enabled or disabled
     * breakpoint, plus an optional overlaid brktmp icon plus an optional
     * overlaid brkcond icon, plus an optional overlaid brkorph icon. Then
     * the same sequence for watchpoints.
     */
    m_icons.resize(32);
    QPixmap canvas(16,16);

    for (int i = 0; i < 32; i++) {
	{
	    QPainter p(&canvas);
	    // clear canvas
	    p.fillRect(0,0, canvas.width(),canvas.height(), cyan);
	    // basic icon
	    if (i & 1) {
		p.drawPixmap(1,1, (i & 8) ? watchena : brkena);
	    } else {
		p.drawPixmap(1,1, (i & 8) ? watchdis : brkdis);
	    }
	    // temporary overlay
	    if (i & 2) {
		p.drawPixmap(1,1, brktmp);
	    }
	    // conditional overlay
	    if (i & 4) {
		p.drawPixmap(1,1, brkcond);
	    }
	    // orphan overlay
	    if (i & 16) {
		p.drawPixmap(1,1, brkorph);
	    }
	}
	canvas.setMask(canvas.createHeuristicMask());
	m_icons[i] = canvas;
    }
}

void BreakpointItem::display()
{
    BreakpointTable* lb = static_cast<BreakpointTable*>(listView()->parent());

    /* breakpoint icon code; keep order the same as in BreakpointTable::initListAndIcons */
    int code = enabled() ? 1 : 0;
    if (temporary)
	code += 2;
    if (!condition.isEmpty() || ignoreCount > 0)
	code += 4;
    if (type == watchpoint)
	code += 8;
    if (isOrphaned())
	code += 16;
    setPixmap(0, lb->m_icons[code]);

    // more breakpoint info
    if (!location.isEmpty()) {
	setText(0, location);
    } else if (!Breakpoint::text.isEmpty()) {
	setText(0, Breakpoint::text);
    } else if (!fileName.isEmpty()) {
	// use only the file name portion
	QString file = fileName;
	int slash = file.findRev('/');
	if (slash >= 0) {
	    file = file.mid(slash+1);
	}
	// correct zero-based line-numbers
	setText(0, file + ":" + QString::number(lineNo+1));
    } else {
	setText(0, "*" + address.asString());
    }

    int c = 0;
    setText(++c, address.asString());
    QString tmp;
    if (hitCount == 0) {
	setText(++c, QString());
    } else {
	tmp.setNum(hitCount);
	setText(++c, tmp);
    }
    if (ignoreCount == 0) {
	setText(++c, QString());
    } else {
	tmp.setNum(ignoreCount);
	setText(++c, tmp);
    }
    if (condition.isEmpty()) {
	setText(++c, QString());
    } else {
	setText(++c, condition);
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
    QString title = kapp->caption();
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


#include "brkpt.moc"
