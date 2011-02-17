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
#include <QFileInfo>
#include <QPainter>
#include <QLabel>
#include <QBitmap>
#include <QPixmap>

#include <QMouseEvent>
#include "debugger.h"
#include "brkpt.h"
#include "dbgdriver.h"
#include <ctype.h>
#include <list>
#include "mydebug.h"

#include "ui_brkptcondition.h"



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
    bpText = bpText.trimmed();
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
    wpExpr = wpExpr.trimmed();
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
private:
      Ui::BrkPtCondition m_ui;

public:
    ConditionalDlg(QWidget* parent);
    ~ConditionalDlg();

    void setCondition(const QString& text) { m_ui.condition->setText(text); }
    QString condition() { return m_ui.condition->text(); }
    void setIgnoreCount(uint count){ m_ui.ignoreCount->setValue(count); };
    uint ignoreCount(){ return m_ui.ignoreCount->value(); };
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
    QPixmap brkena = UserIcon("brkena");
    QPixmap brkdis = UserIcon("brkdis");
    QPixmap watchena = UserIcon("watchena");
    QPixmap watchdis = UserIcon("watchdis");
    QPixmap brktmp = UserIcon("brktmp");
    QPixmap brkcond = UserIcon("brkcond");
    QPixmap brkorph = UserIcon("brkorph");

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
	QString file = QFileInfo(fileName).fileName();
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
	QDialog(parent)
{
    m_ui.setupUi(this);
    QString title = KGlobal::caption();
    title += i18n(": Conditional breakpoint");
    setWindowTitle(title);
}

ConditionalDlg::~ConditionalDlg()
{
}


#include "brkpt.moc"
