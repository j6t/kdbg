// $Id$

// Copyright by Judin Max, Johannes Sixt
// This file is under GPL, the GNU General Public Licence

#ifndef REGWND_H
#define REGWND_H

#include <qlistview.h>
#include <map>

class QPopupMenu;
class RegisterViewItem;
struct RegisterInfo;

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

    operator uint() {
	return mode;
    }
    bool contains(uint pmode) const {
	bool val=((mode&0xf0)==pmode)||((mode&0x0f)==pmode);    
	return val;
    }
    uint bitsFlag() { return mode&0xf0; }
    uint presentationFlag() const { return mode&0x0f; }
    uint bits() const { return bitMap[(mode>>4)&0x07]; }
private:
    uint mode;
    static uint bitMap[];
};


class RegisterView : public QListView
{
    Q_OBJECT
public:
    RegisterView(QWidget* parent, const char *name = 0L);
    ~RegisterView();

protected slots:
    void rightButtonClicked(QListViewItem*, const QPoint&, int);
    void slotModeChange(int);
    void updateRegisters(QList<RegisterInfo>&);

private:
    void paletteChange(const QPalette& oldPal);
    void updateGroupVisibility();
    QListViewItem* findMatchingGroup(const QString& regName);
    QListViewItem* findGroup(const QString& groupName);
    QPopupMenu* m_modemenu;
    RegisterDisplay m_mode;
    typedef std::map<QString,RegisterViewItem*> RegMap;
    RegMap m_registers;

friend class RegisterViewItem;
};

#endif // REGWND_H
