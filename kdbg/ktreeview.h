/*
 * $Revision$
 * $Date$
 * 
 * KTreeView class interface
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

#ifndef KDE_KTREE_VIEW_H
#define KDE_KTREE_VIEW_H

#include <qpixmap.h>			/* used in items */
#include <qstack.h>			/* used to specify tree paths */
#include <qstring.h>			/* used in items */
#include <qtablevw.h>			/* base class for widget */

// use stack of strings to represent path information
typedef QStack<QString> KPath;

class KTreeView;			/* forward declaration */

/** Items for the KTreeView widget */
class KTreeViewItem
{
    friend class KTreeView;
public:
    /**
     * Item constructor. While text defaults to a null string, and the
     * item can be constructed this way, the text has to be non-null when
     * the item is added to the tree, or it will not be inserted.
     * 
     * The constructor sets the delete-children flag to false. This flag
     * tells the item whether it shall delete the child items when it is
     * itself deleted. By default the creator of the item is responsible to
     * also delete the child items. (However, the versions of
     * KTreeView::appendChildItem and KTreeView::insertChildItem that do
     * not take a KTreeViewItem set the delete-children flag to true.)
     */
    KTreeViewItem(const QString& theText = QString()); // text can not be null when added to the list!
    KTreeViewItem(const QString& theText, const QPixmap& thePixmap);

    /**
     * Destructor. It destroys its children if this item has been marked
     * with setDeleteChildren(true).
     */
    virtual ~KTreeViewItem();

    /**
     * Appends a new (direct) child item at the end. It does not update
     * administrative data in newChild except for its parent (which is this
     * item) and owner.
     */
    void appendChild(KTreeViewItem* newChild);

    /**
     * Returns a pointer to the child item at the given index in this
     * item's sub tree, or 0 if not found.
     */	
    KTreeViewItem* childAt(int index) const;

    /**
     * Returns the number of child items in this item's sub tree.
     */
    uint childCount() const;

    /**
     * Returns the index in this items sub tree of the given item or -1 if
     * not found. The specified child must not be 0.
     */
    int childIndex(KTreeViewItem* child) const;

    /**
     * Determines whether the specified point is inside the expand button.
     */
    bool expandButtonClicked(const QPoint& coord) const;

    /**
     * Returns a pointer to the first child item in this item's sub tree, or
     * 0 if none.
     */
    KTreeViewItem* getChild() const;

    /**
     * Returns a pointer to the parent of this item, or 0 if none.
     */
    KTreeViewItem* getParent() const;

    /**
     * Returns a reference to this item's pixmap. If there is no pixmap
     * associated with this item, it will return a reference to a valid,
     * null QPixmap.
     */
    const QPixmap& getPixmap() const;

    /**
     * Returns a pointer to the next item in the same branch below this
     * one, or 0 if none.
     */
    KTreeViewItem* getSibling() const;

    /**
     * Returns this item's text.
     */
    const QString& getText() const;

    /**
     * Indicates whether this item has any children.
     */
    bool hasChild() const;

    /**
     * Indicates whether this item has a parent.
     */
    bool hasParent() const;

    /**
     * Indicates whether this item has a sibling item, that is, an item
     * that would be displayed below it at the same level as this item.
     */
    bool hasSibling() const;

    /**
     * Inserts the a new (direct) child in this item before the child at
     * the specified index (first child is index 0). If there is no child
     * at the specified index, the item is appended. It does not update
     * administrative data in newChild except for its parent (which is this
     * item) and owner.
     */
    void insertChild(int index, KTreeViewItem* newChild);

    /**
     * Indicates whether the item is expanded, that is, whether the child
     * items (if any) would be visible if this item were visible.
     * 
     * Note: If this function returns true, it does not necessarily indicate
     * that this item is visible or that this item has any children.
     */
    bool isExpanded() const;

    /**
     * Returns true if the item is visible. An item is visible if all its
     * ancestors are expanded.
     * 
     * Note: If this function returns true, it does not necessarily indicate
     * that the widget is visible.
     */
    bool isVisible() const;

    /**
     * Removes the specified (direct) child from this item and returns
     * true. If it is not a direct child of this item, nothing happens, and
     * false is returned. This function does not update the owning
     * KTreeView.
     */
    bool removeChild(KTreeViewItem* child);

    /**
     * Sets the delayed-expanding flag. If this flag is true, the expanding
     * signal is emitted when the item is about to be expanded. The expand
     * button is always painted for this item, even if it doesn't have
     * children.
     */
    void setDelayedExpanding(bool flag);

    /**
     * Tells the item whether it should delete its children when it is
     * deleted. The default is false, which means that the child items must
     * be deleted explicitly.
     */
    void setDeleteChildren(bool flag);

    void setDrawExpandButton(bool doit);

    void setDrawText(bool doit);

    void setDrawTree(bool doit);

    void setExpanded(bool is);

    /**
     * Sets the item pixmap to the given pixmap. It does not redraw the
     * item or update the owning KTreeView.
     */
    void setPixmap(const QPixmap& pm);

    /**
     * Sets the item text. This function does not redraw the item or update
     * the owning KTreeView.
     */
     void setText(const QString& t);

protected:
    /**
     * Returns the bounding rectangle of the item.
     */
    virtual QRect boundingRect(int indent) const;

    /**
     * Returns the hieght of the item. The default implementation uses font
     * metrics of the owning KTreeView widget.
     */
    virtual int height() const;

    /*
     * Returns the height of the item depending on the passed-in font
     * metrics.
     */
    virtual int height(const QFontMetrics& fm) const;

    /**
     * Paints the item: pixmap, text, expand button, parent branches
     */
    virtual void paint(QPainter* p, int indent,
			const QColorGroup& cg, bool highlighted) const;

    /**
     * paints the expand button
     */
    virtual void paintExpandButton(QPainter* p, int indent, int cellHeight,
				   const QColorGroup& cg) const;

    /**
     * paints the highlighted text
     */
    virtual void paintHighlight(QPainter* p, int indent,
			const QColorGroup& cg, bool hasFocus,
			GUIStyle style) const;

    /**
     * paints the item's text
     */
    virtual void paintText(QPainter* p, int indent, int cellHeight,
			   const QColorGroup& cg, bool highlighted) const;

    /**
     * paints the item's tree part.
     */
    virtual void paintTree(QPainter* p, int indent, int cellHeight) const;

    /**
     * Internal function that counts the number of child items.
     */
    void synchNumChildren();
    
    /**
     * Returns the bounding rectangle of the text.
     */
    virtual QRect textBoundingRect(int indent) const;

    /**
     * Returns the width of the item taking into account the specified
     * indentation. The default implementation uses font metrics of the
     * owning KTreeView widget.
     */
    virtual int width(int indent) const;

    /**
     * Returns the width of the item depending on the passed-in font
     * metrics and taking into account the specified indentation.
     */
    virtual int width(int indent, const QFontMetrics& fm) const;

protected:
    /** The KTreeView that this item belongs to */
    KTreeView* owner;
    int numChildren;
    bool doExpandButton;
    bool expanded;
    bool delayedExpanding;
    bool doTree;
    bool doText;
    mutable QRect expandButton;		/* is set in paint() */
    KTreeViewItem* child;
    KTreeViewItem* parent;
    KTreeViewItem* sibling;
    QPixmap pixmap;
    QString text;
    bool deleteChildren;
};

// easier declarations of function prototypes for forEvery type functions
typedef bool (KTreeView::*KForEveryM)
  (KTreeViewItem *, void *);
typedef bool (*KForEvery)
  (KTreeViewItem *, void *);

/** 
  A collapsible treelist widget.

  1. Introduction
  2. Features
  3. Installation
  4. Public interface

  1. Introduction
  ================================================================================

  KTreeView is a class inherited from QTableView in the Qt user interface
  library. It provides a way to display hierarchical data in a single-inheritance
  tree, similar to tree controls in Microsoft Windows and other GUI's. It is most
  suitable for directory trees or outlines, but I'm sure other uses will come to
  mind. Frankly, it was designed mostly with the above two functions in mind, but
  I have tried to make it as flexible as I know how to make it easy to adapt to
  other uses. 

  In case of problems, I encourage you to read all of the other documentation
  files in this package before contacting me  as you may find the answer to your
  question in one of them. Also read the source code if you have time. I have
  tried to comment it adequately and make the source understandable.

  2. Features
  ================================================================================

  * Displays both text and optional pixmap supplied by the programmer. A support
  class, KTreeViewItem, can be inherited and modified to draw items as needed
  by the programmer.

  * The list items can be returned by index or logical path and the tree
  navigated by parent, child or sibling references contained in them. Also,
  item information such as text, pixmap, branch level can be obtained.
  
  * Items can be inserted, changed and removed either by index in the visible
  structure, or by logical paths through the tree hierarchy. 

  * The logical path through the tree for any item can be obtained with the index
  of the item.

  * Tree structure display and expanding/collapsing of sub-trees is handled with
  no intervention from the programmer.
  
  * entire tree can be expanded or collapsed to a specified sub-level (handy for
  outline views)
  
  * Configuration as follows:

  enable/disable item text display (if you only want to display pixmaps)
  
  enable/disable drawing of expand/collapse button
  
  enable/disable drawing of tree structure
  
  * Keyboard support as follows:

  up/down arrows move the highlight appropriately and scroll the list an item at
  a time, if necessary
  
  pgup/pgdn move the highlight a 'page' up or down as applicable and scroll the
  view
  
  +/- keys expand/collapse the highlighted item if it appropriate
  
  enter key selects the highlighted item
  
  * Mouse support as follows:

  left click on item highlights it
  
  left click on an item "hot button" expands or collapses its sub-tree, as
  applicable
  
  double click on item selects it
  
  normal scrolling functions in conjunction with scrollbars if present

  2nd scrolling with the middle mouse button: pressing MMB inserts a
  rubberband, showing which part of the whole tree is currently visible.
  moving the mouse will scroll the visible part
  
  * Signals/Slots

  signal void highlighted(int) - emitted when an item in the tree is
  highlighted; sends the index of the item
  
  signal void selected(int) - emitted when an item in the tree is
  selected; sends the index of the item
  
  signal void expanded(int) - emitted when an item in the tree is expanded;
  sends the index of the item
  
  signal void collpased(int) - emitted when an item in the tree is collapsed;
  sends the index of the item
  */
class KTreeView : public QTableView
{
    friend class KTreeViewItem;
    Q_OBJECT
public:
    /**
     * Widget contructor. Passes all parameters on to base QTableView, and
     * does not use them directly. Does internal initialization, sets the
     * current item to -1, and sets default values for scroll bars (both
     * auto).
     */
    KTreeView(QWidget* parent = 0, const char* name = 0, WFlags f = 0);

    /*
     * Desctructor. Deletes all items from the topmost level that have been
     * marked with setDeleteChildren(true).
     */
    virtual ~KTreeView();

    /**
     * Appends a new child item to the item at the specified row. If that
     * item already has children, the new item is appended below these
     * children. A KTreeViewItem is created for which the delete-children
     * flag is set to true.
     */
    void appendChildItem(const char* theText, const QPixmap& thePixmap,
		      int index);

    /**
     * Same as above except that the parent item is specified by a path.
     */
    void appendChildItem(const char* theText, const QPixmap& thePixmap,
		      const KPath& thePath); 

    /**
     * Appendss the specified item as a child of the item that is at the
     * specified row. If that item already has children, the new item is
     * appended below these children.
     */
    void appendChildItem(KTreeViewItem* newItem, int index);

    /**
     * Same as above except that the parent item is specified by a path.
     */
    void appendChildItem(KTreeViewItem* newItem, const KPath& thePath);                                                             

    /**
	Returns a bool value indicating whether the list will display a
	horizontal scrollbar if one of the displayed items is wider than can
	be displayed at the current width of the view.
	*/
  bool autoBottomScrollBar() const;

  /**
	Returns a bool value indicating whether the list will display a
	vertical scrollbar if the number of displayed items is more than can
	be displayed at the current height of the view.
	*/
  bool autoScrollBar() const;

  /**
	Returns a bool value indicating whether the list will update
	immediately on changing the state of the widget in some way.
	*/
  bool autoUpdate() const;

  /**
	Returns a bool value indicating whether the list has currently has a
	horizontal scroll bar.
	*/
  bool bottomScrollBar() const;

  /**
	Changes the text and/or pixmap of the given item at the specified
	index to the given values and updates the display if auto update
	enabled. If changing only the text or pixmap, set the other parameter
	to 0.
	*/
  void changeItem(const char *newText, 
				  const QPixmap *newPixmap, 
				  int index);

  /**
	Same as above function, except item to change is specified by a path
	through the tree.
	*/
  void changeItem(const char *newText,
				  const QPixmap *newPixmap,
				  const KPath& thePath);

  /**
	Removes all items from the tree.

	*/
  void clear();

  /**
	Returns the total number of items in the tree, whether visible
	(expanded sub-trees) or not (collapsed).
	*/
  uint count();

  /**
	Returns the index of the current (highlighted) item. If no current
	item, returns -1.
	*/
  int currentItem() const;

  /**
	Collapses the sub-tree at the specified index. 
	*/
  void collapseItem(int index);

  /**
	Expands the sub-tree at the specified index. 
	*/
  void expandItem(int index);

  /**
	Returns the depth to which all parent items are automatically
	expanded.
	*/
  int expandLevel() const;

  /**
	Same as above functions combined into one. If sub-tree is expanded,
	collapses it, if it is collapsed, it expands it.
	*/
  void expandOrCollapseItem(int index);

    /**
     * Iterates every item in the tree, visible or not, and applies the
     * function func with a pointer to each item and user data supplied as
     * parameters. The children of the specified root item are visited
     * (root itself is not visited!). If root is 0 all items in the tree
     * are visited. KForEveryFunc is defined as:
     * 
     * typedef bool (*KForEvery)(KTreeViewItem*, void*); 
     * 
     * That is, a function that returns bool and takes a pointer to a
     * KTreeViewItem and pointer to void as parameters. The traversal ends
     * earlier if the supplied function returns bool. In this case the
     * return value is also true.
     */
    bool forEveryItem(KForEvery func, void* user,
		      KTreeViewItem* root = 0);

    /**
     * Same as above, but only iterates visible items, in order. If the
     * specified root item is invisible no items are visited.
     */
    bool forEveryVisibleItem(KForEvery func, void *user,
			     KTreeViewItem* root = 0);

  /**
	Returns a pointer to the current item if there is one, or 0.
	*/
  KTreeViewItem *getCurrentItem();

    /**
     * Returns the number of pixels an item is indented for each level. If,
     * in a derived class, the levels are indented differently this value
     * may be ignored.
     */
    int indentSpacing();

    /**
     * Inserts an item into the tree with the given text and pixmap either
     * before or after the item currently at the given row, depending on
     * the value of prefix. The new item is added to the same branch as the
     * referenced item. If row is -1, the item is simply appended to the
     * tree at the topmost level. A KTreeViewItem is created for which the
     * delete-children flag is set to true. Returns true if the item has
     * been successfully inserted in the tree, otherwise false.
     */
    bool insertItem(const char* theText, const QPixmap& thePixmap,
		    int row = -1, bool prefix = true);

    /**
     * Same as above, but uses a path through the tree to reference the
     * insert position. If there is no item at the specified path, the item
     * is simply appended to the tree at the topmost level.
     */
    bool insertItem(const char* theText, const QPixmap& thePixmap,
		    const KPath& thePath, bool prefix = true);

    /**
     * Same as above, but an item is specified instead of a text and a pixmap.
     */
    bool insertItem(KTreeViewItem *newItem, 
		    int row = -1, bool prefix = true); 

    /**
     * Same as above, but uses a path through the tree to reference the
     * insert position.
     */
    bool insertItem(KTreeViewItem *newItem,
		    const KPath& thePath, bool prefix = true);

    /**
     * Returns a pointer to the item in the specified row, or 0 if the
     * specified row is outside the limits. This is a cheap operation.
     */
    KTreeViewItem* itemAt(int row);

    /**
     * Returns a pointer to the item at the end of the path.
     */
    KTreeViewItem* itemAt(const KPath& path);

    /**
     * Returns the row at which the specified item is found in the visible
     * tree or -1 if the item is not visible or not in the tree.
     */
    int itemRow(KTreeViewItem* item);

    /**
     * Fills path with the logical path to the item at the specified row.
     * The specified path variable should be empty. Any strings popped from
     * the path must be deleted by the caller. If the row is invalid, path
     * remains unchanged (i.e. empty).
     */
    void itemPath(int row, KPath& path);

    /**
     * Outdents the item at the given row one level so that it becomes a
     * sibling of its parent.
     */
    void join(int index);

    /**
     * Same as above but uses a path to specify the item.
     */
    void join(const KPath& path);

    /**
     * Moves the item at the specified row down one row in its current
     * branch.
     */
    void lowerItem(int index);

    /**
     * Same as above but uses a path to specify the item.                              
     */
    void lowerItem(const KPath& path);

    /**
     * Moves the item at the specified row up one row in its current
     * branch.
     */
    void raiseItem(int row);

    /**
     * Same as above but uses a path to specify the item.
     */
    void raiseItem(const KPath& path);

    /**
     * Removes the item at the specified row.
     */
    void removeItem(int row);

    /**
     * Same as above except uses path through the tree to find the item.
     */
    void removeItem(const KPath& thePath);

  /**
	Returns bool value indicating whether the list currently displays a
	vertical scroll bar.
	*/
  bool scrollBar() const;

  /**
	If enable is TRUE (default), enables auto update, else disables it.
	*/
  void setAutoUpdate(bool enable);

  /**
	If enable is TRUE, displays a horizontal scroll bar, else hides it.
	*/
  void setBottomScrollBar(bool enable);

    /**
     * Makes the item at row current and highlights it. The signal
     * highlighted is emitted if the current item changes.
     */
    void setCurrentItem(int row);

  void setExpandButtonDrawing(bool enable);

  void setExpandLevel(int level);

    /**
     * Sets the indentation stepping, in pixels.  If, in a derived class,
     * the levels are indented differently this value may be ignored.
     */
    void setIndentSpacing(int spacing);

  /**
	If enable is TRUE, displays a vertical scroll bar, else hides it.                                        
	*/
  void setScrollBar(bool enable);

  /**
	If enable is TRUE (default), item text will be displayed, otherwise 
	it will not, and no highlight will be shown in the default widget.
	*/
  void setShowItemText(bool enable);

  /**
	If enable is TRUE, enables smooth scrolling, else disables 
	it (default).
	*/
  void setSmoothScrolling(bool enable);

  /**
	If enable is TRUE (default), lines depicting the structure of the
	tree will be drawn, otherwise they will not.
	*/
  void setTreeDrawing(bool enable);

  /**
	Indicates whether item text is displayed.
	*/
  bool showItemText() const;

  /**
	Returns a bool value indicating whether smooth scrolling is enabled.
	*/
  bool smoothScrolling() const;

    /**
     * Indents the item at the specified index, creating a new branch.
     */
    void split(int index);

    /**
     * Same as above but uses a path to specify the item.                    
     */
    void split(const KPath& path);

    /**
     * Removes the item at the given index from the tree, but does not
     * delete it, returning a pointer to the removed item.
     */
    KTreeViewItem* takeItem(int index);

    /**
     * Same as above but uses a path to specify the item to take.
     */
    KTreeViewItem* takeItem(const KPath& path);

  /**
	Indicates whether the tree structure is drawn.
	*/
  bool treeDrawing() const;

public:
    /**
     * This function is deprecated. Use numRows() instead.
     * Returns the number of items that are visible (their parents are
     * expanded).  
     */
    int visibleCount() const { return numRows(); }

signals:
    void collapsed(int index);
    void expanded(int index);
    /**
     * The expanding signal is emitted when an item that has the
     * delayedExpanding flag set is about to be expanded. The
     * delayedExpanding flag is not reset; the slot that the signal is
     * connected to should do so. The item being expanded is passed to the
     * slot. The slot gets the opportunity to insert child items into that
     * item. It should not change the item any other way. It can allow or
     * disallow the expansion by setting the second parameter allow. If it
     * is set to false, the item is not expanded.
     *
     * The signal is always emitted, regardless whether the expansion was
     * triggered by the user or by the program.
     */
    void expanding(KTreeViewItem* item, bool& allow);
    void highlighted(int index);
    void selected(int index);
protected:
    /**
     * Appends theChild to theParent as a new direct child. All internal
     * state is updated and the widget is repainted as necessary. theChild
     * remains invisible if any ancestor of theParent is collapsed.
     */
    void appendChildItem(KTreeViewItem* theParent,
			 KTreeViewItem* theChild);
    virtual int cellHeight(int row);
    virtual int cellWidth(int col);
    void changeItem(KTreeViewItem* toChange,
		    int itemRow, const char* newText,
		    const QPixmap* newPixmap);
    /**
     * Collapses the specified subtree and updates the display. subRoot
     * need not be visible.
     */
    void collapseSubTree(KTreeViewItem* subRoot);
    /** Internal function used for counting items */
    bool countItem(KTreeViewItem* item, void* total);

    void expandOrCollapse(KTreeViewItem *parentItem);
    /**
     * Expands the specified subtree and updates the display. subRoot need
     * not be visible.
     */
    void expandSubTree(KTreeViewItem* subRoot);
  void fixChildren(KTreeViewItem *parentItem);
  virtual void focusInEvent(QFocusEvent *e);
  void forEveryItem(KForEveryM func, 
					void *user);
  void forEveryVisibleItem(KForEveryM func,
						   void *user);

    /** internal function used to determine maximum item width */
    bool getMaxItemWidth(KTreeViewItem* item, void *user);

    /**
     * Returns the indentation of the specified item in pixels.
     */
    virtual int indentation(KTreeViewItem* item) const;

    /**
     * Inserts the specified newItem before or after the specified
     * referenceItem. If referenceItem is 0, the newItem is appended at the
     * topmost level. If referenceItem is not 0, it must be an item that is
     * already in the KTreeView. Internal data is updated and the display
     * is refreshed as necessary. The inserted item may still be invisible
     * if any of the parents is collapsed. newItem must not be 0.
     */
    bool insertItem(KTreeViewItem* referenceItem, KTreeViewItem* newItem,
		    bool prefix);

    /**
     * Finds the logical path of the specified item. The specified path
     * variable should be empty.
     */
    void itemPath(KTreeViewItem* item, KPath& path) const;

  void join(KTreeViewItem *item);
  virtual void keyPressEvent(QKeyEvent *e);
    int level(KTreeViewItem* item) const;
  void lowerItem(KTreeViewItem *item);
  virtual void mouseDoubleClickEvent(QMouseEvent *e);
  virtual void mouseMoveEvent(QMouseEvent *e);
  virtual void mousePressEvent(QMouseEvent *e);
  virtual void mouseReleaseEvent(QMouseEvent *e);
  virtual void paintCell(QPainter *p, int row, int col);
    /*
     * virtual void paintItem(QPainter *p, KTreeViewItem *item, 
			      * bool highlighted);
     */
    /**
     * Internal. Needed to make kcontrol's color setup work.
     */
    void paletteChange(const QPalette&);
    void raiseItem(KTreeViewItem* item);

    /**
     * Internal function that finds the item at the given path. Returns 0
     * if the item cannot be found. The path is destroyed by this function.
     */
    KTreeViewItem* recursiveFind(KPath& path);

  bool setItemExpanded(KTreeViewItem *item, void *);
  bool setItemExpandButtonDrawing(KTreeViewItem *item, void *);
  bool setItemShowText(KTreeViewItem *item, void *);
  bool setItemTreeDrawing(KTreeViewItem *item, void *);
  void split(KTreeViewItem *item);
  void takeItem(KTreeViewItem *item);
    virtual void updateCellWidth();
    virtual void updateVisibleItems();
    void updateVisibleItemRec(KTreeViewItem* parent, int& count, int& width);

    KTreeViewItem* treeRoot;
    bool clearing;
    int current;
    bool drawExpandButton;
    bool drawTree;
    int expansion;
    bool goingDown;
    int itemIndent;
    int maxItemWidth;
    bool showText;
    // list of visible items
    int itemCapacity;			/* for how many items we've space allocated */
    KTreeViewItem** visibleItems;

  // Rainer Bawidamann: move window in "rubberband" mode
  bool rubberband_mode;             // true if in "rubberband_mode"
  QPoint rubber_startMouse;         // where the user pressed the MMB
  int rubber_height, rubber_width,  // the size if the rubberband rect
	rubber_startX, rubber_startY; // the x/yOffset() when the MMB was pressed
  void draw_rubberband();
  void start_rubberband(const QPoint& where);
  void end_rubberband();
  void move_rubberband(const QPoint& where);
};

#endif // KDE_KTREE_VIEW_H
