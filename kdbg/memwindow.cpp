// $Id$

// Copyright by Johannes Sixt
// This file is under GPL, the GNU General Public Licence

#include "memwindow.h"
#if QT_VERSION >= 200
#include <kglobalsettings.h>
#else
#include <kapp.h>
#endif
#include "debugger.h"
#include "dbgdriver.h"			/* memory dump formats */

MemoryWindow::MemoryWindow(QWidget* parent, const char* name) :
	QWidget(parent, name),
	m_debugger(0),
	m_expression(this, "expression"),
	m_memory(this, "memory"),
	m_layout(this, 0, 2),
	m_format(MDTword | MDThex)
{
    // use fixed font for contents
#if QT_VERSION < 200
    m_memory.setFont(kapp->fixedFont);
#else
    m_memory.setFont(KGlobalSettings::fixedFont());
#endif

    QSize minSize = m_expression.sizeHint();
    m_expression.setMinimumSize(minSize);
    QSize memSize = m_memory.sizeHint();
    if (memSize.width() < 0 || memSize.height() < 0)	// sizeHint unimplemented?
	memSize = minSize;
    m_memory.setMinimumSize(memSize);

    // create layout
    m_layout.addWidget(&m_expression, 0);
    m_layout.addWidget(&m_memory, 10);
    m_layout.activate();

    m_memory.setReadOnly(true);
    connect(&m_expression, SIGNAL(returnPressed()), this, SLOT(slotNewExpression()));

    // the popup menu
    m_popup.insertItem("&Bytes", MDTbyte);
    m_popup.insertItem("&Halfwords (2 Bytes)", MDThalfword);
    m_popup.insertItem("&Words (4 Bytes)", MDTword);
    m_popup.insertItem("&Giantwords (8 Bytes)", MDTgiantword);
    m_popup.insertSeparator();
    m_popup.insertItem("He&xadecimal", MDThex);
    m_popup.insertItem("Signed &decimal", MDTsigned);
    m_popup.insertItem("&Unsigned decimal", MDTunsigned);
    m_popup.insertItem("&Octal", MDToctal);
    m_popup.insertItem("Bi&nary", MDTbinary);
    m_popup.insertItem("&Addresses", MDTaddress);
    m_popup.insertItem("&Character", MDTchar);
    m_popup.insertItem("&Floatingpoint", MDTfloat);
    m_popup.insertItem("&Strings", MDTstring);
    m_popup.insertItem("&Instructions", MDTinsn);
    connect(&m_popup, SIGNAL(activated(int)), this, SLOT(slotTypeChange(int)));

    m_expression.installEventFilter(this);
    m_memory.installEventFilter(this);
}

MemoryWindow::~MemoryWindow()
{
}

void MemoryWindow::paletteChange(const QPalette& oldPal)
{
    // font may have changed
#if QT_VERSION < 200
    m_memory.setFont(kapp->fixedFont);
#else
    m_memory.setFont(KGlobalSettings::fixedFont());
#endif
    QWidget::paletteChange(oldPal);
}

void MemoryWindow::mousePressEvent(QMouseEvent* ev)
{
    handlePopup(ev);
}

#if QT_VERSION < 200
# define MOUSEPRESS Event_MouseButtonPress
#else
# define MOUSEPRESS QEvent::MouseButtonPress
#endif

bool MemoryWindow::eventFilter(QObject* o, QEvent* ev)
{
    if (ev->type() == MOUSEPRESS) {
	handlePopup(static_cast<QMouseEvent*>(ev));
	return true;
    }
    return false;
}

void MemoryWindow::handlePopup(QMouseEvent* ev)
{
    if (ev->button() == RightButton)
    {
	// show popup menu
	if (m_popup.isVisible())
	{
	    m_popup.hide();
	}
	else
	{
	    m_popup.popup(mapToGlobal(ev->pos()));
	}
	return;
    }
}

void MemoryWindow::slotNewExpression()
{
    QString expr = m_expression.text();
    expr = expr.simplifyWhiteSpace();
    m_debugger->setMemoryExpression(expr);

    // clear memory dump if no dump wanted
    if (expr.isEmpty()) {
	m_memory.setText(QString());
    }
}

void MemoryWindow::slotTypeChange(int id)
{
    // compute new type
    if (id & MDTsizemask)
	m_format = (m_format & ~MDTsizemask) | id;
    if (id & MDTformatmask)
	m_format = (m_format & ~MDTformatmask) | id;
    m_debugger->setMemoryFormat(m_format);

    // force redisplay
    slotNewExpression();
}

void MemoryWindow::slotNewMemoryDump(const QString& dump)
{
    m_memory.setText(dump);
}


#include "memwindow.moc"
