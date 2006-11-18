// $Id$

// Copyright by Johannes Sixt
// This file is under GPL, the GNU General Public Licence

#include "exprwnd.h"
#include "exprwnd.moc"
#include "typetable.h"
#include <qstrlist.h>
#include <qpainter.h>
#include <qscrollbar.h>
#include <kapp.h>
#include <kiconloader.h>		/* icons */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include "mydebug.h"

VarTree::VarTree(const QString& name, NameKind aKind) :
	KTreeViewItem(name),
	m_varKind(VKsimple),
	m_nameKind(aKind),
	m_valueChanged(false),
	m_type(0),
	m_exprIndex(0),
	m_exprIndexUseGuard(false)
{
}

VarTree::~VarTree()
{
}

void VarTree::paintValue(QPainter* p)
{
    p->save();
    int cellHeight = height(p->fontMetrics());
    int textX = 2;
    int textY = (cellHeight - p->fontMetrics().height()) / 2 +
	p->fontMetrics().ascent();

    if (m_valueChanged) {
	p->setPen(red);
    }
//    p->setBackgroundColor(cg.base());
    p->drawText(textX, textY, m_value, m_value.length());
    p->restore();
}

int VarTree::valueWidth()
{
    assert(owner != 0);
    return owner->fontMetrics().width(m_value) + 4;
}

QString VarTree::computeExpr() const
{
    assert(getParent() != 0);
    // just to be sure
    if (getParent() == 0)
	return QString();

    // top-level items are special
    if (getParent()->getParent() == 0)
	return getText();

    // get parent expr
    VarTree* par = static_cast<VarTree*>(getParent());
    QString parentExpr = par->computeExpr();

    /* don't add this item's name if this is a base class sub-item */
    if (m_nameKind == NKtype) {
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
    return getParent() != 0 && getParent()->getParent() == 0;
}

bool VarTree::isAncestorEq(const VarTree* child) const
{
    const KTreeViewItem* c = child;
    while (c != 0 && c != this) {
	c = c->getParent();
    }
    return c != 0;
}

bool VarTree::updateValue(const QString& newValue)
{
    // check whether the value changed
    bool prevValueChanged = m_valueChanged;
    m_valueChanged = false;
    if (m_value != newValue) {
	m_value = newValue;
	m_valueChanged = true;
    }
    /*
     * We must repaint the cell if the value changed. If it did not change,
     * we still must repaint the cell if the value changed previously,
     * because the color of the display must be changed (from red to
     * black).
     */
    return m_valueChanged || prevValueChanged;
}

void VarTree::inferTypesOfChildren(ProgramTypeTable& typeTable)
{
    /*
     * Type inference works like this: We use type information of those
     * children that have a type name in their name (base classes) or in
     * their value (pointers)
     */

    // first recurse children
    VarTree* child = static_cast<VarTree*>(getChild());
    while (child != 0) {
	child->inferTypesOfChildren(typeTable);
	child = static_cast<VarTree*>(child->getSibling());
    }

    // if this is a pointer, get the type from the value (less the pointer)
    if (m_varKind == VKpointer) {
#ifndef I_know_a_way_to_do_this_cleanly
	return;
#else
	const char* p = m_value.data();
	const char* start = p;
	// the type of the pointer shows up in the value (sometimes)
	if (p == 0 || *p != '(')
	    return;
	skipNested(p, '(', ')');
	/*
	 * We only recognize pointers to data "(int *)" but not pointers
	 * to functions "(void (*)())".
	 */
	if (p-start < 3 &&		/* at least 3 chars necessary: (*) */
	    p[-2] != '*')		/* skip back before the closing paren */
	{
	    return;
	}
	const QString& typeName =
	    FROM_LATIN1(start+1, p-start-3) // minus 3 chars
		.stripWhiteSpace();
	m_type = typeTable.lookup(typeName);
	if (m_type == 0) {
	    m_type = TypeInfo::unknownType();
	}
#endif
    } else if (m_varKind == VKstruct) {
	// check if this is a base class part
	if (m_nameKind == NKtype) {
	    const QString& typeName =
		text.mid(1, text.length()-2); // strip < and >
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

/*
 * Get the type of the first base class whose type we know.
 */
TypeInfo* VarTree::inferTypeFromBaseClass()
{
    if (m_varKind == VKstruct) {
	VarTree* child = static_cast<VarTree*>(getChild());
	while (child != 0 &&
	       // only check base class parts (i.e. type names)
	       child->m_nameKind == NKtype)
	{
	    if (child->m_type != 0 &&
		child->m_type != TypeInfo::unknownType())
	    {
		// got a type!
		return child->m_type;
	    }
	    child = static_cast<VarTree*>(child->getSibling());
	}
    }
    return 0;
}


ExprWnd::ExprWnd(QWidget* parent, const char* name) :
	KTreeView(parent, name),
	maxValueWidth(0),
	m_edit(this)
{
    setNumCols(2);
    
    connect(this, SIGNAL(expanded(int)), SLOT(slotExpandOrCollapse(int)));
    connect(this, SIGNAL(collapsed(int)), SLOT(slotExpandOrCollapse(int)));

    m_pixPointer = UserIcon("pointer.xpm");
    if (m_pixPointer.isNull())
	TRACE("Can't load pointer.xpm");
}

ExprWnd::~ExprWnd()
{
}

void ExprWnd::exprList(QStrList& exprs)
{
    // ASSERT(exprs does deep-copies)
    KTreeViewItem* item;
    for (item = itemAt(0); item != 0; item = item->getSibling()) {
	exprs.append(item->getText());
    }
}

void ExprWnd::insertExpr(VarTree* expr)
{
    // append the expression
    insertItem(expr);

    collectUnknownTypes(expr);

    updateValuesWidth();
}

void ExprWnd::updateExpr(VarTree* expr)
{
    // search the root variable
    KPath path;
    path.push(expr->getText());
    KTreeViewItem* item = itemAt(path);
    if (item == 0) {
	return;
    }
    // now update it
    if (updateExprRec(static_cast<VarTree*>(item), expr)) {
	updateVisibleItems();
	updateValuesWidth();
	repaint();
    }
    collectUnknownTypes(static_cast<VarTree*>(item));
}

void ExprWnd::updateExpr(VarTree* display, VarTree* newValues)
{
    if (updateExprRec(display, newValues) && display->isVisible()) {
	updateVisibleItems();
	updateValuesWidth();
	repaint();
    }
    collectUnknownTypes(display);
}

/*
 * returns true if there's a visible change
 */
bool ExprWnd::updateExprRec(VarTree* display, VarTree* newValues)
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
	return isExpanded;		/* no visible change if not expanded */
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
	  newValues->childCount() != 0)))
    {
	if (isExpanded) {
	    collapseSubTree(display, false);
	}

	// since children changed, it is likely that the type has also changed
	display->m_type = 0;		/* will re-evaluate the type */

	// display the new value
	updateSingleExpr(display, newValues);
	replaceChildren(display, newValues);

	// update the m_varKind
	if (newValues->m_varKind != VarTree::VKdummy) {
	    display->m_varKind = newValues->m_varKind;
	    display->setDelayedExpanding(newValues->m_varKind == VarTree::VKpointer);
	}

	// (note that the new value might not have a sub-tree at all)
	return display->m_valueChanged || isExpanded;	/* no visible change if not expanded */
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
	    m_updatePtrs.append(display);
	}
	/*
	 * If the visible sub-tree has children, but newValues doesn't, we
	 * can stop here.
	 */
	if (newValues->childCount() == 0) {
	    return display->m_valueChanged;
	}
    }

    ASSERT(display->childCount() == newValues->childCount());

    // go for children
    bool childChanged = false;

    VarTree* vDisplay = static_cast<VarTree*>(display->getChild());
    VarTree* vNew = static_cast<VarTree*>(newValues->getChild());
    while (vDisplay != 0) {
	// check whether the names are the same
	if (strcmp(vDisplay->getText(), vNew->getText()) != 0) {
	    // set new name
	    vDisplay->setText(vNew->getText());
	    int i = itemRow(vDisplay);
	    if (i >= 0) {
		updateCell(i, 0, true);
		childChanged = true;
	    }
	}
	// recurse
	if (updateExprRec(vDisplay, vNew)) {
	    childChanged = true;
	}
	vDisplay = static_cast<VarTree*>(vDisplay->getSibling());
	vNew = static_cast<VarTree*>(vNew->getSibling());
    }

    // update of children propagates only if this node is expanded
    return display->m_valueChanged || (display->isExpanded() && childChanged);
}

void ExprWnd::updateSingleExpr(VarTree* display, VarTree* newValue)
{
    /*
     * If newValues is a VKdummy, we are only interested in its children.
     * No need to update anything here.
     */
    if (newValue->m_varKind == VarTree::VKdummy) {
	return;
    }

    /*
     * If this node is a struct and we know its type then don't update its
     * value now. This is a node for which we know how to find a nested
     * value. So register the node for an update.
     */
    if (display->m_varKind == VarTree::VKstruct &&
	display->m_type != 0 &&
	display->m_type != TypeInfo::unknownType())
    {
	ASSERT(newValue->m_varKind == VarTree::VKstruct);
	display->m_partialValue = display->m_type->m_displayString[0];
	m_updateStruct.append(display);
    }
    else
    {
	if (display->updateValue(newValue->m_value)) {
	    int i = itemRow(display);
	    if (i >= 0) {
		updateCell(i, 1, true);
	    }
	}
    }
}

void ExprWnd::updateStructValue(VarTree* display)
{
    ASSERT(display->m_varKind == VarTree::VKstruct);

    if (display->updateValue(display->m_partialValue)) {
	int i = itemRow(display);
	if (i >= 0) {
	    updateValuesWidth();
	    updateCell(i, 1, true);
	}
    }
    // reset the value
    display->m_partialValue = "";
    display->m_exprIndex = -1;
}

void ExprWnd::replaceChildren(VarTree* display, VarTree* newValues)
{
    ASSERT(display->childCount() == 0 || display->m_varKind != VarTree::VKsimple);

    // delete all children of display
    while (VarTree* c = static_cast<VarTree*>(display->getChild())) {
	unhookSubtree(c);
	display->removeChild(c);
	delete c;
    }
    // insert copies of the newValues
    for (KTreeViewItem* c = newValues->getChild(); c != 0; c = c->getSibling()) {
	VarTree* v = static_cast<VarTree*>(c);
	VarTree* vNew = new VarTree(v->getText(), v->m_nameKind);
	vNew->m_varKind = v->m_varKind;
	vNew->m_value = v->m_value;
	vNew->m_type = v->m_type;
	vNew->setDelayedExpanding(vNew->m_varKind == VarTree::VKpointer);
	vNew->setExpanded(v->isExpanded());
	display->appendChild(vNew);
	// recurse
	replaceChildren(vNew, v);
    }
}

void ExprWnd::collectUnknownTypes(VarTree* var)
{
    /*
     * forEveryItem does not scan the root item itself. So we must do it
     * ourselves.
     */
    ASSERT(var->m_varKind != VarTree::VKpointer || var->m_nameKind != VarTree::NKtype);
    if (var->m_type == 0 &&
	var->m_varKind == VarTree::VKstruct &&
	var->m_nameKind != VarTree::NKtype)
    {
	/* this struct node doesn't have a type yet: register it */
	m_updateType.append(var);
    }

    // add pointer pixmap to pointers
    if (var->m_varKind == VarTree::VKpointer) {
	var->setPixmap(m_pixPointer);
    }

    forEveryItem(collectUnknownTypes, this, var);
}

bool ExprWnd::collectUnknownTypes(KTreeViewItem* item, void* user)
{
    VarTree* var = static_cast<VarTree*>(item);
    ExprWnd* tree = static_cast<ExprWnd*>(user);
    ASSERT(var->m_varKind != VarTree::VKpointer || var->m_nameKind != VarTree::NKtype);
    if (var->m_type == 0 &&
	var->m_varKind == VarTree::VKstruct &&
	var->m_nameKind != VarTree::NKtype)
    {
	/* this struct node doesn't have a type yet: register it */
	tree->m_updateType.append(var);
    }
    // add pointer pixmap to pointers
    if (var->m_varKind == VarTree::VKpointer) {
	var->setPixmap(tree->m_pixPointer);
    }
    return false;
}


VarTree* ExprWnd::topLevelExprByName(const char* name)
{
    KPath path;
    path.push(name);
    KTreeViewItem* item = itemAt(path);

    return static_cast<VarTree*>(item);
}

VarTree* ExprWnd::ptrMemberByName(VarTree* v, const QString& name)
{
    // v must be a pointer variable, must have children
    if (v->m_varKind != VarTree::VKpointer || v->childCount() == 0)
	return 0;

    // the only child of v is the pointer value that represents the struct
    KTreeViewItem* item = v->getChild();
    return memberByName(static_cast<VarTree*>(item), name);
}

VarTree* ExprWnd::memberByName(VarTree* v, const QString& name)
{
    // search immediate children for name
    KTreeViewItem* item = v->getChild();
    while (item != 0 && item->getText() != name)
	item = item->getSibling();

    if (item != 0)
	return static_cast<VarTree*>(item);

    // try in base classes
    item = v->getChild();
    while (item != 0 &&
	   static_cast<VarTree*>(item)->m_nameKind == VarTree::NKtype)
    {
	v = memberByName(static_cast<VarTree*>(item), name);
	if (v != 0)
	    return v;
	item = item->getSibling();
    }
    return 0;
}

void ExprWnd::removeExpr(VarTree* item)
{
    unhookSubtree(item);

    takeItem(item);
    delete item;

    updateValuesWidth();
}

void ExprWnd::unhookSubtree(VarTree* subTree)
{
    // must remove any pointers scheduled for update from the list
    unhookSubtree(m_updatePtrs, subTree);
    unhookSubtree(m_updateType, subTree);
    unhookSubtree(m_updateStruct, subTree);
    emit removingItem(subTree);
}

void ExprWnd::unhookSubtree(QList<VarTree>& list, VarTree* subTree)
{
    if (subTree == 0)
	return;

    VarTree* checkItem = list.first();
    while (checkItem != 0) {
	if (!subTree->isAncestorEq(checkItem)) {
	    // checkItem is not an item from subTree
	    // advance
	    checkItem = list.next();
	} else {
	    // checkItem is an item from subTree
	    /* 
	     * If checkItem is the last item in the list, we need a special
	     * treatment, because remove()ing it steps the current item of
	     * the list in the "wrong" direction.
	     */
	    if (checkItem == list.getLast()) { // does not set current item
		list.remove();
		/* we deleted the last element, so we've finished */
		checkItem = 0;
	    } else {
		list.remove();
		/* remove() advanced already */
		checkItem = list.current();
	    }
	}
    }
}

QString ExprWnd::exprStringAt(int index)
{
    KTreeViewItem* item = itemAt(index);
    if (item == 0) return QString();	/* paranoia */
    VarTree* expr = static_cast<VarTree*>(item);
    return expr->computeExpr();
}

void ExprWnd::clearPendingUpdates()
{
    m_updatePtrs.clear();
    m_updateType.clear();
    m_updateStruct.clear();
}

VarTree* ExprWnd::nextUpdatePtr()
{
    VarTree* ptr = m_updatePtrs.first();
    if (ptr != 0) {
	m_updatePtrs.remove();
    }
    return ptr;
}

VarTree* ExprWnd::nextUpdateType()
{
    VarTree* ptr = m_updateType.first();
    if (ptr != 0) {
	m_updateType.remove();
    }
    return ptr;
}

VarTree* ExprWnd::nextUpdateStruct()
{
    VarTree* ptr = m_updateStruct.first();
    if (ptr != 0) {
	m_updateStruct.remove();
    }
    return ptr;
}

void ExprWnd::paintCell(QPainter* painter, int row, int col)
{
    if (col == 0) {
	KTreeView::paintCell(painter, row, col);
    } else {
	VarTree* item = static_cast<VarTree*>(itemAt(row));
	if (item != 0) {
	    item->paintValue(painter);
	}
    }
}

int ExprWnd::cellWidth(int col) const
{
    if (col == 0) {
	return KTreeView::cellWidth(col);
    } else {
	return maxValueWidth;
    }
}

void ExprWnd::updateValuesWidth()
{
    int maxW = 0;
    forEveryVisibleItem(static_cast<KForEveryFunc>(&getMaxValueWidth), &maxW);
    maxValueWidth = maxW;
    updateTableSize();
}

// called by updateValuesWidth() for each item in the visible list
bool ExprWnd::getMaxValueWidth(KTreeViewItem* item, void* user)
{
    int *maxW = (int *)user;
    VarTree* v = static_cast<VarTree*>(item);
    int w = v->valueWidth();
    if(w > *maxW)
	*maxW = w;
    return false;
}

void ExprWnd::slotExpandOrCollapse(int)
{
    updateValuesWidth();
}

void ExprWnd::editValue(int row, const QString& text)
{
    int x;
    colXPos(1, &x);
    int y;
    rowYPos(row, &y);
    int w = cellWidth(1);
    int h = cellHeight(row);
    QScrollBar* sbV = static_cast<QScrollBar*>(child("table_sbV"));

    /*
     * Make the edit widget at least 5 characters wide (but not wider than
     * this widget). If less than half of this widget is used to display
     * the text, scroll this widget so that half of it shows the text (or
     * less than half of it if the text is shorter).
     */
    QFontMetrics metr = m_edit.font();
    int wMin = metr.width("88888");
    if (w < wMin)
	w = wMin;
    int wThis = width();
    if (sbV->isVisible())	// subtract width of scrollbar
	wThis -= sbV->width();
    if (x >= wThis/2 &&		// less than half the width displays text
	x+w > wThis)		// not all text is visible
    {
	// scroll so that more text is visible
	int wScroll = QMIN(x-wThis/2, x+w-wThis);
	sbHor(xOffset()+wScroll);
	colXPos(1, &x);
    }
    else if (x < 0)
    {
	// don't let the edit move out at the left
	x = 0;
    }

    // make the edit box as wide as the visible column
    QRect rect(x,y, wThis-x,h);
    m_edit.setText(text);
    m_edit.selectAll();

    m_edit.setGeometry(rect);
    m_edit.m_finished = false;
    m_edit.m_row = row;
    m_edit.show();
    m_edit.setFocus();
}

bool ExprWnd::isEditing() const
{
    return m_edit.isVisible();
}


ValueEdit::ValueEdit(ExprWnd* parent) :
	QLineEdit(parent, "valueedit")
{
    setFrame(false);
    hide();
    lower();	// lower the window below scrollbars
    connect(parent, SIGNAL(selected(int)), SLOT(slotSelectionChanged()));
    connect(parent, SIGNAL(collapsed(int)), SLOT(slotSelectionChanged()));
    connect(parent, SIGNAL(expanded(int)), SLOT(slotSelectionChanged()));
    connect(this, SIGNAL(done(int, const QString&)),
	    parent, SIGNAL(editValueCommitted(int, const QString&)));
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
	    emit done(m_row, text());
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
    // Don't let a RMB close the editor
    if (focusEv->reason() != QFocusEvent::Popup &&
	focusEv->reason() != QFocusEvent::ActiveWindow)
    {
	terminate(true);
    }
}

void ValueEdit::slotSelectionChanged()
{   
    TRACE("ValueEdit::slotSelectionChanged");
    terminate(false);
}
