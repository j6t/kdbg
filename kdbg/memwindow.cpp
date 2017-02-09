/*
 * Copyright Johannes Sixt
 * This file is licensed under the GNU General Public License Version 2.
 * See the file COPYING in the toplevel directory of the source directory.
 */

#include "memwindow.h"
#include <QFontDatabase>
#include <QHeaderView>
#include <QMouseEvent>
#include <QList>
#include <klocale.h>
#include <kconfigbase.h>
#include <kconfiggroup.h>
#include "debugger.h"

const int COL_ADDR = 0;
const int COL_DUMP_ASCII = 9;

MemoryWindow::MemoryWindow(QWidget* parent) :
	QWidget(parent),
	m_debugger(0),
	m_expression(this),
	m_memory(this),
	m_layout(QBoxLayout::TopToBottom, this),
	m_format(MDTword | MDThex)
{
    setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));

    m_expression.setEditable(true);
    QSize minSize = m_expression.sizeHint();
    m_expression.setMinimumSize(minSize);
    m_expression.setInsertPolicy(QComboBox::NoInsert);
    m_expression.setMaxCount(15);

    m_memory.setColumnCount(10);
    m_memory.setHeaderLabel(i18n("Address"));
    m_memory.header()->setSectionResizeMode(COL_ADDR, QHeaderView::Fixed);
    for (int i = 1; i < 10; i++) {
	m_memory.hideColumn(i);
	m_memory.header()->setSectionResizeMode(i, QHeaderView::ResizeToContents);
	m_memory.headerItem()->setText(i, QString());
    }
    m_memory.header()->setSectionResizeMode(COL_DUMP_ASCII, QHeaderView::Fixed);
    m_memory.header()->setStretchLastSection(false);

    m_memory.setSortingEnabled(false);		/* don't sort */
    m_memory.setAllColumnsShowFocus(true);
    m_memory.setRootIsDecorated(false);
    m_memory.header()->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_memory.header()->setSectionsClickable(false);
    m_memory.header()->setSectionsMovable(false);  // don't move columns
    m_memory.setContextMenuPolicy(Qt::NoContextMenu);	// defer to parent

    // create layout
    m_layout.setSpacing(2);
    m_layout.addWidget(&m_expression, 0);
    m_layout.addWidget(&m_memory, 10);
    m_layout.activate();

    connect(&m_expression, SIGNAL(activated(const QString&)),
	    this, SLOT(slotNewExpression(const QString&)));
    connect(m_expression.lineEdit(), SIGNAL(returnPressed()),
	    this, SLOT(slotNewExpression()));

    // the popup menu
    QAction* pAction;
    pAction = m_popup.addAction(i18n("B&ytes"));
    pAction->setData(MDTbyte);
    pAction = m_popup.addAction(i18n("Halfwords (&2 Bytes)"));
    pAction->setData(MDThalfword);
    pAction = m_popup.addAction(i18n("Words (&4 Bytes)"));
    pAction->setData(MDTword);
    pAction = m_popup.addAction(i18n("Giantwords (&8 Bytes)"));
    pAction->setData(MDTgiantword);
    m_popup.addSeparator();
    pAction = m_popup.addAction(i18n("He&xadecimal"));
    pAction->setData(MDThex);
    pAction = m_popup.addAction(i18n("Signed &decimal"));
    pAction->setData(MDTsigned);
    pAction = m_popup.addAction(i18n("&Unsigned decimal"));
    pAction->setData(MDTunsigned);
    pAction = m_popup.addAction(i18n("&Octal"));
    pAction->setData(MDToctal);
    pAction = m_popup.addAction(i18n("&Binary"));
    pAction->setData(MDTbinary);
    pAction = m_popup.addAction(i18n("&Addresses"));
    pAction->setData(MDTaddress);
    pAction = m_popup.addAction(i18n("&Character"));
    pAction->setData(MDTchar);
    pAction = m_popup.addAction(i18n("&Floatingpoint"));
    pAction->setData(MDTfloat);
    pAction = m_popup.addAction(i18n("&Strings"));
    pAction->setData(MDTstring);
    pAction = m_popup.addAction(i18n("&Instructions"));
    pAction->setData(MDTinsn);
    connect(&m_popup, SIGNAL(triggered(QAction*)), this, SLOT(slotTypeChange(QAction*)));
}

MemoryWindow::~MemoryWindow()
{
}

void MemoryWindow::contextMenuEvent(QContextMenuEvent* ev)
{
    m_popup.popup(ev->globalPos());
    ev->accept();
}

void MemoryWindow::slotNewExpression()
{
    slotNewExpression(m_expression.lineEdit()->text());
}

void MemoryWindow::slotNewExpression(const QString& newText)
{
    QString text = newText.simplified();

    // see if the string is in the list
    // (note: must count downwards because of removeItem!)
    for (int i = m_expression.count()-1; i >= 0; i--)
    {
	if (m_expression.itemText(i) == text) {
	    // yes it is!
	    // look up the format that was used last time for this expr
	    QMap<QString,unsigned>::iterator pFormat = m_formatCache.find(text);
	    if (pFormat != m_formatCache.end()) {
		m_format = *pFormat;
		m_debugger->setMemoryFormat(m_format);
	    }
	    // remove this text, will be inserted at the top
	    m_expression.removeItem(i);
	}
    }
    m_expression.insertItem(0, text);

    if (!text.isEmpty()) {
	m_formatCache[text] = m_format;
    }

    displayNewExpression(text);
}

void MemoryWindow::displayNewExpression(const QString& expr)
{
    m_debugger->setMemoryExpression(expr);
    m_expression.setEditText(expr);

    // clear memory dump if no dump wanted
    if (expr.isEmpty()) {
	m_memory.clear();
	m_old_memory.clear();
    }
}

void MemoryWindow::slotTypeChange(QAction* action)
{
    int id = action->data().toInt();

    m_old_memory.clear();

    // compute new type
    if (id & MDTsizemask)
	m_format = (m_format & ~MDTsizemask) | id;
    if (id & MDTformatmask)
	m_format = (m_format & ~MDTformatmask) | id;
    m_debugger->setMemoryFormat(m_format);

    // change the format in the cache
    QString expr = m_expression.currentText();
    m_formatCache[expr.simplified()] = m_format;

    // force redisplay
    displayNewExpression(expr);
}

QString MemoryWindow::parseMemoryDumpLineToAscii(const QString& line)
{
    QStringList hexdata = line.split("\t");

    // Get the size of value from hex str representation length
    //               0x00 =   (4 - 2) / 2 -> 1 byte
    //             0x0000 =   (6 - 2) / 2 -> 2 half-word
    //         0x00000000 =  (10 - 2) / 2 -> 4 word
    // 0x0000000000000000 =  (18 - 2) / 2 -> 8 giant word

    int len = (hexdata[0].length()-2) / 2;
    QString dumpAscii;
    for (const auto& hex: hexdata)
    {
	bool ok;
	unsigned long long parsedValue = hex.toULongLong(&ok, 16);
	if (ok) {
	    for (int s  = 0; s < len; s++)
	    {
		unsigned char b = parsedValue & 0xFF;
		parsedValue >>= 8;
		if (isprint(b)) {
		    dumpAscii += b;
		} else {
		    dumpAscii += '.';
		}
	    }
	}
    }
    return dumpAscii;
}

void MemoryWindow::slotNewMemoryDump(const QString& msg, const std::list<MemoryDump>& memdump)
{
    m_memory.clear();
    if (!msg.isEmpty()) {
	new QTreeWidgetItem(&m_memory, QStringList() << QString() << msg);
	return;
    }
    bool showDumpAscii = (m_format & MDTformatmask) == MDThex;

    std::list<MemoryDump>::const_iterator md = memdump.begin();

    // show only needed columns
    QStringList sl = md->dump.split( "\t" );
    for (int i = COL_DUMP_ASCII-1; i > 0; i--)
	m_memory.setColumnHidden(i, i > sl.count());

    m_memory.setColumnHidden(COL_DUMP_ASCII, !showDumpAscii);

    QMap<QString,QString> tmpMap;

    QFontMetrics fm(font());
    int maxWidthAddr = 0;
    int maxWidthAscii = 0;

    for (; md != memdump.end(); ++md)
    {
	QString addr = md->address.asString() + " " + md->address.fnoffs;
	QStringList sl = md->dump.split( "\t" );

	if (fm.width(addr) > maxWidthAddr) {
	    maxWidthAddr = fm.width(addr);
	}

	// save memory
	tmpMap[addr] = md->dump;

	QTreeWidgetItem* line =
		new QTreeWidgetItem(&m_memory, QStringList(addr) << sl);

	if (showDumpAscii) {
	    QString dumpAscii = parseMemoryDumpLineToAscii(md->dump);
	    line->setText(COL_DUMP_ASCII, dumpAscii);
	    line->setTextAlignment(COL_DUMP_ASCII, Qt::AlignRight);
	    if (fm.width(dumpAscii) > maxWidthAscii) {
		maxWidthAscii = fm.width(dumpAscii);
	    }
	}

	QStringList tmplist;
	QMap<QString,QString>::Iterator pos = m_old_memory.find( addr );

	if( pos != m_old_memory.end() )
	    tmplist = pos.value().split( "\t" );

	for (int i = 0; i < sl.count(); i++)
	{
	    bool changed =
		i < tmplist.count() &&
		sl[i] != tmplist[i];
	    line->setForeground(i+1, changed ? QBrush(QColor(Qt::red)) : palette().text());
	}
    }

    // resize to longest string and add padding to addr/dumpascii columns
    int padding = 25;
    m_memory.header()->resizeSection(COL_ADDR, maxWidthAddr + padding);
    m_memory.header()->resizeSection(COL_DUMP_ASCII, maxWidthAscii + padding);

    m_old_memory.clear();
    m_old_memory = tmpMap;
}

static const char MemoryGroup[] = "Memory";
static const char NumExprs[] = "NumExprs";
static const char ExpressionFmt[] = "Expression%d";
static const char FormatFmt[] = "Format%d";
static const char ColumnWidths[] = "ColumnWidths";

void MemoryWindow::saveProgramSpecific(KConfigBase* config)
{
    KConfigGroup g = config->group(MemoryGroup);

    int numEntries = m_expression.count();
    g.writeEntry(NumExprs, numEntries);
    QString exprEntry;
    QString fmtEntry;
    for (int i = 0; i < numEntries;) {
	QString text = m_expression.itemText(i);
	i++;				/* entries are counted 1-based */
	exprEntry.sprintf(ExpressionFmt, i);
	fmtEntry.sprintf(FormatFmt, i);
	g.writeEntry(exprEntry, text);
	QMap<QString,unsigned>::iterator pFormat = m_formatCache.find(text);
	unsigned fmt = pFormat != m_formatCache.end()  ?  *pFormat  :  MDTword | MDThex;
	g.writeEntry(fmtEntry, fmt);
    }

    // column widths
    QList<int> widths;
    for (int i = 0; i < 2; i++) {
	int w = m_memory.columnWidth(i);
	widths.append(w);
    }
    g.writeEntry(ColumnWidths, widths);
}

void MemoryWindow::restoreProgramSpecific(KConfigBase* config)
{
    KConfigGroup g = config->group(MemoryGroup);

    int numEntries = g.readEntry(NumExprs, 0);
    m_expression.clear();

    QString exprEntry;
    QString fmtEntry;
    // entries are counted 1-based
    for (int i = 1; i <= numEntries; i++) {
	exprEntry.sprintf(ExpressionFmt, i);
	fmtEntry.sprintf(FormatFmt, i);
	QString expr = g.readEntry(exprEntry, QString());
	unsigned fmt = g.readEntry(fmtEntry, MDTword | MDThex);
	m_expression.addItem(expr);
	m_formatCache[expr] = fmt & (MDTsizemask | MDTformatmask);
    }

    // initialize with top expression
    if (numEntries > 0) {
	m_expression.setCurrentIndex(0);
	QString expr = m_expression.itemText(0);
	m_format = m_formatCache[expr];
	m_debugger->setMemoryFormat(m_format);
	displayNewExpression(expr);
    }

    // column widths
    QList<int> widths = g.readEntry(ColumnWidths, QList<int>());
    QList<int>::iterator w = widths.begin();
    for (int i = 0; i < 2 && w != widths.end(); ++i, ++w) {
	m_memory.setColumnWidth(i, *w);
    }
}


#include "memwindow.moc"
