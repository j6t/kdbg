// $Id$

// Copyright by Judin Max, Johannes Sixt
// This file is under GPL, the GNU General Public Licence

#include <qheader.h>
#if QT_VERSION >= 200
#include <kglobal.h>
#include <klocale.h>			/* i18n */
#include <kiconloader.h>
#include <qfontdialog.h>
#include <qmessagebox.h>
#else
#include <kapp.h>			/* i18n */
#endif
#include <qpopmenu.h>
#include <ctype.h>
#include "regwnd.h"


RegisterViewItem::RegisterViewItem(RegisterView* parent, QString reg, QString value) :
	QListViewItem(parent, parent->m_lastInsert),
	m_reg(reg),
	m_changes(false),
	m_found(true)
{
    parent->m_lastInsert = this;
    setValue(value);
    setText(0, reg);
}

RegisterViewItem::~RegisterViewItem()
{
    RegisterView* r = static_cast<RegisterView*>(listView());
    if (r->m_lastInsert == this) {
	r->m_lastInsert = itemAbove();
    }
}

/*
 * We must be careful when converting the hex value because
 * it may exceed this computer's long values.
 */
inline int hexCharToDigit(char h)
{
    if (h < '0')
	return -1;
    if (h <= '9')
	return h - '0';
    if (h < 'A')
	return -1;
    if (h <= 'F')
	return h - ('A' - 10);
    if (h < 'a')
	return -1;
    if (h <= 'f')
	return h - ('a' - 10);
    return -1;
}
#if QT_VERSION < 200
#define DIGIT(c) hexCharToDigit(c)
#else
#define DIGIT(c) hexCharToDigit(c.latin1())
#endif

static QString toBinary(QString hex)
{
    static const char digits[16][8] = {
	"0000", "0001", "0010", "0011", "0100", "0101", "0110", "0111",
	"1000", "1001", "1010", "1011", "1100", "1101", "1110", "1111"
    };
    QString result;
    
    for (unsigned i = 2; i < hex.length(); i++) {
	int idx = DIGIT(hex[i]);
	if (idx < 0) {
	    // not a hex digit; no conversion
	    return hex;
	}
	const char* bindigits = digits[idx];
	result += bindigits;
    }
    // remove leading zeros
    switch (DIGIT(hex[2])) {
    case 0: case 1: result.remove(0, 3); break;
    case 2: case 3: result.remove(0, 2); break;
    case 4: case 5:
    case 6: case 7: result.remove(0, 1); break;
    }
    return result;
}

static QString toOctal(QString hex)
{
    QString result;
    int shift = 0;
    unsigned v = 0;
    for (int i = hex.length()-1; i >= 2; i--) {
	int idx = DIGIT(hex[i]);
	if (idx < 0)
	    return hex;
	v += idx << shift;
	result.insert(0, (v & 7) + '0');
	v >>= 3;
	shift++;
	if (shift == 3) {
	    // an extra digit this round
	    result.insert(0, v + '0');
	    shift = v = 0;
	}
    }
    if (v != 0) {
	result.insert(0, v + '0');
    }
    return "0" + result;
}

static QString toDecimal(QString hex)
{
    return hex;
}

#undef DIGIT

void RegisterViewItem::setValue(QString value)
{
    m_value = value;

    QString coded;
    QString decoded;

    /*
     * We split off the first token (separated by whitespace). If that one
     * is in hexadecimal notation, we allow to display it in different modes.
     * The rest of the value we treat as "decoded" value and display in
     * the third column.
     */
    int pos = value.find(' ');
    if (pos < 0) {
	coded = value;
	decoded = QString();
    } else {
	coded = value.left(pos);
	decoded = value.mid(pos+1,value.length()).stripWhiteSpace();
    }
    if (coded.length() > 2 && coded[0] == '0' && coded[1] == 'x') {
	switch (static_cast<RegisterView*>(listView())->m_mode) {
	case  2: coded = toBinary(coded); break;
	case  8: coded = toOctal(coded); break;
	case 10: coded = toDecimal(coded); break;
	default: /* no change */ break;
	}
    }
	
    setText(1, coded);
    setText(2, decoded);
}

void RegisterViewItem::paintCell(QPainter* p, const QColorGroup& cg,
				 int column, int width, int alignment)
{
    if (m_changes) {
#if QT_VERSION >= 200
	QColorGroup newcg = cg;
	newcg.setColor(QColorGroup::Text, red);
#else
	QColorGroup newcg(cg.foreground(), cg.background(),
			  cg.light(), cg.dark(), cg.mid(),
			  red, cg.base());
#endif
	QListViewItem::paintCell(p, newcg, column, width, alignment);
    } else {
	QListViewItem::paintCell(p, cg, column, width, alignment);
    }
}


RegisterView::RegisterView(QWidget* parent, const char* name) :
	QListView(parent, name),
	m_lastInsert(0),
	m_mode(16)
{
    setSorting(-1);

#if QT_VERSION < 200
    addColumn(i18n("Register"));
    addColumn(i18n("Value"));
    addColumn(i18n("Decoded value"));
#else
    QPixmap iconRegs = BarIcon("regs.xpm");
    QPixmap iconWatchcoded = BarIcon("watchcoded.xpm");
    QPixmap iconWatch = BarIcon("watch.xpm");

    addColumn(QIconSet(iconRegs), i18n("Register"));
    addColumn(QIconSet(iconWatchcoded), i18n("Value"));
    addColumn(QIconSet(iconWatch), i18n("Decoded value"));
#endif

    setColumnAlignment(0,AlignLeft);
    setColumnAlignment(1,AlignLeft);
    setColumnAlignment(2,AlignLeft);

    setAllColumnsShowFocus( true );
    header()->setClickEnabled(false);

    connect(this, SIGNAL(doubleClicked(QListViewItem*)),
	    SLOT(doubleClicked(QListViewItem*)));
    connect(this, SIGNAL(rightButtonClicked(QListViewItem*,const QPoint&,int)),
	    SLOT(rightButtonClicked(QListViewItem*,const QPoint&,int)));

    m_modemenu = new QPopupMenu;
    m_modemenu->insertItem(i18n("Binary"),0);
    m_modemenu->insertItem(i18n("Octal"),1);
    m_modemenu->insertItem(i18n("Decimal"),2);
    m_modemenu->insertItem(i18n("Hexadecimal"),3);
    connect(m_modemenu,SIGNAL(activated(int)),SLOT(slotModeChange(int)));

    m_menu = new QPopupMenu();
//    m_menu->setTitle(i18n("Register Setting"));
    m_menu->insertItem(i18n("Font..."), this, SLOT(slotSetFont()));
    m_menu->insertItem(i18n("View mode"), m_modemenu);

    resize(200,300);
}

RegisterView::~RegisterView()
{
}

void RegisterView::updateRegisters( const char* output )
{
    if ( strncmp(output,"The program has no registers now.",33) == 0 ){
	clear();
	return;
    }

    setUpdatesEnabled(false);

    // mark all items as 'not found'
    for (QListViewItem* i = firstChild(); i; i = i->nextSibling()) {
	static_cast<RegisterViewItem*>(i)->m_found = false;
    }

    QString reg;
    QString value;

    // parse register values
    while (*output != '\0')
    {
	// skip space at the start of the line
	while (isspace(*output))
	    output++;

	// register name
	const char* start = output;
	while (*output != '\0' && !isspace(*output))
	    output++;
	if ( *output == '\0' ) break;
#if QT_VERSION >= 200
	reg = QString::fromLatin1(start, output-start);
#else
	reg = QString(start, output-start+1);
#endif

	// skip space
	while (isspace(*output))
	    output++;
	// the rest of the line is the register value
	start = output;
	output = strchr(output,'\n');
#if QT_VERSION >= 200
	value = QString::fromLatin1(start, output  ?  output-start  :  -1);
#else
	if (output)
	    value = QString(start, output-start+1);
	else
	    value = QString(start);
#endif

	// check if this is a new register
	bool found = false;
	for (QListViewItem* i = firstChild(); i; i = i->nextSibling()) {
	    RegisterViewItem* it = static_cast<RegisterViewItem*>(i);
	    if (it->m_reg == reg) {
		found = true;
		it->m_found = true;
		if (it->m_value != value) {
		    it->m_changes = true;
		    it->setValue(value);
		    repaintItem(it);
		} else {
		    /*
		     * If there was a change last time, but not now, we
		     * must revert the color.
		     */
		    if (it->m_changes) {
			it->m_changes = false;
			repaintItem(it);
		    }
		}
		m_lastInsert = it;
	    }
	}
	if (!found)
	    new RegisterViewItem(this, reg, value);

	if (!output)
	    break;
	// skip '\n'
	output++;
    }

    // remove all 'not found' items;
    QList<QListViewItem> deletedItem;
    deletedItem.setAutoDelete(true);
    for (QListViewItem* i = firstChild(); i; i = i->nextSibling() ){
	RegisterViewItem* it = static_cast<RegisterViewItem*>(i);
	if (!it->m_found) {
	    deletedItem.append(it);
	}
    }
    deletedItem.clear();

    setUpdatesEnabled(true);
//    repaint();
    triggerUpdate();
}

void RegisterView::doubleClicked( QListViewItem* item )
{
#if QT_VERSION >= 200
    RegisterViewItem* it = static_cast<RegisterViewItem*>(item);
    QMessageBox::information(this, "work in progress...",
			     QString("Prepare for change register value\nregister: %1    current value: %2").arg(it->m_reg).arg(it->m_value));
#endif
}

void RegisterView::rightButtonClicked(QListViewItem* item, const QPoint& p, int c)
{
    m_modemenu->setItemChecked(0, m_mode ==  2);
    m_modemenu->setItemChecked(1, m_mode ==  8);
    m_modemenu->setItemChecked(2, m_mode == 10);
    m_modemenu->setItemChecked(3, m_mode == 16);
    m_menu->popup(p);
}

void RegisterView::slotSetFont()
{
#if QT_VERSION >= 200
    bool ok;
    QFont f = QFontDialog::getFont(&ok, font());
    if (ok) {
	setFont(f);
	header()->setFont(*KGlobal::_generalFont);
    }
#endif
}

void RegisterView::slotModeChange(int code)
{
    static const int modes[] = { 2, 8, 10, 16 };
    if (code < 0 || code >= 4 || m_mode == modes[code])
	return;

    m_mode = modes[code];

    for (QListViewItem* i = firstChild(); i; i = i->nextSibling()) {
	RegisterViewItem* it = static_cast<RegisterViewItem*>(i);
	it->setValue(it->m_value);
    }
}

#include "regwnd.moc"
