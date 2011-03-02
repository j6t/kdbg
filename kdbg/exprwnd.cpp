/*
 * Copyright Johannes Sixt
 * This file is licensed under the GNU General Public License Version 2.
 * See the file COPYING in the toplevel directory of the source directory.
 */

#include "exprwnd.h"
#include "exprwnd.moc"
#include "typetable.h"
#include <QHeaderView>
#include <QStringList>
#include <QPainter>
#include <QPaintEvent>
#include <QFocusEvent>
#include <QKeyEvent>
#include <QScrollBar>
#include <kiconloader.h>		/* icons */
#include <klocale.h>			/* i18n */
#include "mydebug.h"

VarTree::VarTree(VarTree* parent, ExprValue* v) :
	QTreeWidgetItem(parent),
	m_varKind(v->m_varKind),
	m_nameKind(v->m_nameKind),
	m_type(0),
	m_exprIndex(0),
	m_exprIndexUseGuard(false),
	m_baseValue(v->m_value),
	m_baseChanged(false),
	m_structChanged(false)
{
    setText(v->m_name);
    updateValueText();
    if (v->m_child != 0 || m_varKind == VarTree::VKpointer)
	setChildIndicatorPolicy(QTreeWidgetItem::ShowIndicator);
    else
	setChildIndicatorPolicy(QTreeWidgetItem::DontShowIndicator);
    setExpanded(v->m_initiallyExpanded);
}

VarTree::VarTree(ExprWnd* parent, ExprValue* v) :
	QTreeWidgetItem(parent),
	m_varKind(VKsimple),
	m_nameKind(VarTree::NKplain),
	m_type(0),
	m_exprIndex(0),
	m_exprIndexUseGuard(false),
	m_baseValue(v->m_value),
	m_baseChanged(false),
	m_structChanged(false)
{
    setText(v->m_name);
    updateValueText();
}

VarTree::~VarTree()
{
}

QString VarTree::computeExpr() const
{
    // top-level items are special
    if (isToplevelExpr())
	return getText();

    // get parent expr
    VarTree* par = static_cast<VarTree*>(parent());
    QString parentExpr = par->computeExpr();

    // skip this item's name if it is a base class or anonymous struct or union
    if (m_nameKind == NKtype || m_nameKind == NKanonymous) {
	return parentExpr;
    }
    /* augment by this item's text */
    QString result;
    /* if this is an address, dereference it */
    if (m_nameKind == NKaddress) {
	ASSERT(par->m_varKind == VKpointer);
	result = "*" + parentExpr;
	return result;
    }
    switch (par->m_varKind) {
    case VKarray:
	{
	    QString index = getText();
	    int i = 1;
	    // skip past the index
	    while (index[i].isDigit())
		i++;
	    /*
	     * Some array indices are actually ranges due to repeated array
	     * values. We use the first index in these cases.
	     */
	    if (index[i] != ']') {
		// remove second index
		index.remove(i, index.length()-i-1);
	    }
	    result = "(" + parentExpr + ")" + index;
	}
	break;
    case VKstruct:
	result = "(" + parentExpr + ")." + getText();
	break;
    case VKsimple:			/* parent can't be simple */
    case VKpointer:			/* handled in NKaddress */
    case VKdummy:			/* can't occur at all */
	ASSERT(false);
	result = parentExpr;		/* paranoia */
	break;
    }
    return result;
}

bool VarTree::isToplevelExpr() const
{
    return parent() == 0;
}

bool VarTree::isAncestorEq(const VarTree* child) const
{
    const QTreeWidgetItem* c = child;
    while (c != 0 && c != this) {
	c = c->parent();
    }
    return c != 0;
}

bool VarTree::updateValue(const QString& newValue)
{
    // check whether the value changed
    bool prevValueChanged = m_baseChanged;
    if ((m_baseChanged = m_baseValue != newValue)) {
	m_baseValue = newValue;
	updateValueText();
	setForeground(1, QBrush(QColor(Qt::red)));
    } else if (prevValueChanged) {
	setForeground(1, treeWidget()->palette().text());
    }
    /*
     * We must repaint the cell if the value changed. If it did not change,
     * we still must repaint the cell if the value changed previously,
     * because the color of the display must be changed (from red to
     * black).
     */
    return m_baseChanged || prevValueChanged;
}

bool VarTree::updateStructValue(const QString& newValue)
{
    // check whether the value changed
    bool prevValueChanged = m_structChanged;
    if ((m_structChanged = m_structValue != newValue)) {
	m_structValue = newValue;
	updateValueText();
	setForeground(1, QBrush(QColor(Qt::red)));
    } else if (prevValueChanged) {
	setForeground(1, treeWidget()->palette().text());
    }
    /*
    * We must repaint the cell if the value changed. If it did not change,
    * we still must repaint the cell if the value changed previously,
    * because the color of the display must be changed (from red to
    * black).
    */
    return m_structChanged || prevValueChanged;
}

void VarTree::updateValueText()
{
    if (m_baseValue.isEmpty()) {
	setText(1, m_structValue);
    } else if (m_structValue.isEmpty()) {
	setText(1, m_baseValue);
    } else {
	setText(1, m_baseValue + " " + m_structValue);
    }
}

void VarTree::inferTypesOfChildren(ProgramTypeTable& typeTable)
{
    /*
     * Type inference works like this: We use type information of those
     * children that have a type name in their name (base classes) or in
     * their value (pointers)
     */

    // first recurse children
    for (int i = 0; i < childCount(); i++)
    {
	child(i)->inferTypesOfChildren(typeTable);
    }

    // if this is a pointer, get the type from the value (less the pointer)
    if (m_varKind == VKpointer) {
	if (isWcharT())
	{
	    /*
	     * wchart_t pointers must be treated as struct, because the array
	     * of characters is printed similar to how QStrings are decoded.
	     */
	    m_varKind = VKstruct;
	    setChildIndicatorPolicy(QTreeWidgetItem::DontShowIndicator);
	}
	// don't know how to do this cleanly
    } else if (m_varKind == VKstruct) {
	// check if this is a base class part
	if (m_nameKind == NKtype) {
	    const QString& typeName =
		getText().mid(1, getText().length()-2);	// strip < and >
	    m_type = typeTable.lookup(typeName);

	    /* if we don't have a type yet, get it from the base class */
	    if (m_type == 0) {
		m_type = inferTypeFromBaseClass();
		/*
		 * If there is a known type now, it is the one from the
		 * first base class whose type we know.
		 */
	    }

	    /*
	     * If we still don't have a type, the type is really unknown.
	     */
	    if (m_type == 0) {
		m_type = TypeInfo::unknownType();
	    }
	} // else
	    /*
	     * This is not a base class part. We don't assign a type so
	     * that later we can ask gdb.
	     */
    }
}

// the value contains the pointer type in parenthesis
bool VarTree::isWcharT() const
{
    return value().startsWith("(const wchar_t *)") ||
	    value().startsWith("(wchar_t *)");
}

/*
 * Get the type of the first base class whose type we know.
 */
const TypeInfo* VarTree::inferTypeFromBaseClass()
{
    if (m_varKind == VKstruct) {
	for (int i = 0; i < childCount(); i++)
	{
	    VarTree* child = VarTree::child(i);
	    // only check base class parts (i.e. type names)
	    if (child->m_nameKind != NKtype)
		break;
	    if (child->m_type != 0 &&
		child->m_type != TypeInfo::unknownType())
	    {
		// got a type!
		return child->m_type;
	    }
	}
    }
    return 0;
}

ExprValue::ExprValue(const QString& name, VarTree::NameKind aKind) :
	m_name(name),
	m_varKind(VarTree::VKsimple),
	m_nameKind(aKind),
	m_child(0),
	m_next(0),
	m_initiallyExpanded(false)
{
}

ExprValue::~ExprValue()
{
    delete m_child;
    delete m_next;
}

void ExprValue::appendChild(ExprValue* newChild)
{
    if (m_child == 0) {
	m_child = newChild;
    } else {
	// walk chain of children to find the last one
	ExprValue* last = m_child;
	while (last->m_next != 0)
	    last = last->m_next;
	last->m_next = newChild;
    }
    newChild->m_next = 0;	// just to be sure
}

int ExprValue::childCount() const
{
    int i = 0;
    ExprValue* c = m_child;
    while (c) {
	++i;
	c = c->m_next;
    }
    return i;
}



ExprWnd::ExprWnd(QWidget* parent, const QString& colHeader) :
	QTreeWidget(parent),
	m_edit(0)
{
    QTreeWidgetItem* pHeaderItem = new QTreeWidgetItem();
    pHeaderItem->setText(0, colHeader);
    pHeaderItem->setText(1, i18n("Value"));
    setHeaderItem(pHeaderItem);
    header()->setResizeMode(0, QHeaderView::Interactive);
    header()->setResizeMode(1, QHeaderView::Interactive);

    setSortingEnabled(false);		// do not sort items
    setRootIsDecorated(true);
    setAllColumnsShowFocus(true);

    m_pixPointer = UserIcon("pointer.xpm");
    if (m_pixPointer.isNull())
	TRACE("Can't load pointer.xpm");
}

ExprWnd::~ExprWnd()
{
}

QStringList ExprWnd::exprList() const
{
    QStringList exprs;
    for (int i = 0; i < topLevelItemCount(); i++)
    {
	exprs.append(topLevelItem(i)->getText());
    }
    return exprs;
}

VarTree* ExprWnd::insertExpr(ExprValue* expr, ProgramTypeTable& typeTable)
{
    // append a new dummy expression
    VarTree* display = new VarTree(this, expr);

    // replace it right away
    updateExpr(display, expr, typeTable);
    return display;
}

void ExprWnd::updateExpr(ExprValue* expr, ProgramTypeTable& typeTable)
{
    // search the root variable
    VarTree* item = 0;
    for (int i = 0; i < topLevelItemCount(); i++)
    {
	if (topLevelItem(i)->getText() == expr->m_name) {
	    item = topLevelItem(i);
	    break;
	}
    }
    if (item == 0) {
	return;
    }
    // now update it
    updateExprRec(item, expr, typeTable);
    collectUnknownTypes(item);
}

void ExprWnd::updateExpr(VarTree* display, ExprValue* newValues, ProgramTypeTable& typeTable)
{
    updateExprRec(display, newValues, typeTable);
    collectUnknownTypes(display);
}

/*
 * returns true if there's a visible change
 */
void ExprWnd::updateExprRec(VarTree* display, ExprValue* newValues, ProgramTypeTable& typeTable)
{
    bool isExpanded = display->isExpanded();

    /*
     * If we are updating a pointer without children by a dummy, we don't
     * collapse it, but simply insert the new children. This happens when a
     * pointer has just been expanded by the user.
     */
    if (display->m_varKind == VarTree::VKpointer &&
	display->childCount() == 0 &&
	newValues->m_varKind == VarTree::VKdummy)
    {
	replaceChildren(display, newValues);
	return;
    }

    /*
     * If the display and newValues have different kind or if their number
     * of children is different, replace the whole sub-tree.
     */
    if (// the next two lines mean: not(m_varKind remains unchanged)
	!(newValues->m_varKind == VarTree::VKdummy ||
	  display->m_varKind == newValues->m_varKind)
	||
	(display->childCount() != newValues->childCount() &&
	 /*
	  * If this is a pointer and newValues doesn't have children, we
	  * don't replace the sub-tree; instead, below we mark this
	  * sub-tree for requiring an update.
	  */
	 (display->m_varKind != VarTree::VKpointer ||
	  newValues->m_child != 0)))
    {
	if (isExpanded) {
	    display->setExpanded(false);
	}

	// since children changed, it is likely that the type has also changed
	display->m_type = 0;		/* will re-evaluate the type */

	// display the new value
	updateSingleExpr(display, newValues);
	replaceChildren(display, newValues);

	// update the m_varKind
	if (newValues->m_varKind != VarTree::VKdummy) {
	    display->m_varKind = newValues->m_varKind;
	    if (newValues->m_child != 0 || newValues->m_varKind == VarTree::VKpointer)
		display->setChildIndicatorPolicy(QTreeWidgetItem::ShowIndicator);
	    else
		display->setChildIndicatorPolicy(QTreeWidgetItem::DontShowIndicator);
	}

	// get some types (after the new m_varKind has been set!)
	display->inferTypesOfChildren(typeTable);

	// (note that the new value might not have a sub-tree at all)
	return;
    }

    // display the new value
    updateSingleExpr(display, newValues);

    /*
     * If this is an expanded pointer, record it for being updated.
     */
    if (display->m_varKind == VarTree::VKpointer) {
	if (isExpanded &&
	    // if newValues is a dummy, we have already updated this pointer
	    newValues->m_varKind != VarTree::VKdummy)
	{
	    m_updatePtrs.push_back(display);
	}
	/*
	 * If the visible sub-tree has children, but newValues doesn't, we
	 * can stop here.
	 */
	if (newValues->m_child == 0) {
	    return;
	}
    }

    ASSERT(display->childCount() == newValues->childCount());

    // go for children
    ExprValue* vNew = newValues->m_child;
    for (int i = 0; i < display->childCount(); i++)
    {
	VarTree* vDisplay = display->child(i);
	// check whether the names are the same
	if (vDisplay->getText() != vNew->m_name) {
	    // set new name
	    vDisplay->setText(vNew->m_name);
	}
	// recurse
	updateExprRec(vDisplay, vNew, typeTable);

	vNew = vNew->m_next;
    }
}

void ExprWnd::updateSingleExpr(VarTree* display, ExprValue* newValue)
{
    /*
     * If newValues is a VKdummy, we are only interested in its children.
     * No need to update anything here.
     */
    if (newValue->m_varKind == VarTree::VKdummy) {
	return;
    }

    /*
     * If this node is a struct and we know its type then we know how to
     * find a nested value. So register the node for an update.
     *
     * wchar_t types are also treated specially here: We consider them
     * as struct (has been set in inferTypesOfChildren()).
     */
    if (display->m_varKind == VarTree::VKstruct &&
	display->m_type != 0 &&
	display->m_type != TypeInfo::unknownType())
    {
	ASSERT(newValue->m_varKind == VarTree::VKstruct);
	if (display->m_type == TypeInfo::wchartType())
	{
	    display->m_partialValue = "L";
	}
	else
	    display->m_partialValue = display->m_type->m_displayString[0];
	m_updateStruct.push_back(display);
    }

    display->updateValue(newValue->m_value);
}

void ExprWnd::updateStructValue(VarTree* display)
{
    ASSERT(display->m_varKind == VarTree::VKstruct);

    display->updateStructValue(display->m_partialValue);
    // reset the value
    display->m_partialValue = "";
    display->m_exprIndex = -1;
}

void ExprWnd::replaceChildren(VarTree* display, ExprValue* newValues)
{
    ASSERT(display->childCount() == 0 || display->m_varKind != VarTree::VKsimple);

    // delete all children of display
    while (VarTree* c = display->child(0)) {
	unhookSubtree(c);
	delete c;
    }
    // insert copies of the newValues
    for (ExprValue* v = newValues->m_child; v != 0; v = v->m_next)
    {
	VarTree* vNew = new VarTree(display, v);
	// recurse
	replaceChildren(vNew, v);
    }
}

void ExprWnd::collectUnknownTypes(VarTree* var)
{
    QTreeWidgetItemIterator i(var);
    for (; *i; ++i)
    {
	checkUnknownType(static_cast<VarTree*>(*i));
    }
}

void ExprWnd::checkUnknownType(VarTree* var)
{
    ASSERT(var->m_varKind != VarTree::VKpointer || var->m_nameKind != VarTree::NKtype);
    if (var->m_type == 0 &&
	var->m_varKind == VarTree::VKstruct &&
	var->m_nameKind != VarTree::NKtype &&
	var->m_nameKind != VarTree::NKanonymous)
    {
	if (!var->isWcharT())
	{
	    /* this struct node doesn't have a type yet: register it */
	    m_updateType.push_back(var);
	}
	else
	{
	    var->m_type = TypeInfo::wchartType();
	    var->m_partialValue = "L";
	    m_updateStruct.push_back(var);
	}
    }
    // add pointer pixmap to pointers
    if (var->m_varKind == VarTree::VKpointer) {
	var->setPixmap(m_pixPointer);
    }
}

QString ExprWnd::formatWCharPointer(QString value)
{
    int pos = value.indexOf(") ");
    if (pos > 0)
	value = value.mid(pos+2);
    return value + " L";
}


VarTree* ExprWnd::topLevelExprByName(const QString& name) const
{
    for (int i = 0; i < topLevelItemCount(); i++)
    {
	if (topLevelItem(i)->getText() == name)
	    return topLevelItem(i);
    }
    return 0;
}

VarTree* ExprWnd::ptrMemberByName(VarTree* v, const QString& name)
{
    // v must be a pointer variable, must have children
    if (v->m_varKind != VarTree::VKpointer || v->childCount() == 0)
	return 0;

    // the only child of v is the pointer value that represents the struct
    VarTree* item = v->child(0);
    return memberByName(item, name);
}

VarTree* ExprWnd::memberByName(VarTree* v, const QString& name)
{
    // search immediate children for name
    for (int i = 0; i < v->childCount(); i++)
    {
	if (v->child(i)->getText() == name)
	    return v->child(i);
    }

    // try in base classes and members that are anonymous structs or unions
    for (int i = 0; i < v->childCount(); i++)
    {
	VarTree* item = v->child(i);
	if (item->m_nameKind == VarTree::NKtype ||
	    item->m_nameKind == VarTree::NKanonymous)
	{
	    item = memberByName(item, name);
	    if (item != 0)
		return item;
	}
    }
    return 0;
}

void ExprWnd::removeExpr(VarTree* item)
{
    unhookSubtree(item);

    delete item;
}

void ExprWnd::unhookSubtree(VarTree* subTree)
{
    // must remove any pointers scheduled for update from the list
    unhookSubtree(m_updatePtrs, subTree);
    unhookSubtree(m_updateType, subTree);
    unhookSubtree(m_updateStruct, subTree);
    emit removingItem(subTree);
}

void ExprWnd::unhookSubtree(std::list<VarTree*>& list, VarTree* subTree)
{
    if (subTree == 0)
	return;

    std::list<VarTree*>::iterator i = list.begin();
    while (i != list.end()) {
	std::list<VarTree*>::iterator checkItem = i;
	++i;
	if (subTree->isAncestorEq(*checkItem)) {
	    // checkItem is an item from subTree
	    list.erase(checkItem);
	}
    }
}

void ExprWnd::clearPendingUpdates()
{
    m_updatePtrs.clear();
    m_updateType.clear();
    m_updateStruct.clear();
}

VarTree* ExprWnd::nextUpdatePtr()
{
    VarTree* ptr = 0;
    if (!m_updatePtrs.empty()) {
	ptr = m_updatePtrs.front();
	m_updatePtrs.pop_front();
    }
    return ptr;
}

VarTree* ExprWnd::nextUpdateType()
{
    VarTree* ptr = 0;
    if (!m_updateType.empty()) {
	ptr = m_updateType.front();
	m_updateType.pop_front();
    }
    return ptr;
}

VarTree* ExprWnd::nextUpdateStruct()
{
    VarTree* ptr = 0;
    if (!m_updateStruct.empty()) {
	ptr = m_updateStruct.front();
	m_updateStruct.pop_front();
    }
    return ptr;
}


void ExprWnd::editValue(VarTree* item, const QString& text)
{
    if (m_edit == 0)
	m_edit = new ValueEdit(this);

    QRect r = visualItemRect(item);
    int x = columnViewportPosition(1);
    int y = r.y();
    int w = columnWidth(1);
    int h = r.height();

    /*
     * Make the edit widget at least 5 characters wide (but not wider than
     * this widget). If less than half of this widget is used to display
     * the text, scroll this widget so that half of it shows the text (or
     * less than half of it if the text is shorter).
     */
    QFontMetrics metr = m_edit->font();
    int wMin = metr.width("88888");
    if (w < wMin)
	w = wMin;
    int wThis = viewport()->width();
    if (x >= wThis/2 &&		// less than half the width displays text
	x+w > wThis)		// not all text is visible
    {
	// scroll so that more text is visible
	QScrollBar* pScrollBar = horizontalScrollBar();
	int wScroll = qMin(x-wThis/2, x+w-wThis);
	pScrollBar->setValue(pScrollBar->value() + wScroll);
	x -= wScroll;
    }
    else if (x < 0)
    {
	// don't let the edit move out at the left
	x = 0;
    }

    // make the edit box as wide as the visible column
    QRect rect(x,y, wThis-x,h);
    m_edit->setText(text);
    m_edit->selectAll();

    m_edit->setGeometry(rect);
    m_edit->m_finished = false;
    m_edit->m_item = item;
    m_edit->show();
    m_edit->setFocus();
}

bool ExprWnd::isEditing() const
{
    return m_edit != 0 && m_edit->isVisible();
}


ValueEdit::ValueEdit(ExprWnd* parent) :
	QLineEdit(parent->viewport())
{
    setFrame(false);
    hide();
    lower();	// lower the window below scrollbars
    connect(parent, SIGNAL(itemActivated(QTreeWidgetItem*,int)),
	    SLOT(slotSelectionChanged()));
    connect(parent, SIGNAL(currentItemChanged(QTreeWidgetItem*,QTreeWidgetItem*)),
	    SLOT(slotSelectionChanged()));
    connect(parent, SIGNAL(itemExpanded(QTreeWidgetItem*)),
	    SLOT(slotSelectionChanged()));
    connect(parent, SIGNAL(itemCollapsed(QTreeWidgetItem*)),
	    SLOT(slotSelectionChanged()));
    connect(this, SIGNAL(done(VarTree*, const QString&)),
	    parent, SIGNAL(editValueCommitted(VarTree*, const QString&)));
}

ValueEdit::~ValueEdit()
{
}

void ValueEdit::terminate(bool commit)
{
    TRACE(commit?"ValueEdit::terminate(true)":"ValueEdit::terminate(false)");
    if (!m_finished)
    {
	m_finished = true;
	hide();	// will call focusOutEvent, that's why we need m_finished
	if (commit) {
	    emit done(m_item, text());
	}
    }
}

void ValueEdit::keyPressEvent(QKeyEvent *e)
{
    if(e->key() == Qt::Key_Return || e->key() == Qt::Key_Enter)
	terminate(true);
    else if(e->key() == Qt::Key_Escape)
	terminate(false);
    else
	QLineEdit::keyPressEvent(e);
}

void ValueEdit::paintEvent(QPaintEvent* e)
{
    QLineEdit::paintEvent(e);

    QPainter p(this);
    p.drawRect(rect());
}

void ValueEdit::focusOutEvent(QFocusEvent* ev)
{
    TRACE("ValueEdit::focusOutEvent");
    QFocusEvent* focusEv = static_cast<QFocusEvent*>(ev);
    if (focusEv->reason() == Qt::ActiveWindowFocusReason)
    {
	// Switching to a different window should terminate the edit,
	// because if the window with this variable display is floating
	// then that different window could be the main window, where
	// the user had clicked one of the Execute buttons. This in turn
	// may pull the item away that we are editing here.
	terminate(false);
    }
    // Don't let a RMB close the editor
    else if (focusEv->reason() != Qt::PopupFocusReason)
    {
	terminate(true);
    }
}

void ValueEdit::slotSelectionChanged()
{   
    TRACE("ValueEdit::slotSelectionChanged");
    terminate(false);
}
