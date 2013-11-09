/*
 * Copyright Max Judin, Johannes Sixt, Daniel Kristjansson
 * This file is licensed under the GNU General Public License Version 2.
 * See the file COPYING in the toplevel directory of the source directory.
 */

#include "regwnd.h"
#include "dbgdriver.h"
#include <kglobalsettings.h>
#include <klocale.h>			/* i18n */
#include <QMenu>
#include <QRegExp>
#include <QStringList>
#include <QHeaderView>
#include <QContextMenuEvent>
#include <stdlib.h>			/* strtoul */

/** 
 * Register display modes
 */
class RegisterDisplay {
public:
    enum BitSize {
	bits8   = 0x10,
	bits16  = 0x20,
	bits32  = 0x30,
	bits64  = 0x40,
	bits80  = 0x50,
	bits128 = 0x60,
	bitsUnknown = 0x70
    };

    enum Format {
	nada    = 0x01,
	binary  = 0x02,
	octal   = 0x03,
	decimal = 0x04,
	hex     = 0x05,
	bcd     = 0x06,
	realE   = 0x07,
	realG   = 0x08,
	realF   = 0x09
    };
    RegisterDisplay() : mode(bitsUnknown|nada) { }
    RegisterDisplay(uint newMode) : mode(newMode) { }

    bool contains(uint pmode) const {
	bool val=((mode&0xf0)==pmode)||((mode&0x0f)==pmode);    
	return val;
    }
    uint bitsFlag() { return mode&0xf0; }
    uint presentationFlag() const { return mode&0x0f; }
    uint bits() const { return bitMap[(mode>>4)&0x07]; }
    void changeFlag(uint code) {
	uint mask=((code&0xf0)==code)?0x0f:0xf0;
        mode = code | (mode & mask);
    }
private:
    uint mode;
    static uint bitMap[];
};

// helper struct
struct MenuPair
{
    const char* name;
    uint mode;
    bool isSeparator() { return name == 0; }
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
    { 0, 0 },
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

class ModeItem : public QTreeWidgetItem
{
public:
    ModeItem(QTreeWidget* parent, const QString& name) : QTreeWidgetItem(parent, QStringList(name)) {}
    ModeItem(QTreeWidgetItem* parent) : QTreeWidgetItem(parent) {}

    virtual void setMode(RegisterDisplay mode) = 0;
    virtual RegisterDisplay mode() = 0;
};

class GroupingViewItem : public ModeItem
{
public:
    GroupingViewItem(RegisterView* parent, 
		     const QString& name, const QString& pattern,
		     RegisterDisplay mode) :
	ModeItem(parent, name), matcher(pattern), gmode(mode)
    {
	setExpanded(true);
    } 

    bool matchName(const QString& str) const
    {
        return matcher.exactMatch(str);
    }

    virtual void setMode(RegisterDisplay mode)
    {
	gmode=mode; 
	for(int i = 0; i < childCount(); i++)
	{
	    static_cast<ModeItem*>(child(i))->setMode(gmode);
	}
    }

    virtual RegisterDisplay mode()
    {
        return gmode;
    }

private:
    QRegExp matcher;
    RegisterDisplay gmode;
};

class RegisterViewItem : public ModeItem
{
public:
    RegisterViewItem(GroupingViewItem* parent,
		     const RegisterInfo& regInfo);
    ~RegisterViewItem();

    void setValue(const RegisterInfo& regInfo);
    virtual void setMode(RegisterDisplay mode);
    virtual RegisterDisplay mode() { return m_mode; }
    RegisterInfo m_reg;
    RegisterDisplay m_mode;		/* display mode */
    bool m_changes;
    bool m_found;
};


RegisterViewItem::RegisterViewItem(GroupingViewItem* parent,
				   const RegisterInfo& regInfo) :
	ModeItem(parent),
	m_reg(regInfo),
	m_changes(false),
	m_found(true)
{
    setValue(m_reg);
    setText(0, m_reg.regName);
    setMode(parent->mode());
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
    
    for (int i = 2; i < hex.length(); i++) {
	int idx = hexCharToDigit(hex[i].toLatin1());
	if (idx < 0) {
	    // not a hex digit; no conversion
	    return hex;
	}
	const char* bindigits = digits[idx];
	result += bindigits;
    }
    // remove leading zeros
    switch (hexCharToDigit(hex[2].toLatin1())) {
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
	int idx = hexCharToDigit(hex[i].toLatin1());
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
    if (hex.length() > int(sizeof(unsigned long)*2+2))	/*  count in leading "0x" */
	return hex;

    bool ok = false;
    unsigned long val = hex.toULong(&ok, 0);
    if (!ok)
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
	    if (j%2==0)
		data[j/2]=hexCharToDigit(hex[i].toLatin1());
	    else
		data[j/2]|=(hexCharToDigit(hex[i].toLatin1())<<4);
	    i--;j++;
	}
    } else { // big endian
	uint j=0;
	if (hex.length()<=2) return 0;
	for (int i=2; i<hex.length(); ) {
	    if (j%2==0)
		data[j/2]=hexCharToDigit(hex[i].toLatin1())<<4;
	    else
		data[j/2]|=hexCharToDigit(hex[i].toLatin1());
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
    int prec=6;
    if (hex.length() <= 10)
	prec = 6;
    else if (hex.length() <= 18)
	prec = 17;
    else
	prec = 20;

    char fmt[8] = "%.*Lf";
    fmt[4] = p;
    char buf[50];
    sprintf(buf, fmt, prec, extractNumber(hex));
    QString cooked = QString::fromLatin1(buf);
    if (p=='e') {
	prec+=7;    
	while (cooked.length()<prec) cooked=cooked.prepend(" ");
    }
    return cooked;
}

static QString convertSingle(const QString& raw, const RegisterDisplay mode) 
{
    switch (mode.presentationFlag()) {
    case RegisterDisplay::binary:  return toBinary(raw);
    case RegisterDisplay::octal:   return toOctal(raw);
    case RegisterDisplay::decimal: return toDecimal(raw);
    case RegisterDisplay::hex:     return raw;
    case RegisterDisplay::bcd:     return toBCD(raw);
    case RegisterDisplay::realE:   return toFloat(raw, 'e');
    case RegisterDisplay::realG:   return toFloat(raw, 'g');
    case RegisterDisplay::realF:   return toFloat(raw, 'f');
    default: return raw;
    }
}

QString convertRaw(const RegisterInfo reg, RegisterDisplay mode)
{
    QString cooked;
    int totalNibles=0, nibles=mode.bits()>>2;
    if (RegisterDisplay::nada!=mode.presentationFlag() &&
        reg.rawValue.length() > 2 && reg.rawValue[0] == '0' && reg.rawValue[1] == 'x')
    {
        if ("uint128"==reg.type) totalNibles=32; 
        else if ("uint64"==reg.type) totalNibles=16;
        else if (reg.type.isEmpty()) totalNibles=nibles;
        else {
            return "don't know how to handle vector type <"+reg.type+">";
        }
        if (0==nibles) nibles=8; // default to 4 byte, 32 bits values
        if (nibles>totalNibles) totalNibles=nibles; // minimum one value

        QString raw=reg.rawValue.right(reg.rawValue.length()-2); // clip off "0x"
        while (raw.length()<totalNibles) raw.prepend("0"); // pad out to totalNibles

	QString separator=",";	// locale-specific?
        for (int nib=totalNibles-nibles; nib>=0; nib-=nibles) {
            QString qstr=convertSingle(raw.mid(nib, nibles).prepend("0x"), mode);
            
            if (nib==int(totalNibles-nibles)) cooked=qstr+cooked;
            else cooked=qstr+separator+cooked;
        }
    }
    else
    {
	cooked = reg.cookedValue;
    }
    if (!cooked.isEmpty() && cooked.at(0) != ' ' && cooked.at(0) != '-' && cooked.at(0) != '+')
	cooked.prepend(" ");
    return cooked;
}

void RegisterViewItem::setValue(const RegisterInfo& reg)
{
    m_reg = reg;

    setText(1, reg.rawValue);
    QString cookedValue = convertRaw(reg, m_mode);
    setText(2, cookedValue);
}

void RegisterViewItem::setMode(RegisterDisplay mode)
{
    m_mode = mode;

    QString cookedValue = convertRaw(m_reg, mode);
    setText(2, cookedValue);
}


RegisterView::RegisterView(QWidget* parent) :
	QTreeWidget(parent)
{
    setFont(KGlobalSettings::fixedFont());

    QTreeWidgetItem* header = headerItem();
    header->setText(0, i18n("Register"));
    header->setText(1, i18n("Value"));
    header->setText(2, i18n("Decoded value"));

    setAllColumnsShowFocus(true);

    m_modemenu = new QMenu("ERROR", this);
    for (uint i=0; i<sizeof(menuitems)/sizeof(MenuPair); i++) {
	if (menuitems[i].isSeparator())
	    m_modemenu->addSeparator();
	else {
	    QAction* action = m_modemenu->addAction(i18n(menuitems[i].name));
	    action->setData(menuitems[i].mode);
	    action->setCheckable(true);
	}
    }
    connect(m_modemenu, SIGNAL(triggered(QAction*)), SLOT(slotModeChange(QAction*)));
    
    new GroupingViewItem(this, i18n("GP and others"), "^$",
			 RegisterDisplay::nada);
    new GroupingViewItem(this, i18n("Flags"),
			 "(^eflags$|^fctrl$|^mxcsr$|^cr$|^fpscr$|^vscr$|^ftag$|^fstat$)",
			 RegisterDisplay::bits32|RegisterDisplay::binary);
    new GroupingViewItem(this, i18n("x86/x87 segment"),
			 "(^cs$|^ss$|^ds$|^es$|^fs$|^gs$|^fiseg$|^foseg$)",
			 RegisterDisplay::nada);
    new GroupingViewItem(this, "x87", "^st.*",
			 RegisterDisplay::bits80|RegisterDisplay::realE);
    new GroupingViewItem(this, "SSE", "^xmm.*",
			 RegisterDisplay::bits32|RegisterDisplay::realE);
    new GroupingViewItem(this, "MMX", "^mm.*",
			 RegisterDisplay::bits32|RegisterDisplay::realE);
    new GroupingViewItem(this, "POWER real", "^fpr.*",
			 RegisterDisplay::bits32|RegisterDisplay::realE);
    new GroupingViewItem(this, "AltiVec", "^vr.*",
			 RegisterDisplay::bits32|RegisterDisplay::realE);
    new GroupingViewItem(this, "MIPS VU", "^vu.*",
			 RegisterDisplay::bits32|RegisterDisplay::realE);

    updateGroupVisibility();
    setRootIsDecorated(true);
    
    resize(200,300);
}

RegisterView::~RegisterView()
{
}

GroupingViewItem* RegisterView::findMatchingGroup(const QString& regName)
{
    for (int i = 0; i < topLevelItemCount(); i++)
    {
	GroupingViewItem* it = static_cast<GroupingViewItem*>(topLevelItem(i));
	if (it->matchName(regName))
	    return it;
    }
    // not better match found, so return "GP and others"
    return static_cast<GroupingViewItem*>(topLevelItem(0));
}

GroupingViewItem* RegisterView::findGroup(const QString& groupName)
{
    for (int i = 0; i < topLevelItemCount(); i++)
    {
	QTreeWidgetItem* it = topLevelItem(i);
	if (it->text(0) == groupName)
	    return static_cast<GroupingViewItem*>(it);
    }
    // return that nothing was found.
    return 0;
}

// only show a group if it has subitems.
void RegisterView::updateGroupVisibility()
{
    for(int i = 0; i < topLevelItemCount(); i++)
    {
	QTreeWidgetItem* item = topLevelItem(i);
	item->setHidden(item->childCount() == 0);
    }
}

void RegisterView::updateRegisters(const std::list<RegisterInfo>& regs)
{
    setUpdatesEnabled(false);

    // mark all items as 'not found'
    for (RegMap::iterator i = m_registers.begin(); i != m_registers.end(); ++i)
    {
	i->second->m_found = false;
    }

    // parse register values
    for (std::list<RegisterInfo>::const_iterator reg = regs.begin(); reg != regs.end(); ++reg)
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

		it->setForeground(0,Qt::red);
		it->setForeground(1,Qt::red);
		it->setForeground(2,Qt::red);

	    } else {
		/*
		 * If there was a change last time, but not now, we
		 * must revert the color.
		 */
		if (it->m_changes) {
		    it->m_changes = false;
		    it->setForeground(0,Qt::black);
		    it->setForeground(1,Qt::black);
		    it->setForeground(2,Qt::black);
		}
	    }
	}
	else
	{
	    GroupingViewItem* group = findMatchingGroup(reg->regName);
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
}
  

void RegisterView::contextMenuEvent(QContextMenuEvent* event)
{
    QTreeWidgetItem *item = itemAt(event->pos());

    if (item) {
        RegisterDisplay mode=static_cast<ModeItem*>(item)->mode();        
	int i = 0;
	foreach(QAction* action, m_modemenu->actions())
	{
	    action->setChecked(mode.contains(menuitems[i].mode));
	    ++i;
        }
        m_modemenu->setTitle(item->text(0));
	m_modemenu->popup(event->globalPos());

	event->accept();
    }    
}

void RegisterView::slotModeChange(QAction* action)
{
    RegMap::iterator it=m_registers.find(m_modemenu->title());
    ModeItem* view;
    if (it != m_registers.end())
	view = it->second;
    else
	view = findGroup(m_modemenu->title());

    if (view) {
	RegisterDisplay mode = view->mode();
	mode.changeFlag(action->data().toInt());
	view->setMode(mode);
    }
}

void RegisterView::paletteChange(const QPalette&)
{
    setFont(KGlobalSettings::fixedFont());
}

#include "regwnd.moc"
