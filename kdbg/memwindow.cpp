/*
 * Copyright Johannes Sixt
 * This file is licensed under the GNU General Public License Version 2.
 * See the file COPYING in the toplevel directory of the source directory.
 */

#include "memwindow.h"
#include <QFontDatabase>
#include <QHeaderView>
#include <QMouseEvent>
#include <QScrollBar>
#include <QList>
#include <klocale.h>
#include <kconfigbase.h>
#include <kconfiggroup.h>
#include "debugger.h"

const int COL_ADDR = 0;
const int COL_DUMP_ASCII = 9;
const int MAX_COL = 10;

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
    for (int i = 0; i < MAX_COL; i++) {
	m_memory.header()->setSectionResizeMode(i, QHeaderView::Fixed);
    }
    m_memory.header()->setStretchLastSection(false);

    m_memory.setSortingEnabled(false);		/* don't sort */
    m_memory.setAllColumnsShowFocus(true);
    m_memory.setRootIsDecorated(false);
    m_memory.header()->setSectionsClickable(false);
    m_memory.header()->setSectionsMovable(false);  // don't move columns
    m_memory.setHeaderHidden(true);                // hide header
    m_memory.setContextMenuPolicy(Qt::NoContextMenu);	// defer to parent

    // get row height
    new QTreeWidgetItem(&m_memory, QStringList("0x179bf"));
    m_memoryRowHeight = m_memory.visualRect(m_memory.indexAt(QPoint(0, 0))).height();
    m_memory.clear();

    // create layout
    m_layout.setSpacing(2);
    m_layout.addWidget(&m_expression, 0);
    m_layout.addWidget(&m_memory, 10);
    m_layout.activate();

    connect(&m_expression, SIGNAL(activated(const QString&)),
	    this, SLOT(slotNewExpression(const QString&)));
    connect(m_expression.lineEdit(), SIGNAL(returnPressed()),
	    this, SLOT(slotNewExpression()));
    connect(m_memory.verticalScrollBar(), SIGNAL(valueChanged(int)),
	    this, SLOT(verticalScrollBarMoved(int)));
    connect(m_memory.verticalScrollBar(), SIGNAL(rangeChanged(int, int)),
	    this, SLOT(verticalScrollBarRangeChanged(int, int)));

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

void MemoryWindow::requestMemoryDump(const QString &expr)
{
    /*
     * Memory dump same length for all cases
     * Length of memory dump to request is n_rows (window size / row_height) * column_size
     * Column size
     *     Byte      8
     *     Halfword  8
     *     Word      4
     *     Giantword 2
     * Strings and instructions format are column size 1
     */
    int nrows = m_memory.height() / m_memoryRowHeight;
    unsigned int colum_size = 1;
    unsigned int format = m_format & MDTformatmask;

    if (m_dumpMemRegionEnd) {
        return;
    }

    /* Move to driver ???? */
    if (!(format == MDTinsn || format == MDTstring)) {
        switch(m_format & MDTsizemask) {
            case MDTbyte:
            case MDThalfword:
                colum_size= 8;
                break;
            case MDTword:
                colum_size= 4;
                break;
            case MDTgiantword:
                colum_size= 2;
                break;
        }
    }
    unsigned request_length = nrows * colum_size * 2;
    m_dumpLength += request_length;
    m_debugger->setMemoryExpression(m_expression.lineEdit()->text(), m_dumpLength, expr, request_length);
}

void MemoryWindow::verticalScrollBarRangeChanged(int min, int max)
{
    if (min == 0 && max == 0 && !m_dumpLastAddr.isEmpty()) {
        requestMemoryDump(m_dumpLastAddr.asString());
    }
}

void MemoryWindow::verticalScrollBarMoved(int value)
{
    int scrollmax = m_memory.verticalScrollBar()->maximum();
    if (value == scrollmax && !m_dumpLastAddr.isEmpty()) {
        requestMemoryDump(m_dumpLastAddr.asString());
    }
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
    m_expression.setCurrentIndex(0);

    if (!text.isEmpty()) {
	m_formatCache[text] = m_format;
    }

    displayNewExpression(text);
}

void MemoryWindow::displayNewExpression(const QString& expr)
{
    // Clear variables
    m_dumpMemRegionEnd = false;
    m_dumpLastAddr = DbgAddr{};
    m_dumpLength = 0;

    if (m_memory.verticalScrollBar())
        m_memory.verticalScrollBar()->setValue(0);

    m_memory.clear();
    m_memoryColumnsWidth.clear();
    for (int i  = 0; i < MAX_COL; i++) {
        m_memoryColumnsWidth.append(0);
    }

    requestMemoryDump(expr);
    m_expression.setEditText(expr);
}

void MemoryWindow::slotTypeChange(QAction* action)
{
    int id = action->data().toInt();

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

QString MemoryWindow::parseMemoryDumpLineToAscii(const QString& line, bool littleendian)
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
	    QByteArray dump;
	    for (int s  = 0; s < len; s++)
	    {
		unsigned char b = parsedValue & 0xFF;
		parsedValue >>= 8;
		if (isprint(b)) {
		    dump += b;
		} else {
		    dump += '.';
		}
	    }
	    if (!littleendian)
		std::reverse(dump.begin(), dump.end());
	    dumpAscii += QString::fromLatin1(dump);
	}
    }
    return dumpAscii;
}

void MemoryWindow::slotNewMemoryDump(const QString& msg, const std::list<MemoryDump>& memdump)
{
    QFontMetrics fm(font());

    if (!msg.isEmpty()) {
	for (int i = MAX_COL; i > 0; i--) {
	    m_memory.setColumnHidden(i, true);
	}
	new QTreeWidgetItem(&m_memory, QStringList() << msg);
	m_memory.header()->resizeSection(COL_ADDR, fm.width(msg)+10);
	return;
    }

    bool showDumpAscii = (m_format & MDTformatmask) == MDThex;

    std::list<MemoryDump>::const_iterator md = memdump.begin();

    // show only needed columns
    QStringList sl = md->dump.split( "\t" );
    for (int i = COL_DUMP_ASCII-1; i > 0; i--)
	m_memory.setColumnHidden(i, i > sl.count());

    m_memory.setColumnHidden(COL_DUMP_ASCII, !showDumpAscii);

    for (; md != memdump.end(); ++md)
    {
	QString addr = md->address.asString() + " " + md->address.fnoffs;
	QStringList sl = md->dump.split( "\t" );

	if (fm.width(addr) > m_memoryColumnsWidth[COL_ADDR]) {
	    m_memoryColumnsWidth[COL_ADDR] = fm.width(addr);
	}

	QTreeWidgetItem* line = nullptr;
	QList<QTreeWidgetItem*> items = m_memory.findItems(addr, Qt::MatchExactly, 0);
	if (items.count() != 0) {
	    line = items.at(0);
	}
	if (!line) {	 // line not found in memory view, append new one
	    for (int i = 0; i < sl.count(); i++) {
		if (fm.width(sl[i]) > m_memoryColumnsWidth[i+1]) {
		    m_memoryColumnsWidth[i+1] = fm.width(sl[i]);
		}
	    }

	    line = new QTreeWidgetItem(&m_memory, QStringList(addr) << sl);
	    m_dumpLastAddr = md->address;
	    if (md->endOfDump)
		m_dumpMemRegionEnd = true;
	} else {	// line found in memory view updated it
	    for (int i = 0; i < sl.count(); i++) {
		if (fm.width(sl[i]) > m_memoryColumnsWidth[i+1]) {
		    m_memoryColumnsWidth[i+1] = fm.width(sl[i]);
		}
		bool changed = i < (line->columnCount() - showDumpAscii) && sl[i] != line->text(i+1);
		line->setForeground(i+1, changed ? QBrush(QColor(Qt::red)) : palette().text());
		line->setText(i+1, sl[i]);
	    }
	}

	if (showDumpAscii) {
	    QString dumpAscii = parseMemoryDumpLineToAscii(md->dump, md->littleendian);
	    /*
	     * Add space padding to have always same number of chars in all lines.
	     * Workaround necessary to display correctly aligned when Qt::AlignRight.
	     */
	    int dumpAsciiFixedLen = ((m_format & MDTsizemask) == MDTbyte) ? 8:16;
	    if (dumpAscii.size() < dumpAsciiFixedLen) {
		dumpAscii += QString().sprintf("%*c", dumpAsciiFixedLen- dumpAscii.size(), ' ');
	    }
		line->setText(COL_DUMP_ASCII, dumpAscii);
	    line->setTextAlignment(COL_DUMP_ASCII, Qt::AlignRight);
	    if (fm.width(dumpAscii) > m_memoryColumnsWidth[COL_DUMP_ASCII]) {
		m_memoryColumnsWidth[COL_DUMP_ASCII] = fm.width(dumpAscii);
	    }
	}
    }

    // resize to longest string and add padding
    int padding = 25;
    m_memory.header()->resizeSection(COL_ADDR, m_memoryColumnsWidth[COL_ADDR] + padding);
    m_memory.header()->resizeSection(COL_DUMP_ASCII, m_memoryColumnsWidth[COL_DUMP_ASCII] + padding);
    for (int i = 1; i < COL_DUMP_ASCII; i++) {
        m_memory.header()->resizeSection(i, m_memoryColumnsWidth[i] + 10);
    }
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
