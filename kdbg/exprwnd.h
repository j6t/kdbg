// $Id$

// Copyright by Johannes Sixt
// This file is under GPL, the GNU General Public Licence

#ifndef EXPRWND_H
#define EXPRWND_H

#include "ktreeview.h"
#include <qlineedit.h>
#include <qptrlist.h>
#include <qstrlist.h>

class ProgramTypeTable;
class TypeInfo;

/* a variable's value is the tree of sub-variables */

class VarTree : public KTreeViewItem
{
public:
    enum VarKind { VKsimple, VKpointer, VKstruct, VKarray,
	VKdummy				/* used to update only children */
    };
    VarKind m_varKind;
    enum NameKind { NKplain, NKstatic, NKtype,
	NKaddress			/* a dereferenced pointer */
    };
    NameKind m_nameKind;
    QString m_value;
    bool m_valueChanged;
    TypeInfo* m_type;			/* type of struct */
    int m_exprIndex;			/* used in struct value update */
    bool m_exprIndexUseGuard;		/* ditto; if guard expr should be used */
    QString m_partialValue;		/* while struct value update is in progress */

    VarTree(const QString& name, NameKind kind);
    virtual ~VarTree();
public:
    void paintValue(QPainter* painter);
    int valueWidth();
    QString computeExpr() const;
    bool isToplevelExpr() const;
    /** is this element an ancestor of (or equal to) child? */
    bool isAncestorEq(const VarTree* child) const;
    /** update the value; return if repaint is necessary */
    bool updateValue(const QString& newValue);
    /** find out the type of this value using the child values */
    void inferTypesOfChildren(ProgramTypeTable& typeTable);
    /** get the type from base class part */
    TypeInfo* inferTypeFromBaseClass();
    /** returns whether the pointer is a wchar_t */
    bool isWcharT() const;
};


class ExprWnd;

class ValueEdit : public QLineEdit
{
    Q_OBJECT
public:
    ValueEdit(ExprWnd* parent);
    ~ValueEdit();

    void terminate(bool commit);
    int m_row;
    bool m_finished;
protected:
    void keyPressEvent(QKeyEvent *e);
    void focusOutEvent(QFocusEvent* ev);
    void paintEvent(QPaintEvent* e);
public slots:
    void slotSelectionChanged();
signals:
    void done(int, const QString&);
};


class ExprWnd : public KTreeView
{
    Q_OBJECT
public:
    ExprWnd(QWidget* parent, const char* name);
    ~ExprWnd();

    /** fills the list with the expressions at the topmost level */
    void exprList(QStrList& exprs);
    /** appends var the the end of the tree at the topmost level */
    void insertExpr(VarTree* expr);
    /** updates an existing expression */
    void updateExpr(VarTree* expr);
    void updateExpr(VarTree* display, VarTree* newValues);
    /** updates the value and repaints it for a single item (not the children) */
    void updateSingleExpr(VarTree* display, VarTree* newValues);
    /** updates only the value of the node */
    void updateStructValue(VarTree* display);
    /** get a top-level expression by name */
    VarTree* topLevelExprByName(const char* name);
    /** removes an expression; must be on the topmost level*/
    void removeExpr(VarTree* item);
    /** retrieves the expression string at the specified line */
    QString exprStringAt(int index);
    /** clears the list of pointers needing updates */
    void clearPendingUpdates();
    /** returns a pointer to update (or 0) and removes it from the list */
    VarTree* nextUpdatePtr();
    VarTree* nextUpdateType();
    VarTree* nextUpdateStruct();
    void editValue(int row, const QString& text);
    /** tells whether the a value is currently edited */
    bool isEditing() const;

protected:
    bool updateExprRec(VarTree* display, VarTree* newValues);
    void replaceChildren(VarTree* display, VarTree* newValues);
    virtual void paintCell(QPainter* painter, int row, int col);
    virtual int cellWidth(int col) const;
    void updateValuesWidth();
    static bool getMaxValueWidth(KTreeViewItem* item, void* user);
    void collectUnknownTypes(VarTree* item);
    static bool collectUnknownTypes(KTreeViewItem* item, void* user);
    int maxValueWidth;
    QPixmap m_pixPointer;

    QList<VarTree> m_updatePtrs;	/* dereferenced pointers that need update */
    QList<VarTree> m_updateType;	/* structs whose type must be determined */
    QList<VarTree> m_updateStruct;	/* structs whose nested value needs update */

    ValueEdit m_edit;

    /** remove items that are in the subTree from the list */
    static void sweepList(QList<VarTree>& list, VarTree* subTree);

protected slots:
    void slotExpandOrCollapse(int);

signals:
    void editValueCommitted(int, const QString&);
};

#endif // EXPRWND_H
