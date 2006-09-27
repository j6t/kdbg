/*
 * $Id$
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
#include <qvaluestack.h>		/* used to specify tree paths */
#include <qstring.h>			/* used in items */
#include "tableview.h"			/* base class for widget */

// use stack of strings to represent path information
typedef QValueStack<QString> KPath;

class KTreeView;			/* forward declaration */

/** Items for the KTreeView widget */
class KTreeViewItem : public Qt
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
     * also delete the child items. (However, the versions of @ref
     * #appendChildItem and @ref #insertChildItem that do not take a
     * KTreeViewItem set the delete-children flag to true.)
     *
     * @param text specifies the new item's text
     */
    KTreeViewItem(const QString& text = QString()); // text can not be null when added to the list!
    /**
     * This overloaded constructor allows to specify a pixmap for the new
     * item.
     *
     * @param text specifies the item's text
     * @param pixmap specifies the item's pixmap
     */
    KTreeViewItem(const QString& text, const QPixmap& pixmap);

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
    virtual void appendChild(KTreeViewItem* newChild);

    /**
     * Returns a pointer to the child item at the given index in this
     * item's sub tree, or 0 if not found.
     *
     * @param index specifies the index of the direct child to return
     * @return the direct child at the specified index
     */	
    KTreeViewItem* childAt(int index) const;

    /**
     * Returns the number of child items in this item's sub tree.
     */
    uint childCount() const;

    /**
     * Returns the index in this items sub tree of the given item or -1 if
     * the specified item is not a direct child of this item.
     * 
     * @param child specifies the child to look up; must not be 0
     * @returns the index of the specified direct child
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
     * Returns a pointer to the next sibling item in the same branch below this
     * one, or 0 if this item has no siblings below it.
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
     * 
     * @param index specifies the index of a direct child of this item
     * @param newChild specifies the new item to insert
     */
    virtual void insertChild(int index, KTreeViewItem* newChild);

    /**
     * Indicates whether the item is expanded, that is, whether the child
     * items (if any) would be visible if this item were visible.
     * 
     * Note: If this function returns true, it does not necessarily indicate
     * that this item is visible or that this item has any children.
     */
    bool isExpanded() const;

    /**
     * Returns true if the item is visible. An item is visible if @ref
     * #isExpanded() returns true for all its ancestors.
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
    virtual bool removeChild(KTreeViewItem* child);

    /**
     * Sets the delayed-expanding flag. If this flag is true, the signal
     * @ref #expanding is emitted when the item is about to be expanded.
     * The expand button is always painted for this item, even if it
     * doesn't have children.
     */
    void setDelayedExpanding(bool flag);

    /**
     * Tells the item whether it should delete its children when it is
     * deleted. The default is false, which means that the child items must
     * be deleted explicitly.
     */
    void setDeleteChildren(bool flag);

    /**
     * Tells the item whether it should draw the expand button. The default
     * is true. This function does not update the owning tree widget.
     */
    virtual void setDrawExpandButton(bool doit);

    /**
     * Tells the item whether it should paint the text. The default is
     * true. This function does not update the owning tree widget.
     */
    virtual void setDrawText(bool doit);

    /**
     * Tells the item whether it should draw the tree lines. The default is
     * true. This function does not update the owning tree widget.
     */
    virtual void setDrawTree(bool doit);

    /**
     * Tells whether the item is expanded (i.e. whether its children are
     * visible). The default is false. This function does not update the
     * owning tree widget.
     */
    virtual void setExpanded(bool is);

    /**
     * Sets the item pixmap to the given pixmap. It does not redraw the
     * item or update the owning KTreeView.
     */
    virtual void setPixmap(const QPixmap& pm);

    /**
     * Sets the item text. This function does not redraw the item or update
     * the owning KTreeView.
     */
    virtual void setText(const QString& t);

protected:
    /**
     * Returns the bounding rectangle of the item.
     */
    virtual QRect boundingRect(int indent) const;

    /**
     * Returns the height of the item. The default implementation uses font
     * metrics of the owning KTreeView widget.
     */
    virtual int height() const;

    /*
     * Returns the height of the item depending on the passed-in font
     * metrics.
     */
    virtual int height(const QFontMetrics& fm) const;

    /*
     * The item is given a chance to process key events before the owning
     * KTreeView processes the event.
     * 
     * @param ev specifies the key event; use ev->type() to find out whether
     * this is a key down or up event (see @ref #QEvent).
     * @return true if the event has been processed, false if it has not
     * been processed. The default implementation just returns false to
     * indicate that the event has not been processed.
     */
    virtual bool keyEvent(QKeyEvent* ev);

    /*
     * The item is given a chance to process mouse events before the owning
     * KTreeView processes the event.
     *
     * @param ev specifies the mouse event; use ev->type() to find out whether
     * this is a mouse press, release, double click, or move event (see
     * @ref #QEvent).
     * @param itemCoord specifies the mouse even coordinates relative to this
     * item (the coordinates in ev are the original coordinates).
     * @return true if the event has been processed, false if it has not
     * been processed. The default implementation just returns false to
     * indicate that the event has not been processed.
     */
    virtual bool mouseEvent(QMouseEvent* ev, const QPoint& itemCoord);

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
			const QColorGroup& cg, bool hasFocus) const;

    /**
     * paints the item's text
     */
    virtual void paintText(QPainter* p, int indent, int cellHeight,
			   const QColorGroup& cg, bool highlighted) const;

    /**
     * paints the item's tree part.
     */
    virtual void paintTree(QPainter* p, int indent, int cellHeight,
			   const QColorGroup& cg) const;

    /**
     * Internal function that updates the owner of this item and its
     * children and siblings (the latter only if requested).
     */
    void setOwner(KTreeView* newOwner, bool includeSiblings = false);

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

/**
 * KTreeView is a class that provides a way to display hierarchical data in
 * a single-inheritance tree, similar to tree controls in Microsoft Windows
 * and other GUIs. It is most suitable for directory trees or outlines, but
 * I'm sure other uses will come to mind. It was designed mostly with the
 * above two functions in mind, but I have tried to make it as flexible as
 * I can to make it easy to adapt to other uses.
 *
 * Please read the source code if you have time. I have tried to comment it
 * adequately and make the source understandable.
 *
 * The class features the following:
 *
 * - Displays both text and an optional pixmap supplied by the programmer.
 * A support class, KTreeViewItem, can be inherited and modified to draw
 * items as needed by the programmer.
 *
 * - The list items can be returned by index or logical path and the tree
 * navigated by parent, child or sibling references contained in them.
 * Also, item information such as text, pixmap, branch level can be
 * obtained.
 *
 * - Items can be inserted, changed and removed either by index in the
 * visible structure, or by logical paths through the tree hierarchy.
 *
 * - The logical path through the tree for any item can be obtained with
 * the index of the item.
 *
 * - Tree structure display and expanding/collapsing of sub-trees is
 * handled with no intervention from the programmer.
 *
 * - entire tree can be expanded or collapsed to a specified sub-level
 *  (handy for outline views)
 *
 * @short A collapsible treelist widget
 * @author Johannes Sixt <Johannes.Sixt@telecom.at>, Keith Brown
 */
class KTreeView : public TableView
{
    friend class KTreeViewItem;
    Q_OBJECT
public:
    /**
     * Widget contructor. All parameters are passed on to base TableView,
     * and are not used directly.
     */
    KTreeView(QWidget* parent = 0, const char* name = 0, WFlags f = 0);

    /**
     * Desctructor. Deletes all items from the topmost level that have been
     * marked with setDeleteChildren(true).
     */
    virtual ~KTreeView();

    /**
     * Appends a new child item to the item at the specified row. If that
     * item already has children, the new item is appended below these
     * children. A KTreeViewItem is created for which the delete-children
     * flag is set to true.
     * 
     * @param text specifies text for the new item; must not be 0
     * @param pixmap specifies a pixmap for the new item
     * @param index specifies the item of which the new item will be a child
     */
    void appendChildItem(const char* text, const QPixmap& pixmap,
		      int index);

    /**
     * This overloaded function appends a new item to an item, which is
     * specified by a path.
     * 
     * @param text specifies text for the new item; must not be 0
     * @param pixmap specifies a pixmap for the new item
     * @param path specifies the item of which the new item will be a child
     * @see #appendChildItem
     */
    void appendChildItem(const char* text, const QPixmap& pixmap,
		      const KPath& path); 

    /**
     * Appends the specified item as a child of the item that is at the
     * specified row. If that item already has children, the new item is
     * appended below these children.
     * 
     * @param newItem specifies the new item
     * @param index specifies the item of which the new item will be a child
     * @see #appendChildItem
     */
    void appendChildItem(KTreeViewItem* newItem, int index);

    /**
     * This overloaded function appends a new item to an item, which is
     * specified by a path.
     * 
     * @param newItem specifies the new item
     * @param path specifies the item of which the new item will be a child
     * @see #appendChildItem
     */
    void appendChildItem(KTreeViewItem* newItem, const KPath& path);

    /**
     * Computes coordinates relative to the specified row from the given
     * coordinates. If the row is invalid, the input coordinates are
     * returned unchanged.
     * 
     * @param widget specifies widget coordinates (e.g. from a mouse event)
     * @return coordinates relative to the specified cell
     */
    virtual QPoint cellCoords(int row, const QPoint& widgetCoord);

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
     * Collapses the sub-tree at the specified row index. If the index is
     * out of range, or the item is already collpased, nothing happens.
     * 
     * @param index specifies the row index
     * @param emitSignal specifies whether the signal @ref #collapsed
     * should be emitted
     */
    void collapseItem(int index, bool emitSignal);

    /**
     * Expands the sub-tree at the specified row index. If the index is
     * out of range, or the item is already expanded, nothing happens.
     * 
     * @param index specifies the row index
     * @param emitSignal specifies whether the signal @ref #collapsed
     * should be emitted
     */
    void expandItem(int index, bool emitSignal);

  /**
	Returns the depth to which all parent items are automatically
	expanded.
	*/
  int expandLevel() const;

    /**
     * The type of member functions that is called by @ref #forEveryItem and
     * @ref #forEveryVisibleItem.
     */
    typedef bool (*KForEveryFunc)(KTreeViewItem*, void*);

    /**
     * Iterates every item in the tree, visible or not, and applies the
     * function func with a pointer to each item and user data supplied as
     * parameters. The children of the specified root item are visited
     * (root itself is not visited!). If root is 0 all items in the tree
     * are visited. KForEveryFunc is defined as:
     * 
     * typedef bool (KTreeView::*KForEveryFunc)(KTreeViewItem*, void*);
     * 
     * That is, a member function that returns bool and takes a pointer to
     * a KTreeViewItem and pointer to void as parameters. The traversal
     * ends earlier if the supplied function returns bool. In this case the
     * return value is also true.
     *
     * @param func the member function to call for every visited item
     * @param user extra data that is passed to func
     * @param root the root item of the subtree to scan; this item itself
     * is not scanned
     * @see #forEveryVisibleItem
     */
    bool forEveryItem(KForEveryFunc func, void* user,
		      KTreeViewItem* root = 0);

    /**
     * This function is like @ref #forEveryItem, but only iterates visible
     * items, in order. If the specified root item is invisible no items
     * are visited.
     *
     * @param func the member function to call for every visited item
     * @param user extra data that is passed to func
     * @param root the root item of the subtree to scan; this item itself
     * is not scanned
     * @see #forEveryItem
     */
    bool forEveryVisibleItem(KForEveryFunc func, void *user,
			     KTreeViewItem* root = 0);

  /**
	Returns a pointer to the current item if there is one, or 0.
	*/
  KTreeViewItem *getCurrentItem();

    /**
     * Returns the number of pixels an item is indented for each level. If,
     * in a derived class, the levels are indented differently this value
     * may be ignored.
     * 
     * @return the number of pixels of indentation for a level
     */
    int indentSpacing();

    /**
     * Inserts an item into the tree with the given text and pixmap either
     * before or after the item at the given row. The new item is added to
     * the same branch as the referenced item (that is, the new item will
     * be sibling of the reference item). If row is -1, the item is simply
     * appended to the tree at the topmost level. A KTreeViewItem is
     * created for which the delete-children flag is set to true.
     * 
     * @param text specifies text for the new item; must not be 0
     * @param pixmap specifies a pixmap for the new item
     * @param index specifies the insert position
     * @param prefix if true, the new item is inserted before the reference
     * item, otherwise after it
     * @return true if the item has been successfully inserted in the tree,
     * otherwise false.
     */
    bool insertItem(const char* text, const QPixmap& pixmap,
		    int row = -1, bool prefix = true);

    /**
     * This overloaded function inserts a new item into the tree, but a
     * path through the tree specifies the reference insert position. If
     * there is no item at the specified path, the item is simply appended
     * to the tree at the topmost level.
     * 
     * @param text specifies text for the new item; must not be 0
     * @param pixmap specifies a pixmap for the new item
     * @param path specifies the insert position
     * @param prefix if true, the new item is inserted before the reference
     * item, otherwise after it
     * @return true if the item has been successfully inserted in the tree,
     * otherwise false.
     * @see #insertItem
     */
    bool insertItem(const char* text, const QPixmap& pixmap,
		    const KPath& path, bool prefix = true);

    /**
     * This overloaded function inserts a new item into the tree, but the
     * new item is specified directly. The reference item is specified as a
     * row index.
     * 
     * @param newItem specifies the item to insert
     * @param path specifies the insert position
     * @param prefix if true, the new item is inserted before the reference
     * item, otherwise after it
     * @return true if the item has been successfully inserted in the tree,
     * otherwise false.
     * @see #insertItem
     */
    bool insertItem(KTreeViewItem *newItem, 
		    int row = -1, bool prefix = true); 

    /**
     * This overloaded function inserts a new item into the tree, but the
     * new item is specified directly. The reference item is specified by a
     * path.
     * 
     * @param newItem specifies the item to insert
     * @param path specifies the insert position
     * @param prefix if true, the new item is inserted before the reference
     * item, otherwise after it
     * @return true if the item has been successfully inserted in the tree,
     * otherwise false.
     * @see #insertItem
     */
    bool insertItem(KTreeViewItem *newItem,
		    const KPath& thePath, bool prefix = true);

    /**
     * Returns a pointer to the item in the specified row, or 0 if the
     * specified row is outside the limits. This is a cheap operation.
     * 
     * @param row specifies the row index
     * @return the item at the specified row
     * @see #itemRow
     * @see #itemPath
     */
    KTreeViewItem* itemAt(int row) const;

    /**
     * Returns a pointer to the item at the end of the path, or 0 if there
     * is no such item.
     * 
     * @param path specifies a path through the tree
     * @return the item at the end of the specified path
     * @see #itemRow
     * @see #itemPath
     */
    KTreeViewItem* itemAt(const KPath& path);

    /**
     * Looks up the row index at which the specified item is found in the
     * visible tree or -1 if the item is not visible or not in the tree.
     * 
     * @param specifies the item to search
     * @return the row index of the item
     * @see #itemAt
     * @see #itemPath
     */
    int itemRow(KTreeViewItem* item);

    /**
     * Fills path with the logical path to the item at the specified row.
     * The specified path variable should be empty. Any strings popped from
     * the path must be deleted by the caller. If the row is invalid, path
     * remains unchanged (i.e. empty).
     * 
     * @param row specifies the row index
     * @param path receives the path of the specified item
     * @see #itemAt
     * @see #itemRow
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
     * The specified item is scrolled into view.  If the specified item is
     * already visible, nothing happens, unless children is true, in which
     * case the display is scrolled such that the item and as many of its
     * child items as possible are visible.
     * 
     * @param item specifies the item to make visible
     * @param children specifies whether children should be made visible
     */
    void scrollVisible(KTreeViewItem* item, bool children);

    /**
     * Makes the item at specifies row the current item and highlights it.
     * The signal @ref #highlighted is emitted if the current item changes.
     * 
     * @param row specifies the row to make the current item
     */
    void setCurrentItem(int row);

  void setExpandButtonDrawing(bool enable);

  void setExpandLevel(int level);

    /**
     * Sets the indentation stepping, in pixels.  If, in a derived class,
     * the levels are indented differently this value may be ignored.
     * 
     * @param spacing specifies the new indentation spacing, in pixels
     */
    void setIndentSpacing(int spacing);

    /**
     * If true, removing a top-level item that contains the current item
     * will move the current item to the following sibling (or to the
     * previous if there is none). Otherwise, there will not be a current
     * item.
     */
    void setMoveCurrentToSibling(bool m = true);

  /**
	If enable is TRUE (default), item text will be displayed, otherwise 
	it will not, and no highlight will be shown in the default widget.
	*/
  void setShowItemText(bool enable);

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
     * Use numRows() instead.
     * 
     * @return the number of items that are visible (their parents are
     * expanded).
     * @deprecated
     */
    int visibleCount() const { return numRows(); }

signals:
    /**
     * This signal is emitted when an item in the tree is collapsed.
     * The signal is not emitted for items that are invisible.
     * 
     * @param index the row index of the collapsed item
     */
    void collapsed(int index);
    
    /**
     * This signal is emitted when an item in the tree is expanded.
     * The signal is not emitted for items that are invisible.
     * 
     * @param index the row index of the expanded item
     */
    void expanded(int index);

    /**
     * This signal is emitted when an item that has the delayedExpanding
     * flag set is about to be expanded. The delayedExpanding flag is not
     * reset; the slot that the signal is connected to should do so if
     * necessary. The item being expanded is passed to the slot. The slot
     * gets the opportunity to insert child items into that item. It should
     * not change the item any other way. It can disallow the expansion by
     * setting allow to false; in this case the item is not expanded.
     *
     * The signal is always emitted, regardless whether the expansion was
     * triggered by the user or by the program.
     * 
     * @param item specifies the item that is about to be expanded
     * @param allow can be set to false to disallow expansion; no further
     * actions are taken then
     */
    void expanding(KTreeViewItem* item, bool& allow);

    /**
     * This signal is emitted when an item in the tree is highlighted.
     * 
     * @param index the row index of the highlighted item.
     */
    void highlighted(int index);

    /**
     * This signal is emitted when the user right-clicks.
     * 
     * @param index the row index of where the click occurred; it is -1 if
     * the click was not on an item.
     * @param pt the location (in widget coordinates) where the mouse click
     * happened.
     */
    void rightPressed(int index, const QPoint& pt);

    /**
     * This signal is emitted when an item in the tree is selected.
     * 
     * @param index the row index of the selected item.
     */
    void selected(int index);
  
protected:
    /**
     * Appends a new child item to a parent item as a new direct child. All
     * internal state is updated and the widget is repainted as necessary.
     * The new child remains invisible if any ancestor of it (including the
     * parent) is collapsed.
     * 
     * @param parent specifies the parent of which the new item will become
     * a child
     * @param child specifies the new child item
     * @see #appendChildItem
     */
    void appendChildItem(KTreeViewItem* parent,
			 KTreeViewItem* child);
    virtual int cellHeight(int row) const;
    virtual int cellWidth(int col) const;
    void changeItem(KTreeViewItem* toChange,
		    int itemRow, const char* newText,
		    const QPixmap* newPixmap);
    /**
     * Collapses the specified subtree and updates the display. The
     * specified item need not be visible. This function does nothing if
     * the item is already collapsed.
     * 
     * @param item specifies the item to collapse.
     * @param emitSignal specifies whether the signal @ref #collapsed should be emitted.
     */
    virtual void collapseSubTree(KTreeViewItem* item, bool emitSignal);

    /** Internal function used for counting items */
    static bool countItem(KTreeViewItem* item, void* total);

    /**
     * Expands the specified subtree and updates the display. The specified
     * item need not be visible. This function does nothing if the item is
     * already expanded.
     * 
     * @param item specifies the item to expand.
     * @param emitSignal specifies whether the signal @ref #expanded should be emitted.
     */
    virtual void expandSubTree(KTreeViewItem* item, bool emitSignal);
  void fixChildren(KTreeViewItem *parentItem);
    virtual void focusInEvent(QFocusEvent* e);
    virtual void focusOutEvent(QFocusEvent* e);

    /** internal function used to determine maximum item width */
    static bool getMaxItemWidth(KTreeViewItem* item, void *user);

    /**
     * @param specifies a tree item of this tree
     * @return the total indentation of the specified item, in pixels
     */
    virtual int indentation(KTreeViewItem* item) const;

    /**
     * Inserts a new item before or after a reference item. (That is, the
     * new item will become a sibling of the reference item.) If the
     * reference item is 0, the new item is appended at the topmost level.
     * If the reference item is not 0, it must be an item that is already
     * in this KTreeView. Internal data is updated and the display is
     * refreshed as necessary. The inserted item may still be invisible if
     * any of the parents is collapsed.
     * 
     * @param referenceItem specifies the reference item
     * @param newItem specifies the new item; must not be 0.
     * @return true if the item has been successfully inserted in the tree,
     * otherwise false.
     * @see #insertItem
     */
    bool insertItem(KTreeViewItem* referenceItem, KTreeViewItem* newItem,
		    bool prefix);

    /**
     * Finds the logical path of the specified item. The specified path
     * variable should be empty.
     * 
     * @param item specifies the item whose path is to determine
     * @param path receives the path of the item
     * @see #itemPath
     * @see #itemRow
     * @see #itemAt
     */
    void itemPath(KTreeViewItem* item, KPath& path) const;

  void join(KTreeViewItem *item);

    /**
     * Reimplemented for key handling. If there are any items in the
     * KTreeView, but there is no current item, the topmost item is made
     * current. The key press event is first forwarded to the current item
     * by calling @ref #KTreeViewItem::keyEvent.
     */
    virtual void keyPressEvent(QKeyEvent* e);

    /**
     * The key release event is first forwarded to the current item (if
     * there is one) by calling @ref #KTreeViewItem::keyEvent.
     */
    virtual void keyReleaseEvent(QKeyEvent* e);

    int level(KTreeViewItem* item) const;
  void lowerItem(KTreeViewItem *item);

    /**
     * Reimplemented for mouse event handling. The mouse double click event
     * is first forwarded to the item that has been clicked on (if there is
     * one) by calling @ref #KTreeViewItem::mouseEvent.
     */
    virtual void mouseDoubleClickEvent(QMouseEvent* e);

    /**
     * Reimplemented for mouse event handling. The mouse move event is
     * first forwarded to the current item (if there is one) by calling
     * @ref #KTreeViewItem::mouseEvent.
     */
    virtual void mouseMoveEvent(QMouseEvent* e);

    /**
     * Reimplemented for mouse event handling. The mouse press event is
     * first forwarded to the item that has been clicked on (if there is
     * one) by calling @ref #KTreeViewItem::mouseEvent. The clicked on item
     * is made the current item.
     */
    virtual void mousePressEvent(QMouseEvent* e);

    /**
     * Reimplemented for mouse event handling. The mouse release event is
     * first forwarded to the current item (if there is one) by calling
     * @ref #KTreeViewItem::mouseEvent.
     */
    virtual void mouseReleaseEvent(QMouseEvent* e);

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

    void setItemExpanded(KTreeViewItem* item);
    static bool setItemExpandLevel(KTreeViewItem* item, void*);
    static bool setItemExpandButtonDrawing(KTreeViewItem* item, void*);
    static bool setItemShowText(KTreeViewItem* item, void*);
    static bool setItemTreeDrawing(KTreeViewItem* item, void*);
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
    bool moveCurrentToSibling;
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
