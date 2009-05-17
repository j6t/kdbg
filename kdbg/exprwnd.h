/*
 * Copyright Johannes Sixt
 * This file is licensed under the GNU General Public License Version 2.
 * See the file COPYING in the toplevel directory of the source directory.
 */

#ifndef EXPRWND_H
#define EXPRWND_H

#include "qlistview.h"
#include <qlineedit.h>
#include <qpixmap.h>
#include <list>

class ProgramTypeTable;
class TypeInfo;
struct ExprValue;
class ExprWnd;
class QStringList;

/*! \brief a variable's value is the tree of sub-variables */
class VarTree : public QListViewItem
{
public:
    enum VarKind { VKsimple, VKpointer, VKstruct, VKarray,
	VKdummy				//!< used to update only children
    };
    VarKind m_varKind;
    enum NameKind { NKplain, NKstatic, NKtype,
	NKanonymous,			//!< an anonymous struct or union
	NKaddress			//!< a dereferenced pointer
    };
    NameKind m_nameKind;
    TypeInfo* m_type;			//!< the type of struct if it could be derived
    int m_exprIndex;			//!< used in struct value update
    bool m_exprIndexUseGuard;		//!< ditto; if guard expr should be used
    QString m_partialValue;		//!< while struct value update is in progress

    VarTree(VarTree* parent, QListViewItem* after, ExprValue* v);
    VarTree(ExprWnd* parent, QListViewItem* after, const QString& name);
    virtual ~VarTree();
public:
    virtual void paintCell(QPainter* p, const QColorGroup& cg, int column, int width, int align);
    QString computeExpr() const;
    bool isToplevelExpr() const;
    /** is this element an ancestor of (or equal to) child? */
    bool isAncestorEq(const VarTree* child) const;
    /** update the regular value; returns whether a repaint is necessary */
    bool updateValue(const QString& newValue);
    /** update the "quick member" value; returns whether repaint is necessary */
    bool updateStructValue(const QString& newValue);
    /** find out the type of this value using the child values */
    void inferTypesOfChildren(ProgramTypeTable& typeTable);
    /** get the type from base class part */
    TypeInfo* inferTypeFromBaseClass();
    /** returns whether the pointer is a wchar_t */
    bool isWcharT() const;

    QString getText() const { return text(0); }
    void setText(const QString& t) { QListViewItem::setText(0, t); }
    void setPixmap(const QPixmap& p) { QListViewItem::setPixmap(0, p); }
    QString value() const { return m_baseValue; }
    VarTree* firstChild() const { return static_cast<VarTree*>(QListViewItem::firstChild()); }
    VarTree* nextSibling() const { return static_cast<VarTree*>(QListViewItem::nextSibling()); }

private:
    void updateValueText();
    QString m_baseValue;	//!< The "normal value" that the driver reported
    QString m_structValue;	//!< The "quick member" value
    bool m_baseChanged : 1;
    bool m_structChanged : 1;
};

/**
 * Represents the value tree that is parsed by the debugger drivers.
 */
struct ExprValue
{
    QString m_name;
    QString m_value;
    VarTree::VarKind m_varKind;
    VarTree::NameKind m_nameKind;
    ExprValue* m_child;			/* the first child expression */
    ExprValue* m_next;			/* the next sibling expression */
    bool m_initiallyExpanded;

    ExprValue(const QString& name, VarTree::NameKind kind);
    ~ExprValue();

    void appendChild(ExprValue* newChild);
    int childCount() const;
};


class ValueEdit : public QLineEdit
{
    Q_OBJECT
public:
    ValueEdit(ExprWnd* parent);
    ~ValueEdit();

    void terminate(bool commit);
    VarTree* m_item;
    bool m_finished;
protected:
    void keyPressEvent(QKeyEvent *e);
    void focusOutEvent(QFocusEvent* ev);
    void paintEvent(QPaintEvent* e);
public slots:
    void slotSelectionChanged();
signals:
    void done(VarTree*, const QString&);
};


class ExprWnd : public QListView
{
    Q_OBJECT
public:
    ExprWnd(QWidget* parent, const QString& colHeader, const char* name);
    ~ExprWnd();

    /** returns the list with the expressions at the topmost level */
    QStringList exprList() const;
    /** appends a copy of expr to the end of the tree at the topmost level;
     * returns a pointer to the inserted top-level item */
    VarTree* insertExpr(ExprValue* expr, ProgramTypeTable& typeTable);
    /** updates an existing expression */
    void updateExpr(ExprValue* expr, ProgramTypeTable& typeTable);
    void updateExpr(VarTree* display, ExprValue* newValues, ProgramTypeTable& typeTable);
    /** updates the value and repaints it for a single item (not the children) */
    void updateSingleExpr(VarTree* display, ExprValue* newValues);
    /** updates only the value of the node */
    void updateStructValue(VarTree* display);
    /** get a top-level expression by name */
    VarTree* topLevelExprByName(const QString& name) const;
    /** return a member of the struct that pointer \a v refers to */
    static VarTree* ptrMemberByName(VarTree* v, const QString& name);
    /** return a member of the struct \a v */
    static VarTree* memberByName(VarTree* v, const QString& name);
    /** removes an expression; must be on the topmost level*/
    void removeExpr(VarTree* item);
    /** clears the list of pointers needing updates */
    void clearPendingUpdates();
    /** returns a pointer to update (or 0) and removes it from the list */
    VarTree* nextUpdatePtr();
    VarTree* nextUpdateType();
    VarTree* nextUpdateStruct();
    void editValue(VarTree* item, const QString& text);
    /** tells whether the a value is currently edited */
    bool isEditing() const;

    VarTree* firstChild() const { return static_cast<VarTree*>(QListView::firstChild()); }
    VarTree* currentItem() const { return static_cast<VarTree*>(QListView::currentItem()); }
    VarTree* selectedItem() const { return static_cast<VarTree*>(QListView::selectedItem()); }

protected:
    void updateExprRec(VarTree* display, ExprValue* newValues, ProgramTypeTable& typeTable);
    void replaceChildren(VarTree* display, ExprValue* newValues);
    void collectUnknownTypes(VarTree* item);
    void checkUnknownType(VarTree* item);
    static QString formatWCharPointer(QString value);
    QPixmap m_pixPointer;

    std::list<VarTree*> m_updatePtrs;	//!< dereferenced pointers that need update
    std::list<VarTree*> m_updateType;	//!< structs whose type must be determined
    std::list<VarTree*> m_updateStruct;	//!< structs whose nested value needs update

    ValueEdit* m_edit;

    /** remove items that are in the subTree from the list */
    void unhookSubtree(VarTree* subTree);
    static void unhookSubtree(std::list<VarTree*>& list, VarTree* subTree);

signals:
    void removingItem(VarTree*);
    void editValueCommitted(VarTree*, const QString&);
};

#endif // EXPRWND_H
