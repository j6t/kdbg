/*
 * $Id$
 *
 * KTreeView implementation
 * 
 * Copyright (C) 1997 Johannes Sixt
 * 
 * based on KTreeList, which is
 * Copyright (C) 1996 Keith Brown and KtSoft
 * 
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 * 
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABLILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details. You should have received a copy
 * of the GNU General Public License along with this program; if not, write
 * to the Free Software Foundation, Inc, 675 Mass Ave, Cambridge, MA 02139,
 * USA.
 */

#include <ktreeview.h>
#include "ktreeview.moc"
#include <qapp.h>			/* used for QApplication::closingDown() */
#include <qkeycode.h>			/* used for keyboard interface */
#include <qpainter.h>			/* used to paint items */
#include <assert.h>

/*
 * -------------------------------------------------------------------
 * 
 * KTreeViewItem
 * 
 * -------------------------------------------------------------------
 */

// constructor
KTreeViewItem::KTreeViewItem(const QString& theText) :
	owner(0),
	numChildren(0),
	doExpandButton(true),
	expanded(false),
	delayedExpanding(false),
	doTree(true),
	doText(true),
	child(0),
	parent(0),
	sibling(0),
	deleteChildren(false)
{
    text = theText;
}

// constructor that takes a pixmap
KTreeViewItem::KTreeViewItem(const QString& theText,
			     const QPixmap& thePixmap) :
	owner(0),
	numChildren(0),
	doExpandButton(true),
	expanded(false),
	delayedExpanding(false),
	doTree(true),
	doText(true),
	child(0),
	parent(0),
	sibling(0),
	deleteChildren(false)
{
    text = theText;
    pixmap = thePixmap;
}

// destructor
KTreeViewItem::~KTreeViewItem()
{
    if (deleteChildren) {
	// remove the children
	KTreeViewItem* i = child;
	while (i) {
	    KTreeViewItem* d = i;
	    i = i->getSibling();
	    delete d;
	}
    }
}

// appends a direct child
void KTreeViewItem::appendChild(KTreeViewItem* newChild)
{
    newChild->parent = this;
    newChild->owner = owner;
    if (!getChild()) {
	child = newChild;
    }
    else {
	KTreeViewItem* lastChild = getChild();
	while (lastChild->hasSibling()) {
	    lastChild = lastChild->getSibling();
	}
	lastChild->sibling = newChild;
    }
    newChild->sibling = 0;
    numChildren++;
}

// returns the bounding rectangle of the item content (not including indent
// and branches) for hit testing
QRect KTreeViewItem::boundingRect(int indent) const
{
    const QFontMetrics& fm = owner->fontMetrics();
    int rectX = indent;
    int rectY = 1;
    int rectW = width(indent, fm) - rectX;
    int rectH = height(fm) - 2;
    return QRect(rectX, rectY, rectW, rectH);
}

// returns the child item at the specified index
KTreeViewItem* KTreeViewItem::childAt(int index) const
{
    if (!hasChild())
	return 0;
    KTreeViewItem* item = getChild();
    while (index > 0 && item != 0) {
	item = item->getSibling();
	index--;
    }
    return item;
}

// returns the number of children this item has
uint KTreeViewItem::childCount() const
{
    return numChildren;
}

/* returns the index of the given child item in this item's branch */
int KTreeViewItem::childIndex(KTreeViewItem* searched) const
{
    assert(searched != 0);
    int index = 0;
    KTreeViewItem* item = getChild();
    while (item != 0 && item != searched) {
	item = item->getSibling();
	index++;
    }
    return item == 0  ?  -1  :  index;
}

// indicates whether mouse press is in expand button
inline bool KTreeViewItem::expandButtonClicked(const QPoint& coord) const
{
  return expandButton.contains(coord);
}

// returns a pointer to first child item
KTreeViewItem* KTreeViewItem::getChild() const
{
    return child;
}

// returns the parent of this item
KTreeViewItem* KTreeViewItem::getParent() const
{
    return parent;
}

// returns reference to the item pixmap
const QPixmap& KTreeViewItem::getPixmap() const
{
    return pixmap;
}

// returns the sibling below this item
KTreeViewItem* KTreeViewItem::getSibling() const
{
    return sibling;
}

// returns a pointer to the item text
const QString& KTreeViewItem::getText() const
{
    return text;
}

// indicates whether this item has any children
bool KTreeViewItem::hasChild() const
{
    return child != 0;
}

// indicates whether this item has a parent
bool KTreeViewItem::hasParent() const
{
    return parent != 0;
}

// indicates whether this item has a sibling below it
bool KTreeViewItem::hasSibling() const
{
    return sibling != 0;
}

int KTreeViewItem::height() const
{
    assert(owner != 0);
    return height(owner->fontMetrics());
}

// returns the maximum height of text and pixmap including margins
// or -1 if item is empty -- SHOULD NEVER BE -1!
int KTreeViewItem::height(const QFontMetrics& fm) const
{
    int maxHeight = pixmap.height();
    int textHeight = fm.ascent() + fm.leading();
    maxHeight = textHeight > maxHeight ? textHeight : maxHeight;
    return maxHeight == 0 ? -1 : maxHeight + 8;
}

// inserts child item at specified index in branch
void KTreeViewItem::insertChild(int index, KTreeViewItem* newChild)
{
    if (index < 0) {
	appendChild(newChild);
	return;
    }
    newChild->parent = this;
    newChild->owner = owner;
    if (index == 0) {
	newChild->sibling = getChild();
	child = newChild;
    }
    else {
	KTreeViewItem* prevItem = getChild();
	KTreeViewItem* nextItem = prevItem->getSibling();
	while (--index > 0 && nextItem) {
	    prevItem = nextItem;
	    nextItem = prevItem->getSibling();
	}
	prevItem->sibling = newChild;
	newChild->sibling = nextItem;
    }
    numChildren++;
}

// indicates whether this item is displayed expanded 
// NOTE: a TRUE response does not necessarily indicate the item 
// has any children
bool KTreeViewItem::isExpanded() const
{
    return expanded;
}

// returns true if all ancestors are expanded
bool KTreeViewItem::isVisible() const
{
    for (KTreeViewItem* p = getParent(); p != 0; p = p->getParent()) {
	if (!p->isExpanded())
	    return false;
    }
    return true;
}

// paint this item, including tree branches and expand button
void KTreeViewItem::paint(QPainter* p, int indent, const QColorGroup& cg,
			  bool highlighted) const
{
    assert(getParent() != 0);		/* can't paint root item */

    p->save();

    int cellHeight = height(p->fontMetrics());

    if (doTree) {
	paintTree(p, indent, cellHeight, cg);
    }

    /*
     * If this item has at least one child and expand button drawing is
     * enabled, set the rect for the expand button for later mouse press
     * bounds checking, and draw the button.
     */
    if (doExpandButton && (child || delayedExpanding)) {
	paintExpandButton(p, indent, cellHeight, cg);
    }

    // now draw the item pixmap and text, if applicable
    int pixmapX = indent;
    int pixmapY = (cellHeight - pixmap.height()) / 2;
    p->drawPixmap(pixmapX, pixmapY, pixmap);

    if (doText) {
	paintText(p, indent, cellHeight, cg, highlighted);
    }
    p->restore();
}

void KTreeViewItem::paintExpandButton(QPainter* p, int indent, int cellHeight,
				      const QColorGroup& cg) const
{
    int parentLeaderX = indent - (22 / 2);
    int cellCenterY = cellHeight / 2;

    expandButton.setRect(parentLeaderX - 4, cellCenterY - 4, 9, 9);
    p->setBrush(cg.base());
    p->setPen(cg.foreground());
    p->drawRect(expandButton);
    if (expanded) {
	p->drawLine(parentLeaderX - 2, cellCenterY, 
		    parentLeaderX + 2, cellCenterY);
    } else {
	p->drawLine(parentLeaderX - 2, cellCenterY,
		    parentLeaderX + 2, cellCenterY);
	p->drawLine(parentLeaderX, cellCenterY - 2, 
		    parentLeaderX, cellCenterY + 2);
    }
    p->setBrush(NoBrush);
}

// paint the highlight 
void KTreeViewItem::paintHighlight(QPainter* p, int indent, const QColorGroup& colorGroup,
				   bool hasFocus, GUIStyle style) const
{
    QColor fc;
    if (style == WindowsStyle)
	fc = darkBlue;			/* hardcoded in Qt */
    else
	fc = colorGroup.text();
    QRect textRect = textBoundingRect(indent);
    int t,l,b,r;
    textRect.coords(&l, &t, &r, &b);
    QRect outerRect;
    outerRect.setCoords(l - 2, t - 2, r + 2, b + 2);
    if (style == WindowsStyle) {	/* Windows style highlight */
	if (hasFocus) {
	    p->fillRect(textRect, fc);	/* highlight background */
	    textRect.setCoords(l - 1, t - 1, r + 1, b + 1);
	    p->setPen(QPen(yellow, 0, DotLine));
	    p->setBackgroundColor(fc);
	    p->setBackgroundMode(OpaqueMode);
	    p->drawRect(textRect);
	    p->setPen(fc);
	    p->drawRect(outerRect);
	} else {
	    p->fillRect(outerRect, fc);	/* highlight background */
	}
    } else {				/* Motif style highlight */
	if (hasFocus) {
	    p->fillRect(textRect, fc);	/* highlight background */
	    p->setPen(fc);
	    p->drawRect(outerRect);
	} else {
	    p->fillRect(outerRect, fc);	/* highlight background */
	}
    }
}

// draw the text, highlighted if requested
void KTreeViewItem::paintText(QPainter* p, int indent, int cellHeight,
			      const QColorGroup& cg, bool highlighted) const
{
    int textX = indent + pixmap.width() + 4;
    int textY = cellHeight - ((cellHeight - p->fontMetrics().ascent() - 
			       p->fontMetrics().leading()) / 2);
    if (highlighted) {
	paintHighlight(p, indent, cg,
		       owner->hasFocus(), owner->style());
	p->setPen(cg.base());
	p->setBackgroundColor(cg.text());
    }
    else {
	p->setPen(cg.text());
	p->setBackgroundColor(cg.base());
    }
    p->drawText(textX, textY, text);
}

// paint the tree structure
void KTreeViewItem::paintTree(QPainter* p, int indent, int cellHeight,
			      const QColorGroup& cg) const
{
    int parentLeaderX = indent - (22 / 2);
    int cellCenterY = cellHeight / 2;
    int cellBottomY = cellHeight - 1;
    int itemLeaderX = indent - 3;

    p->setPen(cg.background());

    /*
     * If this is not the first item in the tree draw the line up
     * towards parent or sibling.
     */
    if (parent->parent != 0 || parent->getChild() != this)
	p->drawLine(parentLeaderX, 0, parentLeaderX, cellCenterY);

    // draw the line down towards sibling
    if (sibling) 
	p->drawLine(parentLeaderX, cellCenterY, parentLeaderX, cellBottomY);

    /*
     * If this item has children or siblings in the tree or is a child of
     * an item other than the root item then draw the little line from the
     * item out to the left.
     */
    if (sibling || (doExpandButton && (child || delayedExpanding)) ||
	parent->parent != 0 ||
	/*
	 * The following handles the case of an item at the end of the
	 * topmost level which doesn't have children.
	 */
	parent->getChild() != this)
    {
	p->drawLine(parentLeaderX, cellCenterY, itemLeaderX, cellCenterY);
    }

    /*
     * If there are siblings of ancestors below, draw our portion of the
     * branches that extend through this cell.
     */
    KTreeViewItem* prevRoot = parent;
    while (prevRoot->getParent() != 0) {  /* while not root item */
	if (prevRoot->hasSibling()) {
	    int sibLeaderX = owner->indentation(prevRoot) - (22 / 2);
	    p->drawLine(sibLeaderX, 0, sibLeaderX, cellBottomY);
	}
	prevRoot = prevRoot->getParent();
    }
}

// removes the given (direct) child from the branch
bool KTreeViewItem::removeChild(KTreeViewItem* theChild)
{
    // search item in list of children
    KTreeViewItem* prevItem = 0;
    KTreeViewItem* toRemove = getChild();
    while (toRemove && toRemove != theChild) {
	prevItem = toRemove;
	toRemove = toRemove->getSibling();
    }

    if (toRemove) {
	// found it!
	if (prevItem == 0) {
	    child = toRemove->getSibling();
	} else {
	    prevItem->sibling = toRemove->getSibling();
	}
	numChildren--;
	toRemove->owner = 0;
    }
    assert((numChildren == 0) == (child == 0));

    return toRemove != 0;
}

// sets the delayed-expanding flag
void KTreeViewItem::setDelayedExpanding(bool flag)
{
    delayedExpanding = flag;
}

// tells the item whether it shall delete child items
void KTreeViewItem::setDeleteChildren(bool flag)
{
    deleteChildren = flag;
}

// sets the draw expand button flag of this item
void KTreeViewItem::setDrawExpandButton(bool doit)
{
    doExpandButton = doit;
}

// sets the draw text flag of this item
void KTreeViewItem::setDrawText(bool doit)
{
    doText = doit;
}

// sets the draw tree branch flag of this item
void KTreeViewItem::setDrawTree(bool doit)
{
    doTree = doit;
}

// sets the expanded flag of this item
void KTreeViewItem::setExpanded(bool is)
{
    expanded = is;
}

// sets the item pixmap to the given pixmap
void KTreeViewItem::setPixmap(const QPixmap& pm)
{
    pixmap = pm;
}

// sets the item text to the given string
void KTreeViewItem::setText(const QString& t)
{
    text = t;
}

// counts the child items and stores the result in numChildren
void KTreeViewItem::synchNumChildren()
{
    numChildren = 0;
    KTreeViewItem* item = getChild();
    while (item != 0) {
	numChildren++;
	item = item->getSibling();
    }
}

/*
 * returns the bounding rect of the item text in cell coordinates couldn't
 * get QFontMetrics::boundingRect() to work right so I made my own
 */
QRect KTreeViewItem::textBoundingRect(int indent) const
{
    const QFontMetrics& fm = owner->fontMetrics();
    int cellHeight = height(fm);
    int rectX = indent + pixmap.width() + 3;
    int rectY = (cellHeight - fm.ascent() - fm.leading()) / 2 + 2;
    int rectW = fm.width(text) + 1;
    int rectH = fm.ascent() + fm.leading();
    return QRect(rectX, rectY, rectW, rectH);
}

// returns the total width of text and pixmap, including margins, spacing
// and indent, or -1 if empty -- SHOULD NEVER BE -1!
int KTreeViewItem::width(int indent) const
{
    return width(indent, owner->fontMetrics());
}

int KTreeViewItem::width(int indent, const QFontMetrics& fm) const
{
    int maxWidth = pixmap.width();
    maxWidth += (4 + fm.width(text));
    return maxWidth == 0  ?  -1  :  indent + maxWidth + 3;
}


/*
 * -------------------------------------------------------------------
 *
 * KTreeView
 *
 * -------------------------------------------------------------------
 */

// constructor
KTreeView::KTreeView(QWidget *parent,
		     const char *name,
		     WFlags f) :
	QTableView(parent, name, f),
	clearing(false),
	current(-1),
	drawExpandButton(true),
	drawTree(true),
	expansion(0),
	goingDown(false),
	itemIndent(22),
	showText(true),
	itemCapacity(500),
	visibleItems(0),
	rubberband_mode(false)
{
    initMetaObject();
    setCellHeight(0);
    setCellWidth(0);
    setNumRows(0);
    setNumCols(1);
    setTableFlags(Tbl_autoScrollBars | Tbl_clipCellPainting | Tbl_snapToVGrid);
    clearTableFlags(Tbl_scrollLastVCell | Tbl_scrollLastHCell | Tbl_snapToVGrid);
    switch(style()) {
    case WindowsStyle:
    case MotifStyle:
	setFrameStyle(QFrame::WinPanel | QFrame::Sunken);
	setBackgroundColor(colorGroup().base());
	break;
    default:
	setFrameStyle(QFrame::Panel | QFrame::Plain);
	setLineWidth(1);
    }
    setAcceptFocus(true);
    treeRoot = new KTreeViewItem;
    treeRoot->setExpanded(true);
    treeRoot->owner = this;

    visibleItems = new KTreeViewItem*[itemCapacity];
    // clear those pointers
    for (int j = itemCapacity-1; j >= 0; j--) {
	visibleItems[j] = 0;
    }
}

// destructor
KTreeView::~KTreeView()
{
    goingDown = true;
    clear();
    delete[] visibleItems;
    delete treeRoot;
}

// appends a child to the item at the given index with the given text
// and pixmap
void KTreeView::appendChildItem(const char* theText, const QPixmap& thePixmap,
				int index)
{
    KTreeViewItem* item = new KTreeViewItem(theText, thePixmap);
    item->setDeleteChildren(true);
    appendChildItem(item, index);
}

// appends a child to the item at the end of the given path with
// the given text and pixmap
void KTreeView::appendChildItem(const char* theText, const QPixmap& thePixmap,
				const KPath& thePath)
{
    KTreeViewItem* item = new KTreeViewItem(theText, thePixmap);
    item->setDeleteChildren(true);
    appendChildItem(item, thePath);
}

// appends the given item to the children of the item at the given index
void KTreeView::appendChildItem(KTreeViewItem* newItem, int index)
{                                                                  
    /* find parent item and append new item to parent's sub tree */
    KTreeViewItem* parentItem = itemAt(index);
    if (!parentItem)
	return;
    appendChildItem(parentItem, newItem);
}

// appends the given item to the children of the item at the end of the
// given path
void KTreeView::appendChildItem(KTreeViewItem* newItem, const KPath& thePath)
{                                
    /* find parent item and append new item to parent's sub tree */
    KTreeViewItem* parentItem = itemAt(thePath);
    if (!parentItem)
	return;
    appendChildItem(parentItem, newItem);
}
                                 
// indicates whether horizontal scrollbar appears only when needed
bool KTreeView::autoBottomScrollBar() const
{
  return testTableFlags(Tbl_autoHScrollBar);
}

// indicates whether vertical scrollbar appears only when needed
bool KTreeView::autoScrollBar() const
{
  return testTableFlags(Tbl_autoVScrollBar);
}

// indicates whether display updates automatically on changes
bool KTreeView::autoUpdate() const
{
  return QTableView::autoUpdate();
}

// indicates whether horizontal scrollbar is present
bool KTreeView::bottomScrollBar() const
{
  return testTableFlags(Tbl_hScrollBar);
}

// find item at specified index and change pixmap and/or text
void KTreeView::changeItem(const char *newText,
			      const QPixmap *newPixmap,
			      int index)
{
  KTreeViewItem *item = itemAt(index);
  if(item)
    changeItem(item, index, newText, newPixmap);
}

// find item at end of specified path, and change pixmap and/or text
void KTreeView::changeItem(const char* newText,
			   const QPixmap* newPixmap,
			   const KPath& thePath)
{
    KTreeViewItem* item = itemAt(thePath);
    if (item) {
	int index = itemRow(item);
	changeItem(item, index, newText, newPixmap);
    }
}

// clear all items from list and erase display
void KTreeView::clear()
{
    setCurrentItem(-1);	

    /* somewhat of a hack for takeItem so it doesn't update the current item... */
	clearing = TRUE;
	
	bool autoU = autoUpdate();
	setAutoUpdate(FALSE);
	QStack<KTreeViewItem> stack;
	stack.push(treeRoot);
	while(!stack.isEmpty()) {
		KTreeViewItem *item = stack.pop();
		if(item->hasChild()) {
			stack.push(item);
			stack.push(item->getChild());
		}
		else if(item->hasSibling()) {
			stack.push(item);
			stack.push(item->getSibling());
		}
		else if(item->getParent() != 0) {
			takeItem(item);
			delete item;
		}
	}
	clearing = FALSE;
  if(goingDown || QApplication::closingDown())
    return;
  if(autoU && isVisible())
    repaint();
  setAutoUpdate(autoU);
}

// return a count of all the items in the tree, whether visible or not
uint KTreeView::count()
{
  int total = 0;
  forEveryItem(&KTreeView::countItem, (void *)&total);
  return total;
}

// returns the index of the current (highlighted) item
int KTreeView::currentItem() const
{
  return current;
}

// collapses the item at the specified row index.
void KTreeView::collapseItem(int index, bool emitSignal)
{
    KTreeViewItem* item = itemAt(index);
    if (item)
	collapseSubTree(item, emitSignal);
}

// expands the item at the specified row indes.
void KTreeView::expandItem(int index, bool emitSignal)
{
    KTreeViewItem* item = itemAt(index);
    if (item)
	expandSubTree(item, emitSignal);
}

// returns the depth the tree is automatically expanded to when
// items are added
int KTreeView::expandLevel() const
{
  return expansion;
}

// visits every item in the tree, visible or not and applies 
// the user supplied function with the item and user data passed as parameters
// if user supplied function returns true, traversal ends and function returns
bool KTreeView::forEveryItem(KForEveryM func, void* user, KTreeViewItem* item)
{
    if (item == 0) {
	item = treeRoot;
    }
    assert(item->owner == this);
    item = item->getChild();

    while (item != 0) {
	// visit the siblings
	if ((this->*func)(item, user)) {
	    return true;
	}
	// visit the children (recursively)
	if (item->hasChild()) {
	    if (forEveryItem(func, user, item))
		return true;
	}
	item = item->getSibling();
    }
    return false;
}

// visits every visible item in the tree in order and applies the 
// user supplied function with the item and user data passed as parameters
// if user supplied function returns TRUE, traversal ends and function
// returns
bool KTreeView::forEveryVisibleItem(KForEveryM func, void *user,
				    KTreeViewItem* item)
{
    if (item == 0) {
	item = treeRoot;
    } else {
	// children are invisible (hence, nothing to do)
	// if item is invisible or collapsed
	if (!item->isVisible() || !item->isExpanded()) {
	    return false;
	}
    }
    assert(item->owner == this);
    item = item->getChild();

    while (item != 0) {
	// visit the siblings
	if ((this->*func)(item, user)) {
	    return true;
	}
	// visit the children (recursively)
	if (item->hasChild() && item->isExpanded()) {
	    if (forEveryVisibleItem(func, user, item))
		return true;
	}
	item = item->getSibling();
    }
    return false;
}

// returns a pointer to the KTreeViewItem at the current index
// or 0 if no current item
KTreeViewItem *KTreeView::getCurrentItem()
{
  if(current == -1) return 0;
  return itemAt(current);
}

// returns the current indent spacing
int KTreeView::indentSpacing()
{
    return itemIndent;
}

// inserts a new item with the specified text and pixmap before
// or after the item at the given index, depending on the value
// of prefix
// if index is negative, appends item to tree at root level
bool KTreeView::insertItem(const char* theText, const QPixmap& thePixmap,
			   int row, bool prefix)
{
    KTreeViewItem* refItem = itemAt(row);

    KTreeViewItem* item = new KTreeViewItem(theText, thePixmap);
    item->setDeleteChildren(true);

    bool success = insertItem(refItem, item, prefix);
    if (!success)
	delete item;
    return success;
}

// inserts a new item with the specified text and pixmap before
// or after the item at the end of the given path, depending on the value
// of prefix
bool KTreeView::insertItem(const char* theText, const QPixmap& thePixmap,
			   const KPath& path, bool prefix)
{
    KTreeViewItem* refItem = itemAt(path);

    KTreeViewItem* item = new KTreeViewItem(theText, thePixmap);
    item->setDeleteChildren(true);

    bool success = insertItem(refItem, item, prefix);
    if (!success)
	delete item;
    return success;
}

// inserts the given item or derived object into the tree before
// or after the item at the given index, depending on the value
// of prefix
// if index is negative, appends item to tree at root level
bool KTreeView::insertItem(KTreeViewItem* newItem,
			   int index, bool prefix)
{
    // find the item currently at the index, if there is one
    KTreeViewItem* refItem = itemAt(index);

    // insert new item at the appropriate place
    return insertItem(refItem, newItem, prefix);
}

// inserts the given item or derived object into the tree before
// or after the item at the end of the given path, depending on the value
// of prefix
bool KTreeView::insertItem(KTreeViewItem* newItem,
			   const KPath& thePath, bool prefix)
{                              
    // find the item currently at the end of the path, if there is one
    KTreeViewItem* refItem = itemAt(thePath);

    // insert new item at appropriate place
    return insertItem(refItem, newItem, prefix);
}

/*
 * returns pointer to KTreeViewItem at the specifed row or 0 if row is out
 * of limits.
 */
KTreeViewItem* KTreeView::itemAt(int row)
{
    if (row < 0 || row >= numRows()) {
	return 0;
    }
    else {
	// lookup the item in the list of visible items
	assert(row < itemCapacity);
	KTreeViewItem* i = visibleItems[row];
	assert(i != 0);
	return i;
    }
}

// returns pointer to KTreeViewItem at the end of the
// path or 0 if not found
KTreeViewItem* KTreeView::itemAt(const KPath& path)
{
    if (path.isEmpty())
	return 0;

    // need a copy of the path because recursiveFind will destroy it
    KPath pathCopy;
    pathCopy.setAutoDelete(false);
    pathCopy = path;

    return recursiveFind(pathCopy);
}

// computes the path of the item at the specified index
// if index is invalid, nothing is done.
void KTreeView::itemPath(int row, KPath& path)
{
    KTreeViewItem* item = itemAt(row);
    if (item != 0) {
	itemPath(item, path);
    }
}

// returns the row in the visible tree of the given item or
// -1 if not found
int KTreeView::itemRow(KTreeViewItem* item)
{
    if (item->owner == this) {
	// search in list of visible items
	for (int i = numRows()-1; i >= 0; i--) {
	    if (visibleItems[i] == item) {
		return i;
	    }
	}
    }
    // not found
    return -1;
}

/*
 * move the subtree at the specified index up one branch level (make root
 * item a sibling of its current parent)
 */
void KTreeView::join(int index)
{
  KTreeViewItem *item = itemAt(index);
  if(item)
    join(item);
}

/*
 * move the subtree at the specified index up one branch level (make root
 * item a sibling of it's current parent)
 */
void KTreeView::join(const KPath& path)
{
    KTreeViewItem *item = itemAt(path);
    if (item)
	join(item);
}

/* move item at specified index one slot down in its parent's sub tree */
void KTreeView::lowerItem(int index)
{
  KTreeViewItem *item = itemAt(index);
  if(item)
    lowerItem(item);
}

/* move item at specified path one slot down in its parent's sub tree */
void KTreeView::lowerItem(const KPath& path)
{
    KTreeViewItem* item = itemAt(path);
    if (item)
	lowerItem(item);
}

/* move item at specified index one slot up in its parent's sub tree */
void KTreeView::raiseItem(int index)
{
  KTreeViewItem* item = itemAt(index);
    if (item)
	raiseItem(item);
}

/* move item at specified path one slot up in its parent's sub tree */
void KTreeView::raiseItem(const KPath& path)
{
    KTreeViewItem* item = itemAt(path);
    if (item)
	raiseItem(item);
}

// remove the item at the specified index and delete it
void KTreeView::removeItem(int index)
{
  KTreeViewItem *item = itemAt(index);
  if(item) { 
    takeItem(item);
    delete item;
  }
}

// remove the item at the end of the specified path and delete it
void KTreeView::removeItem(const KPath& thePath)
{
    KTreeViewItem* item = itemAt(thePath);
    if (item) {
	takeItem(item);
	delete item;
    }
}

// indicates whether vertical scrollbar is present
bool KTreeView::scrollBar() const
{
  return testTableFlags(Tbl_vScrollBar);
}

void KTreeView::scrollVisible(KTreeViewItem* item, bool children)
{
    if (item == 0)
	return;
    int row = itemRow(item);
    if (row < 0)
	return;				/* do nothing if invisible */

    if (children && item->isExpanded()) {
	// we are concerned about children
	if (!rowIsVisible(row)) {
	    // just move to the top
	    setTopCell(row);
	} else {
	    // this is the complicated part
	    // count the visible children (including grandchildren)
	    int numVisible = 0;
	    forEveryVisibleItem(countItem, &numVisible, item);
	    // if the last child is visible, do nothing
	    if (rowIsVisible(row + numVisible))
		return;
	    /*
	     * Basically, item will become the top cell; but if there are
	     * more visible rows in the widget than we have children, then
	     * we don't move that far.
	     */
	    int remain = lastRowVisible()-topCell()-numVisible;
	    if (remain <= 0) {
		setTopCell(row);
	    } else {
		setTopCell(QMAX(0,row-remain));
	    }
	}
    } else {
	// we are not concerned about children
	if (rowIsVisible(row))
	    return;
	// just move the item to the top
	setTopCell(row);
    }
}

// enables/disables auto update of display
void KTreeView::setAutoUpdate(bool enable)
{
  QTableView::setAutoUpdate(enable);
}

// enables/disables horizontal scrollbar
void KTreeView::setBottomScrollBar(bool enable)
{
  enable ? setTableFlags(Tbl_hScrollBar) :
    clearTableFlags(Tbl_hScrollBar);
}

// sets the current item and hightlights it, emitting signals
void KTreeView::setCurrentItem(int row)
{
    if (row == current)
	return;
    int numVisible = numRows();
    if (row > numVisible)
	return;
    int oldCurrent = current;
    current = row;
    if (oldCurrent < numVisible)
	updateCell(oldCurrent, 0);
    if (current > -1) {
	updateCell(current, 0, false);
	emit highlighted(current);
    }
}

// enables/disables drawing of expand button
void KTreeView::setExpandButtonDrawing(bool enable)
{
  if(drawExpandButton == enable)
    return;
  drawExpandButton = enable;
  forEveryItem(&KTreeView::setItemExpandButtonDrawing, 0);
  if(autoUpdate() && isVisible())
    repaint();
}

// sets depth to which subtrees are automatically expanded, and
// redraws tree if auto update enabled
void KTreeView::setExpandLevel(int level)
{
    if (expansion == level)
	return;
    expansion = level;
    KTreeViewItem* item = getCurrentItem();
    forEveryItem(&KTreeView::setItemExpanded, 0);
    while (item != 0) {
	if (item->getParent()->isExpanded())
	    break;
	item = item->getParent();
    }
    if (item != 0)
	setCurrentItem(itemRow(item));
    if (autoUpdate() && isVisible())
	repaint();
}

// sets the indent margin for all branches and repaints if auto update enabled
void KTreeView::setIndentSpacing(int spacing)
{
    if (itemIndent == spacing)
	return;
    itemIndent = spacing;
    updateCellWidth();
    if (autoUpdate() && isVisible())
	repaint();
}

// enables/disables vertical scrollbar
void KTreeView::setScrollBar(bool enable)
{
  enable ? setTableFlags(Tbl_vScrollBar) :
    clearTableFlags(Tbl_vScrollBar);
}

// enables/disables display of item text (keys)
void KTreeView::setShowItemText(bool enable)
{
  if(showText == enable)
    return;
  showText = enable;
  forEveryItem(&KTreeView::setItemShowText, 0);
  if(autoUpdate() && isVisible())
    repaint();
}

// indicates whether vertical scrolling is by pixel or row
void KTreeView::setSmoothScrolling(bool enable)
{
  enable ? setTableFlags(Tbl_smoothVScrolling) :
    clearTableFlags(Tbl_smoothVScrolling);
}

// enables/disables tree branch drawing
void KTreeView::setTreeDrawing(bool enable)
{
  if(drawTree == enable)
    return;
  drawTree = enable;
  forEveryItem(&KTreeView::setItemTreeDrawing, 0);
  if(autoUpdate() && isVisible())
    repaint();
}
    
// indicates whether item text keys are displayed
bool KTreeView::showItemText() const
{
  return showText;
}

// indicates whether scrolling is by pixel or row
bool KTreeView::smoothScrolling() const
{
  return testTableFlags(Tbl_smoothVScrolling);
}

// indents the item at the given index, splitting the tree into
// a new branch
void KTreeView::split(int index)
{
  KTreeViewItem *item = itemAt(index);
  if(item)
    split(item);
}

// indents the item at the given path, splitting the tree into
// a new branch
void KTreeView::split(const KPath& path)
{
    KTreeViewItem* item = itemAt(path);
    if (item)
	split(item);
}

// removes item at specified index from tree but does not delete it
// returns pointer to the item or 0 if not succesful
KTreeViewItem *KTreeView::takeItem(int index)
{
  KTreeViewItem *item = itemAt(index);
  if(item)
    takeItem(item);
  return item;
}

// removes item at specified path from tree but does not delete it
// returns pointer to the item or 0 if not successful
KTreeViewItem* KTreeView::takeItem(const KPath& path)
{
    KTreeViewItem* item = itemAt(path);
    if (item)
	takeItem(item);
    return item;
}

// indicates whether tree branches are drawn
bool KTreeView::treeDrawing() const
{
  return drawTree;
}


// appends a child to the specified parent item (note: a child, not a sibling, is added!)
void KTreeView::appendChildItem(KTreeViewItem* theParent,
				KTreeViewItem* newItem)
{
    theParent->appendChild(newItem);

    // set item state
    newItem->setDrawExpandButton(drawExpandButton);
    newItem->setDrawTree(drawTree);
    newItem->setDrawText(showText);
    if (level(newItem) < expansion) {
	newItem->setExpanded(true);
    }

    // fix up branch levels of any children that the new item may already have
    if(newItem->hasChild()) {
	fixChildren(newItem);
    }

    // if necessary, adjust cell width, number of rows and repaint
    if (newItem->isVisible() || theParent->childCount() == 1) {
	bool autoU = autoUpdate();
	setAutoUpdate(false);
	updateVisibleItems();
	if(autoU && isVisible())
	    repaint();
	setAutoUpdate(autoU);
    }
}

// returns the height of the cell(row) at the specified row (index)
int KTreeView::cellHeight(int row)
{
  return itemAt(row)->height(fontMetrics());
}

// returns the width of the cells. Note: this is mostly for derived classes
// which have more than 1 column
int KTreeView::cellWidth(int /*col*/)
{
  return maxItemWidth;
}

// changes the given item with the new text and/or pixmap
void KTreeView::changeItem(KTreeViewItem* toChange, int itemRow,
			   const char* newText, const QPixmap* newPixmap)
{
    int indent = indentation(toChange);
    int oldWidth = toChange->width(indent);
    if(newText)
	toChange->setText(newText);
    if (newPixmap)
	toChange->setPixmap(*newPixmap);
    if(oldWidth != toChange->width(indent))
	updateCellWidth();
    if(itemRow == -1)
	return;
    if(autoUpdate() && rowIsVisible(itemRow))
	updateCell(itemRow, 0);
}

// collapses the subtree at the specified item
void KTreeView::collapseSubTree(KTreeViewItem* subRoot, bool emitSignal)
{
    assert(subRoot->owner == this);
    if (!subRoot->isExpanded())
	return;

    // must move the current item if it is visible
    KTreeViewItem* cur = current >= 0  ?  itemAt(current)  :  0;

    subRoot->setExpanded(false);
    if (subRoot->isVisible()) {
	bool autoU = autoUpdate();
	setAutoUpdate(false);
	updateVisibleItems();
	// re-seat current item
	if (cur != 0) {
	    setCurrentItem(itemRow(cur));
	}
	if (emitSignal) {
	    emit collapsed(itemRow(subRoot));
	}
	if (autoU && isVisible())
	    repaint();
	setAutoUpdate(autoU);
    }
}

// used by count() with forEach() function to count total number
// of items in the tree
bool KTreeView::countItem(KTreeViewItem*, void* total)
{
    int* t = static_cast<int*>(total);
    (*t)++;
    return false;
}

// expands the subtree at the given item
void KTreeView::expandSubTree(KTreeViewItem* subRoot, bool emitSignal)
{
    assert(subRoot->owner == this);
    if (subRoot->isExpanded())
	return;

    // must move the current item if it is visible
    KTreeViewItem* cur = current >= 0  ?  itemAt(current)  :  0;

    bool allow = true;

    if (subRoot->delayedExpanding) {
	emit expanding(subRoot, allow);
    }
    if (!allow)
	return;

    subRoot->setExpanded(true);

    if (subRoot->isVisible()) {
	bool autoU = autoUpdate();
	setAutoUpdate(false);
	updateVisibleItems();
	// re-seat current item
	if (cur != 0) {
	    setCurrentItem(itemRow(cur));
	}
	if (emitSignal) {
	    emit expanded(itemRow(subRoot));
	}
	if (autoU && isVisible())
	    repaint();
	setAutoUpdate(autoU);
    }
}

// fix up branch levels out of whack from split/join operations on the tree
void KTreeView::fixChildren(KTreeViewItem *parentItem)
{
    KTreeViewItem* childItem = 0;
    KTreeViewItem* siblingItem = 0;
//    int childBranch = parentItem->getBranch() + 1;
    if(parentItem->hasChild()) {
	childItem = parentItem->getChild(); 
//	childItem->setBranch(childBranch);
	childItem->owner = parentItem->owner;
	fixChildren(childItem);
    }
    while(childItem && childItem->hasSibling()) {
	siblingItem = childItem->getSibling();
//	siblingItem->setBranch(childBranch);
	siblingItem->owner = parentItem->owner;
	fixChildren(siblingItem);
	childItem = siblingItem;
    }	
}

// handles QFocusEvent processing by setting current item to top
// row if there is no current item, and updates cell to add or
// delete the focus rectangle on the highlight bar
void KTreeView::focusInEvent(QFocusEvent *)
{
  if(current < 0 && numRows() > 0)
    setCurrentItem(topCell());
  updateCell(current, 0);
}

// called by updateCellWidth() for each item in the visible list
bool KTreeView::getMaxItemWidth(KTreeViewItem *item, void *user)
{
    int indent = indentation(item);
    int* maxW = static_cast<int*>(user);
    int w = item->width(indent);
    if (w > *maxW)
	*maxW = w;
    return false;
}

int KTreeView::indentation(KTreeViewItem* item) const
{
    return level(item) * itemIndent + itemIndent + 3;
}

// inserts the new item before or after the reference item, depending
// on the value of prefix
bool KTreeView::insertItem(KTreeViewItem* referenceItem,
			   KTreeViewItem* newItem,
			   bool prefix)
{
    assert(newItem != 0);
    assert(referenceItem == 0 || referenceItem->owner == this);

    /* set the new item's state */
    newItem->setDrawExpandButton(drawExpandButton);
    newItem->setDrawTree(drawTree);
    newItem->setDrawText(showText);
    KTreeViewItem* parentItem;
    if (referenceItem) {
	parentItem = referenceItem->getParent(); 
	int insertIndex = parentItem->childIndex(referenceItem);
	if (!prefix)
	    insertIndex++;
	parentItem->insertChild(insertIndex, newItem);
    }
    else {
	// no reference item, append at end of visible tree
	// need to repaint
	parentItem = treeRoot;
	parentItem->appendChild(newItem);
    }

    // set item expansion
    if (level(newItem) < expansion)
	newItem->setExpanded(true);

    // fix up branch levels of any children
    if (newItem->hasChild())
	fixChildren(newItem);

    // if repaint necessary, do it if visible and auto update
    // enabled
    if (newItem->isVisible() || parentItem->childCount() == 1) {
	bool autoU = autoUpdate();
	setAutoUpdate(FALSE);
	updateVisibleItems();
	if(autoU && isVisible())
	    repaint();
	setAutoUpdate(autoU);
    }
    return true;
}

/*
 * returns pointer to item's path
 */
void KTreeView::itemPath(KTreeViewItem* item, KPath& path) const
{
    assert(item != 0);
    assert(item->owner == this);
    if (item != treeRoot) {
	itemPath(item->getParent(), path);
	path.push(new QString(item->getText()));
    }
}

/*
 * joins the item's branch into the tree, making the item a sibling of its
 * parent
 */
void KTreeView::join(KTreeViewItem *item)
{
  KTreeViewItem *itemParent = item->getParent();
  if(itemParent->hasParent()) {
    bool autoU = autoUpdate();
    setAutoUpdate(FALSE);
    takeItem(item);
    insertItem(itemParent, item, FALSE);
    if(autoU && isVisible())
      repaint();
    setAutoUpdate(autoU);
  }
}

// handles keyboard interface
void KTreeView::keyPressEvent(QKeyEvent* e)
{
    if (numRows() == 0)
	return;				/* nothing to do */

    /* if there's no current item, make the top item current */
    if (currentItem() < 0)
	setCurrentItem(topCell());
    assert(currentItem() >= 0);		/* we need a current item */
    assert(itemAt(currentItem()) != 0);	/* we really need a current item */

    int pageSize, delta;
    KTreeViewItem* item;
    int key = e->key();
repeat:
    switch (key) {
    case Key_Up:
	// make previous item current, scroll up if necessary
	if (currentItem() > 0) {
	    setCurrentItem(currentItem() - 1);
	    if (currentItem() < topCell())
		setTopCell(currentItem());
	}
	break;
    case Key_Down:
	// make next item current, scroll down if necessary
	if (currentItem() < numRows() - 1) {
	    setCurrentItem(currentItem() + 1);
	    if (currentItem() > lastRowVisible())
		setTopCell(topCell() + currentItem() - lastRowVisible());
	}
	break;
    case Key_Next:
	// move highlight one page down and scroll down
	delta = currentItem() - topCell();
	pageSize = lastRowVisible() - topCell();
	setTopCell(QMIN(topCell() +  pageSize, numRows() - 1));
	setCurrentItem(QMIN(topCell() + delta, numRows() - 1));
	break;
    case Key_Prior:
	// move highlight one page up and scroll up
	delta = currentItem() - topCell();
	pageSize = lastRowVisible() - topCell();
	setTopCell(QMAX(topCell() - pageSize, 0));
	setCurrentItem(QMAX(topCell() + delta, 0));
	break;
    case Key_Plus:
    case Key_Right:
	// if current item has subtree and is collapsed, expand it
	item = itemAt(currentItem());
	if (item->isExpanded() && key == Key_Right) {
	    // going right on an expanded item is like going down
	    key = Key_Down;
	    goto repeat;
	} else {
	    expandSubTree(item, true);
	    scrollVisible(item, true);
	}
	break;
    case Key_Minus:
    case Key_Left:
	// if current item has subtree and is expanded, collapse it
	item = itemAt(currentItem());
	if (!item->isExpanded() && key == Key_Left) {
	    // going left on a collapsed item is like going up
	    key = Key_Up;
	    goto repeat;
	} else {
	    collapseSubTree(item, true);
	    scrollVisible(item, true);
	}
	break;
    case Key_Return:
    case Key_Enter:
	// select the current item
	if (currentItem() >= 0)
	    emit selected(currentItem());
	break;
    default:
	break;
    }
}

int KTreeView::level(KTreeViewItem* item) const
{
    assert(item != 0);
    assert(item->owner == this);
    assert(item != treeRoot);
    int l = 0;
    item = item->parent->parent;	/* since item != treeRoot, there is a parent */
    while (item != 0) {
	item = item->parent;
	l++;
    }
    return l;
}

/* move specified item down one slot in parent's subtree */
void KTreeView::lowerItem(KTreeViewItem *item)
{
  KTreeViewItem *itemParent = item->getParent();
  uint itemChildIndex = itemParent->childIndex(item);
  if(itemChildIndex < itemParent->childCount() - 1) {
    bool autoU = autoUpdate();
    setAutoUpdate(FALSE);
    takeItem(item);
    insertItem(itemParent->childAt(itemChildIndex), item, FALSE);
    if(autoU && isVisible())
      repaint();
    setAutoUpdate(autoU);
  }
}

// handle mouse double click events by selecting the clicked item
// and emitting the signal
void KTreeView::mouseDoubleClickEvent(QMouseEvent *e)
{
  // find out which row has been clicked
	
  QPoint mouseCoord = e->pos();
  int itemClicked = findRow(mouseCoord.y());
  
  // if a valid row was not clicked, do nothing
  
  if(itemClicked == -1) 
    return;

  KTreeViewItem *item = itemAt(itemClicked);
  if(!item) return;
  
  // translate mouse coord to cell coord
  
  int  cellX, cellY;
  colXPos(0, &cellX);
  rowYPos(itemClicked, &cellY);
  QPoint cellCoord(mouseCoord.x() - cellX, mouseCoord.y() - cellY);
  
  // hit test item

    int indent = indentation(item);
  if(item->boundingRect(indent).contains(cellCoord))
    emit selected(itemClicked);
}

// handle mouse movement events
void KTreeView::mouseMoveEvent(QMouseEvent *e)
{
  // in rubberband_mode we actually scroll the window now
  if (rubberband_mode) 
	{
	  move_rubberband(e->pos());
	}
}


// handle single mouse presses
void KTreeView::mousePressEvent(QMouseEvent *e)
{
    // first: check which button was pressed

    if (e->button() == MidButton) 
    {
	// RB: the MMB is hardcoded to the "rubberband" scroll mode
	if (!rubberband_mode) {
	    start_rubberband(e->pos());
	}
	return;
    } 
    else if (rubberband_mode) 
    {
	// another button was pressed while rubberbanding, stop the move.
	// RB: if we allow other buttons while rubberbanding the tree can expand
	//     while rubberbanding - we then need to reclaculate and resize the
	//     rubberband rect and show the new size
	end_rubberband();
	return;  // should we process the button press?
    }

    // find out which row has been clicked
    QPoint mouseCoord = e->pos();
    int itemClicked = findRow(mouseCoord.y());

    // nothing to do if not on valid row  
    if (itemClicked == -1)
	return;

    KTreeViewItem* item = itemAt(itemClicked);
    if (!item)
	return;

    // translate mouse coord to cell coord
    int  cellX, cellY;
    colXPos(0, &cellX);
    rowYPos(itemClicked, &cellY);
    QPoint cellCoord(mouseCoord.x() - cellX, mouseCoord.y() - cellY);

    /* hit test expand button (doesn't set currentItem) */
    if (item->expandButtonClicked(cellCoord)) {
	if (item->isExpanded()) {
	    collapseSubTree(item, true);
	} else {
	    expandSubTree(item, true);
	    scrollVisible(item, true);	/* make children visible */
	}
    }
    // hit test item
    else if (item->boundingRect(indentation(item)).contains(cellCoord)) {
	setCurrentItem(itemClicked);
    }
}

// handle mouse release events
void KTreeView::mouseReleaseEvent(QMouseEvent *e)
{
  /* if it's the MMB end rubberbanding */
  if (rubberband_mode && e->button()==MidButton) 
	{
	  end_rubberband();
	}
}

// rubberband move: draw the rubberband
void KTreeView::draw_rubberband()
{
    /*
     * RB: I'm using a white pen because of the XorROP mode. I would prefer
     * to draw the rectangle in red but I don't now how to get a pen which
     * draws red in XorROP mode (this depends on the background). In fact
     * the color should be configurable.
     */

  if (!rubberband_mode) return;
  QPainter paint(this);
  paint.setPen(white);
  paint.setRasterOp(XorROP);
  paint.drawRect(xOffset()*viewWidth()/totalWidth(),
                 yOffset()*viewHeight()/totalHeight(),
                 rubber_width+1, rubber_height+1);
  paint.end();
}

// rubberband move: start move
void KTreeView::start_rubberband(const QPoint& where)
{
  if (rubberband_mode) { // Oops!
    end_rubberband();
  }
    /* RB: Don't now, if this check is necessary */
  if (!viewWidth() || !viewHeight()) return; 
  if (!totalWidth() || !totalHeight()) return;

  // calculate the size of the rubberband rectangle
  rubber_width = viewWidth()*viewWidth()/totalWidth();
  if (rubber_width > viewWidth()) rubber_width = viewWidth();
  rubber_height = viewHeight()*viewHeight()/totalHeight();
  if (rubber_height > viewHeight()) rubber_height = viewHeight();

  // remember the cursor position and the actual offset
  rubber_startMouse = where;
  rubber_startX = xOffset();
  rubber_startY = yOffset();
  rubberband_mode=TRUE;
  draw_rubberband();
}

// rubberband move: end move
void KTreeView::end_rubberband()
{
  if (!rubberband_mode) return;
  draw_rubberband();
  rubberband_mode = FALSE;
}

// rubberband move: hanlde mouse moves
void KTreeView::move_rubberband(const QPoint& where)
{
  if (!rubberband_mode) return;

  // look how much the mouse moved and calc the new scroll position
  QPoint delta = where - rubber_startMouse;
  int nx = rubber_startX + delta.x() * totalWidth() / viewWidth();
  int ny = rubber_startY + delta.y() * totalHeight() / viewHeight();

  // check the new position (and make it valid)
  if (nx < 0) nx = 0;
  else if (nx > maxXOffset()) nx = maxXOffset();
  if (ny < 0) ny = 0;
  else if (ny > maxYOffset()) ny = maxYOffset();

  // redraw the rubberband at the new position
  draw_rubberband();
  setOffset(nx,ny);
  draw_rubberband();
}


// paints the cell at the specified row and col
// col is ignored for now since there is only one
void KTreeView::paintCell(QPainter* p, int row, int)
{
    KTreeViewItem* item = itemAt(row);
    if (item == 0)
	return;

    QColorGroup cg = colorGroup();
    int indent = indentation(item);
    item->paint(p, indent, cg,
		current == row);		/* highlighted */
}


/* This is needed to make the kcontrol's color setup working (Marcin Dalecki) */
void KTreeView::paletteChange(const QPalette &)
{
    setBackgroundColor(colorGroup().base());
    repaint(true);
}


/* raise the specified item up one slot in parent's subtree */
void KTreeView::raiseItem(KTreeViewItem *item)
{
  KTreeViewItem *itemParent = item->getParent();
  int itemChildIndex = itemParent->childIndex(item);
  if(itemChildIndex > 0) {
    bool autoU = autoUpdate();
    setAutoUpdate(FALSE);
    takeItem(item);
    insertItem(itemParent->childAt(--itemChildIndex), item, TRUE);
    if(autoU && isVisible())
      repaint();
    setAutoUpdate(autoU);
  }
}

// find the item at the path
KTreeViewItem* KTreeView::recursiveFind(KPath& path)
{
    if (path.isEmpty())
	return treeRoot;

    // get the next key
    QString* searchString = path.pop();

    // find the parent item
    KTreeViewItem* parent = recursiveFind(path);
    if (parent == 0)
	return 0;

    /*
     * Iterate through the parent's children searching for searchString.
     */
    KTreeViewItem* sibling = parent->getChild();
    while (sibling != 0) {
	if (*searchString == sibling->getText()) {
	    break;			/* found it! */
	}
	sibling = sibling->getSibling();
    }
    return sibling;
}

// called by setExpandLevel for each item in tree
bool KTreeView::setItemExpanded(KTreeViewItem* item, void*)
{
    if (level(item) < expansion) {
	expandSubTree(item, true);
    } else {
	collapseSubTree(item, true);
    }
    return false;
}

// called by setExpandButtonDrawing for every item in tree
bool KTreeView::setItemExpandButtonDrawing(KTreeViewItem *item,
					      void *)
{
  item->setDrawExpandButton(drawExpandButton);
  return FALSE;
}

// called by setShowItemText for every item in tree
bool KTreeView::setItemShowText(KTreeViewItem *item, 
				   void *)
{
  item->setDrawText(showText);
  return FALSE;
}

// called by setTreeDrawing for every item in tree
bool KTreeView::setItemTreeDrawing(KTreeViewItem *item, void *)
{
  item->setDrawTree(drawTree);
  return FALSE;
}

// makes the item a child of the item above it, splitting
// the tree into a new branch
void KTreeView::split(KTreeViewItem *item)
{
  KTreeViewItem *itemParent = item->getParent();
  int itemChildIndex = itemParent->childIndex(item);
  if(itemChildIndex == 0)
    return;
  bool autoU = autoUpdate();
  setAutoUpdate(FALSE);
  takeItem(item);
  appendChildItem(itemParent->childAt(--itemChildIndex), item);
  if(autoU && isVisible())
    repaint();
  setAutoUpdate(autoU);
}

// removes the item from the tree without deleting it
void KTreeView::takeItem(KTreeViewItem* item)
{
    assert(item->owner == this);

    // TODO: go over this function again

    bool wasVisible = item->isVisible();
    /*
     * If we have a current item, make sure that it is not in the subtree
     * that we are about to remove. If the current item is in the part
     * below the taken-out subtree, we must move it up a number of rows if
     * the taken-out subtree is at least partially visible.
     */
    KTreeViewItem* cur = current >= 0  ?  itemAt(current)  :  0;
    if (wasVisible && cur != 0) {
	KTreeViewItem* c = cur;
	while (c != 0 && c != item) {
	    c = c->getParent();
	}
	if (c != 0) {
	    // move current item to parent
	    cur = item->getParent();
	    if (cur == treeRoot)
		cur = 0;
	}
    }
    KTreeViewItem* parentItem = item->getParent();
    parentItem->removeChild(item);
    item->sibling = 0;
    if (wasVisible || parentItem->childCount() == 0) {
	bool autoU = autoUpdate();
	setAutoUpdate(FALSE);
	updateVisibleItems();

	if (autoU && isVisible())
	    repaint();
	setAutoUpdate(autoU);
    }

    // re-seat the current item
    setCurrentItem(cur != 0  ?  itemRow(cur)  :  -1);
}

// visits each item, calculates the maximum width  
// and updates QTableView
void KTreeView::updateCellWidth()
{
    // make cells at least 1 pixel wide to avoid singularities (division by zero)
    int maxW = 1;
    forEveryVisibleItem(&KTreeView::getMaxItemWidth, &maxW);
    maxItemWidth = maxW;
    updateTableSize();
}

void KTreeView::updateVisibleItems()
{
    int index = 0;
    int count = 0;
    updateVisibleItemRec(treeRoot, index, count);
    assert(index == count);
    setNumRows(count);
    updateCellWidth();
}

void KTreeView::updateVisibleItemRec(KTreeViewItem* item, int& index, int& count)
{
    if (!item->isExpanded()) {
	// no visible items if not expanded
	return;
    }

    /*
     * Record the children of item in the list of visible items.
     *
     * Don't register item itself, it's already in the list. Also only
     * allocate new space for children.
     */
    count += item->childCount();
    if (count > itemCapacity) {
	// must reallocate
	int newCapacity = itemCapacity;
	do {
	    newCapacity += newCapacity;
	} while (newCapacity < count);
	KTreeViewItem** newItems = new KTreeViewItem*[newCapacity];
	// clear the unneeded space
	for (int i = index; i < newCapacity; i++) {
	    newItems[i] = 0;
	}
	// move already accumulated items over
	for (int i = index-1; i >= 0; i--) {
	    newItems[i] = visibleItems[i];
	}
	delete[] visibleItems;
	visibleItems = newItems;
	itemCapacity = newCapacity;
    }
    // insert children
    for (KTreeViewItem* i = item->getChild(); i != 0; i = i->getSibling()) {
	visibleItems[index++] = i;
	updateVisibleItemRec(i, index, count);
    }
}
