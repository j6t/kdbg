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

MemoryWindow::MemoryWindow(QWidget* parent, const char* name) :
	QWidget(parent, name),
	m_debugger(0),
	m_expression(this, "expression"),
	m_memory(this, "memory"),
	m_layout(this, 0, 2)
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
    m_popup.insertItem("&Bytes", 1);
    m_popup.insertItem("&Halfwords (2 Bytes)", 2);
    m_popup.insertItem("&Words (4 Bytes)", 3);
    m_popup.insertItem("&Giantwords (8 Bytes)", 4);
    m_popup.insertSeparator();
    m_popup.insertItem("He&xadecimal", 5);
    m_popup.insertItem("Signed &decimal", 6);
    m_popup.insertItem("&Unsigned decimal", 7);
    m_popup.insertItem("&Octal", 8);
    m_popup.insertItem("Bi&nary", 9);
    m_popup.insertItem("&Addresses", 10);
    m_popup.insertItem("&Character", 11);
    m_popup.insertItem("&Floating-point", 12);
    m_popup.insertItem("&Strings", 13);
    m_popup.insertItem("&Instructions", 14);
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
}

void MemoryWindow::slotTypeChange(int id)
{
}


#include "memwindow.moc"
