// -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
// $Id$

// Copyright by Judin Max, Johannes Sixt, Daniel Kristjansson
// This file is under GPL, the GNU General Public Licence

#include <qheader.h>
#include <kglobalsettings.h>
#include <klocale.h>			/* i18n */
#include <kiconloader.h>
#include <qfontdialog.h>
#include <qmessagebox.h>
#include <qpopmenu.h>
#include <qregexp.h>
#include <qstringlist.h>
#include <stdlib.h>			/* strtoul */
#include "regwnd.h"
#include "dbgdriver.h"

// helper struct
struct MenuPair
{
    QString name;
    RegisterDisplay mode;
    bool isSeparator() { return name.isEmpty(); }
};

static MenuPair menuitems[] = {
    // treat as
    { I18N_NOOP("&GDB default"), RegisterDisplay::nada },
    { I18N_NOOP("&Binary"),      RegisterDisplay::binary },
    { I18N_NOOP("&Octal"),       RegisterDisplay::octal },
    { I18N_NOOP("&Decimal"),     RegisterDisplay::decimal },
    { I18N_NOOP("He&xadecimal"), RegisterDisplay::hex },
    { I18N_NOOP("Real (&e)"),    RegisterDisplay::realE },
    { I18N_NOOP("Real (&f)"),    RegisterDisplay::realF },
    { I18N_NOOP("&Real (g)"),    RegisterDisplay::realG },
    { QString(), 0 },
    { "8 bits",  RegisterDisplay::bits8 },
    { "16 bits", RegisterDisplay::bits16 },
    { "32 bits", RegisterDisplay::bits32 },
    { "64 bits", RegisterDisplay::bits64 },
    { "80 bits", RegisterDisplay::bits80 },
    { "128 bits",RegisterDisplay::bits128 },
};

uint RegisterDisplay::bitMap[] = {
  0, 8, 16, 32,
  64, 80, 128, /*default*/32,
};

class GroupingViewItem : public QListViewItem
{
public:
    GroupingViewItem(RegisterView* parent, 
                     const QString& name, const QString& pattern) :
        QListViewItem(parent, name), matcher(pattern)
    {
        setExpandable(true);
        setOpen(true);
    } 
    bool matchName(const QString& str) const
    {
        return matcher.exactMatch(str);
    }
private:
    QRegExp matcher;
};

class RegisterViewItem : public QListViewItem
{
public:
    RegisterViewItem(QListViewItem* parent,
		     const RegisterInfo& regInfo);
    ~RegisterViewItem();

    void setValue(const RegisterInfo& regInfo);
    RegisterInfo m_reg;
    RegisterDisplay m_mode;		/* display mode */
    bool m_changes;
    bool m_found;

protected:
    virtual void paintCell(QPainter*, const QColorGroup& cg,
			   int column, int width, int alignment);

};


RegisterViewItem::RegisterViewItem(QListViewItem* parent,
				   const RegisterInfo& regInfo) :
	QListViewItem(parent),
	m_reg(regInfo),
	m_changes(false),
	m_found(true)
{
    // for ia32 proc
    if (m_reg.regName=="eflags" ||
        m_reg.regName=="fctrl" || m_reg.regName=="mxcsr")
	m_mode=RegisterDisplay::bits32|RegisterDisplay::binary;
    if (m_reg.regName.left(3)=="xmm")
	m_mode=RegisterDisplay::bits32|RegisterDisplay::realE;
    if (m_reg.regName.left(2)=="mm")
	m_mode=RegisterDisplay::bits32|RegisterDisplay::realE;
    if (m_reg.regName.left(2)=="st")
	m_mode=RegisterDisplay::bits80|RegisterDisplay::realE;
    // for POWER AltiVec proc (vr is also for SPARC AltiVec)
    if (m_reg.regName=="cr" || m_reg.regName=="fpscr" || m_reg.regName=="vscr")
	m_mode=RegisterDisplay::bits32|RegisterDisplay::binary;
    if (m_reg.regName.left(3)=="fpr")
	m_mode=RegisterDisplay::bits64|RegisterDisplay::realE;
    if (m_reg.regName.left(2)=="vr")
	m_mode=RegisterDisplay::bits32|RegisterDisplay::realE;
    // for MIPS VU proc (playstation2)
    if (m_reg.regName.left(2)=="vu")
	m_mode=RegisterDisplay::bits32|RegisterDisplay::realE;

    setValue(m_reg);
    setText(0, m_reg.regName);
}

RegisterViewItem::~RegisterViewItem()
{
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

static QString toBinary(QString hex)
{
    static const char digits[16][8] = {
	"0000", "0001", "0010", "0011", "0100", "0101", "0110", "0111",
	"1000", "1001", "1010", "1011", "1100", "1101", "1110", "1111"
    };
    QString result;
    
    for (unsigned i = 2; i < hex.length(); i++) {
	int idx = hexCharToDigit(hex[i].latin1());
	if (idx < 0) {
	    // not a hex digit; no conversion
	    return hex;
	}
	const char* bindigits = digits[idx];
	result += bindigits;
    }
    // remove leading zeros
    switch (hexCharToDigit(hex[2].latin1())) {
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
	int idx = hexCharToDigit(hex[i].latin1());
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

    const char* start = hex.latin1();
    char* end;
    unsigned long val = strtoul(start, &end, 0);
    if (start == end)
	return hex;
    else
	return QString().setNum(val);
}

static QString toBCD(const QString& hex)
{
    return hex.right(2);
}

static char* toRaw(const QString& hex, uint& length)
{
    static uint testNum=1;
    static void* testVoid=(void*)&testNum;
    static char* testChar=(char*)testVoid;
    static bool littleendian=(*testChar==1);

    length=((hex.length()-2)%2)+((hex.length()-2)/2);
    char* data=new char[length];

    if (littleendian) {
	uint j=0;
	if (hex.length()<=2) return 0;
	for (int i=hex.length()-1; i>=2; ) {
	    if (j%2==0) data[j/2]=hexCharToDigit(hex[i].latin1());
	    else data[j/2]|=(hexCharToDigit(hex[i].latin1())<<4);
	    i--;j++;
	}
    } else { // big endian
	uint j=0;
	if (hex.length()<=2) return 0;
	for (uint i=2; i<hex.length(); ) {
	    if (j%2==0) data[j/2]=hexCharToDigit(hex[i].latin1())<<4;
	    else data[j/2]|=hexCharToDigit(hex[i].latin1());
	    i++;j++;
	}
    }
    return data;
}

static long double extractNumber(const QString& hex)
{
    uint length;
    char* data=toRaw(hex, length);
    long double val;
    if (length==4) { // float
	val=*((float*)data);
    } else if (length==8) { // double
	val=*((double*)data);
    } else if (length==10) { // long double
	val=*((long double*)data);
    } else {
	val=*((float*)data);
    }
    delete[] data;

    return val;
}

static QString toFloat(const QString& hex, char p)
{
    uint bits;
    uint prec=6;
    if (hex.length()<=10) { bits=32; prec=6; }
    else if (hex.length()<=18) { bits=64; prec=17; }
    else { bits=80; prec=20; }

    QString cooked=QString::number(extractNumber(hex), p, prec);
    if (p=='e') {
	prec+=7;    
	while (cooked.length()<prec) cooked=cooked.prepend(" ");
    }
    return cooked;
}

static void convertSingle(QString& cooked, const QString& raw,
                          RegisterDisplay mode)
{
    switch (mode.presentationFlag()) {
    case RegisterDisplay::binary:  cooked = toBinary(raw); break;
    case RegisterDisplay::octal:   cooked = toOctal(raw); break;
    case RegisterDisplay::decimal: cooked = toDecimal(raw); break;
    case RegisterDisplay::hex:     cooked = raw; break;
    case RegisterDisplay::bcd:     cooked = toBCD(raw); break;
    case RegisterDisplay::realE:   cooked = toFloat(raw, 'e'); break;
    case RegisterDisplay::realG:   cooked = toFloat(raw, 'g'); break;
    case RegisterDisplay::realF:   cooked = toFloat(raw, 'f'); break;
    }
}

static void convertRaw(QString& cooked, const RegisterInfo& reg,
		       RegisterDisplay mode)
{
    if (RegisterDisplay::nada==mode.presentationFlag()) {
	if (cooked.at(0)!=' ' && cooked.at(0)!='-' && cooked.at(0)!='+')
	    cooked.prepend(" ");
	return;
    }
    uint totalNibles=0, nibles=mode.bits()>>2;
    if (reg.rawValue.length() > 2 && 
	reg.rawValue[0] == '0' && reg.rawValue[1] == 'x')
    {
	if (reg.type=="uint128") totalNibles=32; 
	else if (reg.type=="uint64") totalNibles=16;
	else if (reg.type.isEmpty()) totalNibles=nibles;
	else {
	    cooked = "don't know how to handle vector type <"+reg.type+">";
	    return;
	}

	QString raw=reg.rawValue.right(reg.rawValue.length()-2); // clip off "0x"
	while (raw.length()<totalNibles) raw.prepend("0"); // pad out to totalNibles

	if (nibles>=totalNibles) {
	    raw.prepend("0x");
	    convertSingle(cooked, raw, mode);
	    cooked=" "+cooked; // add space so that +/- sign is not clipped in rendering
	    return;
	}

	// default to 4 byte, 32 bits values
	if (nibles==(RegisterDisplay::bitsUnknown<<2) || nibles==0) nibles=8;

	QString separator=",";
	cooked="";

	for (int nib=totalNibles-nibles;; nib-=nibles) {
	    if (nib<0)
		break;
	    QString qstr;
	    convertSingle(qstr, QString("0x")+raw.mid(nib, nibles), mode);

	    if (nib==int(totalNibles-nibles))
		cooked=qstr+cooked;
	    else
		cooked=qstr+separator+cooked;
	}
	if (!(mode.contains(RegisterDisplay::realE) ||
	      mode.contains(RegisterDisplay::realF) ||
	      mode.contains(RegisterDisplay::realG)))
	    cooked=" "+cooked; // add space so that this is aligned with signed values
    }
}

void RegisterViewItem::setValue(const RegisterInfo& reg)
{
    m_reg = reg;

    setText(1, reg.rawValue);
    QString cookedValue=reg.cookedValue;
    convertRaw(cookedValue, reg, m_mode);
    setText(2, cookedValue);
}

void RegisterViewItem::paintCell(QPainter* p, const QColorGroup& cg,
				 int column, int width, int alignment)
{
    if (m_changes) {
	QColorGroup newcg = cg;
	newcg.setColor(QColorGroup::Text, red);
	QListViewItem::paintCell(p, newcg, column, width, alignment);
    } else {
	QListViewItem::paintCell(p, cg, column, width, alignment);
    }
}


RegisterView::RegisterView(QWidget* parent, const char* name) :
	QListView(parent, name),
	m_mode(RegisterDisplay::bitsUnknown|RegisterDisplay::nada)
{
    setSorting(-1);
    setFont(KGlobalSettings::fixedFont());

    QPixmap iconRegs = BarIcon("regs.xpm");
    QPixmap iconWatchcoded = BarIcon("watchcoded.xpm");
    QPixmap iconWatch = BarIcon("watch.xpm");

    addColumn(QIconSet(iconRegs), i18n("Register"));
    addColumn(QIconSet(iconWatchcoded), i18n("Value"));
    addColumn(QIconSet(iconWatch), i18n("Decoded value"));

    setColumnAlignment(0,AlignLeft);
    setColumnAlignment(1,AlignLeft);
    setColumnAlignment(2,AlignLeft);

    setAllColumnsShowFocus( true );
    header()->setClickEnabled(false);

    connect(this, SIGNAL(contextMenuRequested(QListViewItem*, const QPoint&, int)),
	    SLOT(rightButtonClicked(QListViewItem*,const QPoint&,int)));

    m_modemenu = new QPopupMenu(this, i18n("All Registers"));
    for (uint i=0; i<sizeof(menuitems)/sizeof(MenuPair); i++) {
	if (menuitems[i].isSeparator())
	    m_modemenu->insertSeparator();
	else
	    m_modemenu->insertItem(i18n(menuitems[i].name), menuitems[i].mode);
    }
    connect(m_modemenu,SIGNAL(activated(int)),SLOT(slotModeChange(int)));
    
    new GroupingViewItem(this, "MIPS VU", "^vu.*");
    new GroupingViewItem(this, "AltiVec", "(^vr.*|^vscr$)");
    new GroupingViewItem(this, "POWER real", "(^fpr.*|^fpscr$)");
    new GroupingViewItem(this, "MMX", "^mm.*");
    new GroupingViewItem(this, "SSE", "(^xmm.*|^mxcsr)");
    new GroupingViewItem(this, "x87", 
      "(^st.*|^fctrl$|^fop$|^fooff$|^foseg$|^fioff$|^fiseg$|^ftag$|^fstat$)");
    new GroupingViewItem(this, i18n("x86 segment"), "(^cs$|^ss$|^ds$|^es$|^fs$|^gs$)");
    new GroupingViewItem(this, i18n("GP and others"), "^$");

    updateGroupVisibility();
    setRootIsDecorated(true);
    
    resize(200,300);
}

RegisterView::~RegisterView()
{
}

QListViewItem* RegisterView::findMatchingGroup(const QString& regName)
{
    for (QListViewItem* it = firstChild(); it != 0; it = it->nextSibling())
    {
	GroupingViewItem* i = static_cast<GroupingViewItem*>(it);
	if (i->matchName(regName))
	    return it;
    }
    // not better match found, so return "GP and others"
    return firstChild();
}

QListViewItem* RegisterView::findGroup(const QString& groupName)
{
    for (QListViewItem* it = firstChild(); it != 0; it = it->nextSibling())
    {
	if (it->text(0) == groupName)
	    return it;
    }
    return 0;
}

void RegisterView::updateGroupVisibility()
{
    for (QListViewItem* it = firstChild(); it != 0; it = it->nextSibling())
    {
	it->setVisible(it->childCount() > 0);
    }
}

void RegisterView::updateRegisters(QList<RegisterInfo>& regs)
{
    setUpdatesEnabled(false);

    // mark all items as 'not found'
    for (RegMap::iterator i = m_registers.begin(); i != m_registers.end(); ++i)
    {
	i->second->m_found = false;
    }

    // parse register values
    // must iterate last to first, since QListView inserts at the top
    for (RegisterInfo* reg = regs.last(); reg != 0; reg = regs.prev())
    {
	// check if this is a new register
	RegMap::iterator i = m_registers.find(reg->regName);

	if (i != m_registers.end())
	{
	    RegisterViewItem* it = i->second;
	    it->m_found = true;
	    if (it->m_reg.rawValue != reg->rawValue ||
		it->m_reg.cookedValue != reg->cookedValue)
	    {
		it->m_changes = true;
		it->setValue(*reg);
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
	else
	{
	    QListViewItem* group = findMatchingGroup(reg->regName);
	    m_registers[reg->regName] =
		new RegisterViewItem(group, *reg);
	}
    }

    // remove all 'not found' items;
    QStringList del;
    for (RegMap::iterator i = m_registers.begin(); i != m_registers.end(); ++i)
    {
	if (!i->second->m_found) {
	    del.push_back(i->first);
	}
    }
    for (QStringList::Iterator i = del.begin(); i != del.end(); ++i)
    {
	RegMap::iterator it = m_registers.find(*i);
	delete it->second;
	m_registers.erase(it);
    }

    updateGroupVisibility();
    setUpdatesEnabled(true);
    triggerUpdate();
}
  

void RegisterView::rightButtonClicked(QListViewItem* item, const QPoint& p, int)
{
    RegisterDisplay mode=m_mode;
    
    if (item) {
        m_modemenu->setCaption(item->text(0));
        if (m_registers.end()!=m_registers.find(m_modemenu->caption()))
            mode=static_cast<RegisterViewItem*>(item)->m_mode;
    } else m_modemenu->setCaption(i18n("All Registers"));
    
    for (unsigned int i = 0; i<sizeof(menuitems)/sizeof(MenuPair); i++) {
        m_modemenu->setItemChecked(menuitems[i].mode, 
                                   mode.contains(menuitems[i].mode));
    }
    
    // show popup menu
    m_modemenu->popup(p);
}

void RegisterView::slotModeChange(int pcode)
{
    RegisterDisplay old_mode = m_mode; // default to class mode
    // create flag masks
    uint code=(uint)pcode, mmask, cmask;

    if ((code&0xf0)==code) { cmask=0xf0; mmask=0x0f; }
    else if ((code&0x0f)==code) { cmask=0x0f; mmask=0xf0; }
    else return;

    // find register view
    RegisterViewItem* view=0;
    QListViewItem* gview=0;
    RegMap::iterator it=m_registers.find(m_modemenu->caption());
    if (it!=m_registers.end()) {
        view = it->second;
        old_mode = view->m_mode;
    } else gview=findGroup(m_modemenu->caption());

    // set new mode
    RegisterDisplay mode = (code&cmask) | (old_mode&mmask);
  
    // update menu
    for (unsigned int i = 0; i<sizeof(menuitems)/sizeof(MenuPair); i++) {
        m_modemenu->setItemChecked(menuitems[i].mode, 
                                   mode.contains(menuitems[i].mode));
    }
  
    // update register view[s]
    if (view) {
        view->m_mode=mode;
        view->setValue(view->m_reg);
        repaintItem(view);
    } else if (gview) {
        QListViewItem* it=gview->firstChild(); 
        for (; it!=0; it=it->nextSibling()) {
            view = (RegisterViewItem*)it;
            view->m_mode=mode;
            view->setValue(view->m_reg);
            repaintItem(view);
        }
    } else {
        m_mode=mode;
        for (it=m_registers.begin(); it!=m_registers.end(); ++it) {
            view = it->second;
            view->m_mode=mode;
            view->setValue(view->m_reg);
            repaintItem(view);
        }
    }
}

void RegisterView::paletteChange(const QPalette& oldPal)
{
    setFont(KGlobalSettings::fixedFont());
    QListView::paletteChange(oldPal);
}

#include "regwnd.moc"
