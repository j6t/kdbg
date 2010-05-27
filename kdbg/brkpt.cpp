/*
 * Copyright Johannes Sixt
 * This file is licensed under the GNU General Public License Version 2.
 * See the file COPYING in the toplevel directory of the source directory.
 */

#include <kglobal.h>
#include <klocale.h>			/* i18n */
#include <kiconloader.h>
#include <ksimpleconfig.h>
#include <QDialog>
#include <QPainter>
#include <QLabel>
#include <QBitmap>
#include <QPixmap>
#include <Q3GridLayout>
#include <Q3VBoxLayout>
#include <Q3HBoxLayout>

#include <QMouseEvent>
#include "debugger.h"
#include "brkpt.h"
#include "dbgdriver.h"
#include <ctype.h>
#include <list>
#include "mydebug.h"


class BreakpointItem : public QTreeWidgetItem, public Breakpoint
{
public:
    BreakpointItem(QTreeWidget* list, const Breakpoint& bp);
    void updateFrom(const Breakpoint& bp);
    void display();			/* sets icon and visible texts */
    bool enabled() const { return Breakpoint::enabled; }
};


BreakpointTable::BreakpointTable(QWidget* parent) :
	QWidget(parent),
	m_debugger(0)
{
    m_ui.setupUi(this);
    connect(m_ui.bpEdit, SIGNAL(returnPressed()),
	    this, SLOT(on_btAddBP_clicked()));

    initListAndIcons();

    connect(m_ui.bpList, SIGNAL(currentItemChanged(QTreeWidgetItem*,QTreeWidgetItem*)),
	    this, SLOT(updateUI()));
    // double click on item is same as View code
    connect(m_ui.bpList,SIGNAL(itemDoubleClicked(QTreeWidgetItem*,int)),
	    this, SLOT(on_btViewCode_clicked()));

    // need mouse button events
    m_ui.bpList->viewport()->installEventFilter(this);
}

BreakpointTable::~BreakpointTable()
{
}

void BreakpointTable::updateBreakList()
{
    std::list<BreakpointItem*> deletedItems;

    for (int i = 0 ; i < m_ui.bpList->topLevelItemCount(); i++)
    {
        deletedItems.push_back(static_cast<BreakpointItem*>(m_ui.bpList->topLevelItem(i)));
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
	new BreakpointItem(m_ui.bpList,*bp);
nextItem:;
    }

    // delete all untouched breakpoints
    while (!deletedItems.empty()) {
	delete deletedItems.front();
	deletedItems.pop_front();
    }
}

BreakpointItem::BreakpointItem(QTreeWidget* list, const Breakpoint& bp) :
	QTreeWidgetItem(list),
	Breakpoint(bp)
{
    display();
}

void BreakpointItem::updateFrom(const Breakpoint& bp)
{
    Breakpoint::operator=(bp);		/* assign new values */
    display();
}

void BreakpointTable::on_btAddBP_clicked()
{
    // set a breakpoint at the specified text
    QString bpText = m_ui.bpEdit->text();
    bpText = bpText.stripWhiteSpace();
    if (m_debugger->isReady())
    {
	Breakpoint* bp = new Breakpoint;
	bp->text = bpText;

	m_debugger->setBreakpoint(bp, false);
    }
}

void BreakpointTable::on_btAddWP_clicked()
{
    // set a watchpoint for the specified expression
    QString wpExpr = m_ui.bpEdit->text();
    wpExpr = wpExpr.stripWhiteSpace();
    if (m_debugger->isReady()) {
	Breakpoint* bp = new Breakpoint;
	bp->type = Breakpoint::watchpoint;
	bp->text = wpExpr;

	m_debugger->setBreakpoint(bp, false);
    }
}

void BreakpointTable::on_btRemove_clicked()
{
    BreakpointItem* bp = static_cast<BreakpointItem*>(m_ui.bpList->currentItem());
    if (bp != 0) {
	m_debugger->deleteBreakpoint(bp->id);
	// note that bp may be deleted by now
	// (if bp was an orphaned breakpoint)
    }
}

void BreakpointTable::on_btEnaDis_clicked()
{
    BreakpointItem* bp = static_cast<BreakpointItem*>(m_ui.bpList->currentItem());
    if (bp != 0) {
	m_debugger->enableDisableBreakpoint(bp->id);
    }
}

void BreakpointTable::on_btViewCode_clicked()
{
    BreakpointItem* bp = static_cast<BreakpointItem*>(m_ui.bpList->currentItem());
    if (bp == 0)
	return;

    if (!m_debugger->infoLine(bp->fileName, bp->lineNo, bp->address))
	emit activateFileLine(bp->fileName, bp->lineNo, bp->address);
}

void BreakpointTable::updateUI()
{
    bool enableChkpt = m_debugger->canChangeBreakpoints();
    m_ui.btAddBP->setEnabled(enableChkpt);
    m_ui.btAddWP->setEnabled(enableChkpt);

    BreakpointItem* bp = static_cast<BreakpointItem*>(m_ui.bpList->currentItem());
    m_ui.btViewCode->setEnabled(bp != 0);

    if (bp == 0) {
	enableChkpt = false;
    } else {
	if (bp->enabled()) {
	    m_ui.btEnaDis->setText(i18n("&Disable"));
	} else {
	    m_ui.btEnaDis->setText(i18n("&Enable"));
	}
    }
    m_ui.btRemove->setEnabled(enableChkpt);
    m_ui.btEnaDis->setEnabled(enableChkpt);
    m_ui.btConditional->setEnabled(enableChkpt);
}

bool BreakpointTable::eventFilter(QObject* ob, QEvent* ev)
{
    if (ev->type() == QEvent::MouseButtonPress)
    {
	QMouseEvent* mev = static_cast<QMouseEvent*>(ev);
	if (mev->button() == Qt::MidButton) {
	    // enable or disable the clicked-on item
	    BreakpointItem* bp =
		static_cast<BreakpointItem*>(m_ui.bpList->itemAt(mev->pos()));
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
    Q3VBoxLayout m_layout;
    Q3GridLayout m_inputs;
    Q3HBoxLayout m_buttons;
};

void BreakpointTable::on_btConditional_clicked()
{
    BreakpointItem* bp = static_cast<BreakpointItem*>(m_ui.bpList->currentItem());
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
    m_ui.bpList->setColumnWidth(0, 220);
    m_ui.bpList->setColumnWidth(1, 65);
    m_ui.bpList->setColumnWidth(2, 30);
    m_ui.bpList->setColumnWidth(3, 30);
    m_ui.bpList->setColumnWidth(4, 200);

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
	    p.fillRect(0,0, canvas.width(),canvas.height(), Qt::cyan);
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
	m_icons[i] = QIcon(canvas);
    }
}

void BreakpointItem::display()
{
    BreakpointTable* lb = static_cast<BreakpointTable*>(treeWidget()->parent());

    /* breakpoint icon code; keep order the same as in BreakpointTable::initListAndIcons */
    int code = enabled() ? 1 : 0;
    if (temporary)
	code += 2;
    if (!condition.isEmpty() || ignoreCount > 0)
	code += 4;
    if (Breakpoint::type == watchpoint)
	code += 8;
    if (isOrphaned())
	code += 16;
    setIcon(0, lb->m_icons[code]);

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
    QString title = KGlobal::caption();
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
