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
#include <stdlib.h>			/* strtoul */
#include "regwnd.h"
#include "dbgdriver.h"


class RegisterViewItem : public QListViewItem
{
public:
    RegisterViewItem(RegisterView* parent, QString reg,
		     QString raw, QString cooked);
    ~RegisterViewItem();

    void setValue(QString raw, QString cooked);
    RegisterInfo m_reg;
    bool m_changes;
    bool m_found;

protected:
    virtual void paintCell(QPainter*, const QColorGroup& cg,
			   int column, int width, int alignment);

};


RegisterViewItem::RegisterViewItem(RegisterView* parent, QString reg,
				   QString raw, QString cooked) :
	QListViewItem(parent, parent->m_lastInsert),
	m_changes(false),
	m_found(true)
{
    m_reg.regName = reg;
    parent->m_lastInsert = this;
    setValue(raw, cooked);
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
    /*
     * We convert only numbers that are small enough for this computer's
     * size of long integers.
     */
    if (hex.length() > sizeof(unsigned long)*2+2)	/*  count in leading "0x" */
	return hex;

#if QT_VERSION >= 200
    const char* start = hex.latin1();
#else
    const char* start = hex;
#endif
    char* end;
    unsigned long val = strtoul(start, &end, 0);
    if (start == end)
	return hex;
    else
	return QString().setNum(val);
}

#undef DIGIT

void RegisterViewItem::setValue(QString raw, QString cooked)
{
    m_reg.rawValue = raw;
    m_reg.cookedValue = cooked;

    /*
     * If the raw value is in hexadecimal notation, we allow to display it
     * in different modes (in the second column). The cooked value is
     * displayed in the third column.
     */
    if (raw.length() > 2 && raw[0] == '0' && raw[1] == 'x') {
	switch (static_cast<RegisterView*>(listView())->m_mode) {
	case  2: raw = toBinary(raw); break;
	case  8: raw = toOctal(raw); break;
	case 10: raw = toDecimal(raw); break;
	default: /* no change */ break;
	}
    }
	
    setText(1, raw);
    setText(2, cooked);
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


RegisterView::RegisterView(QWidget* parent, DebuggerDriver* driver, const char* name) :
	QListView(parent, name),
	m_lastInsert(0),
	m_mode(16),
	m_d(driver)
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
    QList<RegisterInfo> regs;
    m_d->parseRegisters(output, regs);

    if (regs.count() == 0) {
	clear();
	return;
    }

    setUpdatesEnabled(false);

    // mark all items as 'not found'
    for (QListViewItem* i = firstChild(); i; i = i->nextSibling()) {
	static_cast<RegisterViewItem*>(i)->m_found = false;
    }

    // parse register values
    while (regs.count() > 0)
    {
	RegisterInfo* reg = regs.take(0);

	// check if this is a new register
	bool found = false;
	for (QListViewItem* i = firstChild(); i; i = i->nextSibling()) {
	    RegisterViewItem* it = static_cast<RegisterViewItem*>(i);
	    if (it->m_reg.regName == reg->regName) {
		found = true;
		it->m_found = true;
		if (it->m_reg.rawValue != reg->rawValue ||
		    it->m_reg.cookedValue != reg->cookedValue)
		{
		    it->m_changes = true;
		    it->setValue(reg->rawValue, reg->cookedValue);
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
	    }
	}
	if (!found)
	    m_lastInsert = new RegisterViewItem(this, reg->regName,
						reg->rawValue, reg->cookedValue);
	delete reg;
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
	it->setValue(it->m_reg.rawValue, it->m_reg.cookedValue);
    }
}

#include "regwnd.moc"
