// $Id$

// Copyright by Johannes Sixt
// This file is under GPL, the GNU General Public Licence

#include "exprwnd.h"
#include "exprwnd.moc"
#include "parsevar.h"
#include <qstrlist.h>
#include <qpainter.h>
#include <kapp.h>
#include <kiconloader.h>		/* icons */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include <kdebug.h>

VarTree::VarTree(const QString& name, NameKind aKind) :
	KTreeViewItem(name),
	m_varKind(VKsimple),
	m_nameKind(aKind),
	m_value(100),			/* reserve some space */
	m_valueChanged(false)
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
    int textY = cellHeight - ((cellHeight - p->fontMetrics().ascent() - 
			       p->fontMetrics().leading()) / 2);
    if (m_valueChanged) {
	p->setPen(red);
    }
//    p->setBackgroundColor(cg.base());
    p->drawText(textX, textY, m_value, m_value.length());
    p->restore();
}

int VarTree::valueWidth(KTreeView* owner)
{
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
	result = "(" + parentExpr + ")" + getText();
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


ExprWnd::ExprWnd(QWidget* parent, const char* name) :
	KTreeView(parent, name),
	maxValueWidth(0)
{
    setNumCols(2);
    clearTableFlags(Tbl_clipCellPainting);
    
    connect(this, SIGNAL(expanded(int)), SLOT(slotExpandOrCollapse(int)));
    connect(this, SIGNAL(collapsed(int)), SLOT(slotExpandOrCollapse(int)));

    KIconLoader* loader = kapp->getIconLoader();
    m_pixPointer = loader->loadIcon("pointer.xpm");
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

    updateValuesWidth();
}

void ExprWnd::updateExpr(VarTree* expr)
{
    // search the root variable
    QString p = expr->getText();
    KPath path;
    path.push(&p);
    KTreeViewItem* item = itemAt(path);
    path.pop();
    if (item == 0) {
	return;
    }
    // now update it
    if (updateExprRec(static_cast<VarTree*>(item), expr)) {
	updateVisibleItems();
	updateValuesWidth();
	repaint();
    }
}

void ExprWnd::updateExpr(VarTree* display, VarTree* newValues)
{
    if (updateExprRec(display, newValues) && display->isVisible()) {
	updateVisibleItems();
	updateValuesWidth();
	repaint();
    }
}

/*
 * returns true if there's a visible change
 */
bool ExprWnd::updateExprRec(VarTree* display, VarTree* newValues)
{
    bool isExpanded = display->isExpanded();
    /*
     * If newValues is a VKdummy, we are only interested in its children.
     */
    if (newValues->m_varKind != VarTree::VKdummy) {
	// check whether the value changed
	bool prevValueChanged = display->m_valueChanged;
	display->m_valueChanged = false;
	if (display->m_value != newValues->m_value) {
	    display->m_value = newValues->m_value;
	    display->m_valueChanged = true;
	}
	/*
	 * We must repaint the cell if the value changed. If it did not
	 * change, we still must repaint the cell if the value changed
	 * previously, because the color of the display must be changed
	 * (from red to black).
	 */
	if (display->m_valueChanged || prevValueChanged) {
	    int i = itemRow(display);
	    if (i >= 0) {
		updateCell(i, 1, true);
	    }
	}
    }

    /*
     * If we are updating a pointer without children by a dummy, we don't
     * collaps it and simply insert the new children.
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
	    collapseSubTree(display);
	}
	// update the m_varKind
	if (newValues->m_varKind != VarTree::VKdummy) {
	    display->m_varKind = newValues->m_varKind;
	    display->setDelayedExpanding(newValues->m_varKind == VarTree::VKpointer);
	}

	replaceChildren(display, newValues);

	// (note that the new value might not have a sub-tree at all)
	return display->m_valueChanged || isExpanded;	/* no visible change if not expanded */
    }
    
    /*
     * If this is an expanded pointer, record it for being updated.
     */
    if (display->m_varKind == VarTree::VKpointer) {
	if (isExpanded &&
	    // if newValues is a dummy, we have already updated this pointer
	    newValues->m_varKind != VarTree::VKdummy)
	{
	    m_updatePtrs.append(display);
	    display->setPixmap(m_pixPointer);
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
    return display->m_valueChanged || (isExpanded && childChanged);
}

void ExprWnd::replaceChildren(VarTree* display, VarTree* newValues)
{
    ASSERT(display->childCount() == 0 || display->m_varKind != VarTree::VKsimple);

    // delete all children of display
    KTreeViewItem* c;
    while ((c = display->getChild()) != 0) {
	display->removeChild(c);
    }
    // insert copies of the newValues
    for (c = newValues->getChild(); c != 0; c = c->getSibling()) {
	VarTree* v = static_cast<VarTree*>(c);
	VarTree* vNew = new VarTree(v->getText(), v->m_nameKind);
	vNew->m_varKind = v->m_varKind;
	vNew->m_value = v->m_value;
	vNew->setDelayedExpanding(vNew->m_varKind == VarTree::VKpointer);
	display->appendChild(vNew);
	// recurse
	replaceChildren(vNew, v);
    }
}

void ExprWnd::removeExpr(const char* name)
{
    QString p = name;
    KPath path;
    path.push(&p);

    // must remove any pointers scheduled for update from the list
    KTreeViewItem* item = itemAt(path);
    if (item != 0) {
	KTreeViewItem* checkItem = m_updatePtrs.first();
	while (checkItem != 0) {
	    KTreeViewItem* p = checkItem;
	    while (p != 0 && p != item) {
		p = p->getParent();
	    }
	    if (p == 0) {
		// checkItem is not a subitem of item
		// advance
		checkItem = m_updatePtrs.next();
	    } else {
		// checkItem is a subitem of item
		/* 
		 * If checkItem is the last item in the list, we need a
		 * special treatment, because remove()ing it steps the
		 * current item of the list in the "wrong" direction.
		 */
		if (checkItem == m_updatePtrs.getLast()) { // does not set current item
		    m_updatePtrs.remove();
		    /* we deleted the last element, so we've finished */
		    checkItem = 0;
		} else {
		    m_updatePtrs.remove();
		    /* remove() advanced already */
		    checkItem = m_updatePtrs.current();
		}
	    }
	}
    }

    removeItem(path);
    path.pop();

    updateValuesWidth();
}

void ExprWnd::clearUpdatePtrs()
{
    m_updatePtrs.clear();
}

VarTree* ExprWnd::nextUpdatePtr()
{
    VarTree* ptr = m_updatePtrs.first();
    if (ptr != 0) {
	m_updatePtrs.remove();
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

int ExprWnd::cellWidth(int col)
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
  forEveryVisibleItem((KForEveryM)&getMaxValueWidth, &maxW);
  maxValueWidth = maxW;
  updateTableSize();
}

// called by updateValuesWidth() for each item in the visible list
bool ExprWnd::getMaxValueWidth(KTreeViewItem *item, void *user)
{
    int *maxW = (int *)user;
    VarTree* v = static_cast<VarTree*>(item);
    int w = v->valueWidth(this);
    if(w > *maxW)
	*maxW = w;
    return false;
}

void ExprWnd::slotExpandOrCollapse(int)
{
    updateValuesWidth();
}
