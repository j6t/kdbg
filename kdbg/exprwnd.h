// $Id$

// Copyright by Johannes Sixt
// This file is under GPL, the GNU General Public Licence

#ifndef EXPRWND_H
#define EXPRWND_H

#include "ktreeview.h"
#include <qlist.h>

// a variable's value is the tree of sub-variables

class VarTree : public KTreeViewItem
{
public:
    enum VarKind { VKsimple, VKpointer, VKstruct, VKarray,
	VKdummy				/* used to update only children */
    };
    VarKind m_varKind;
    enum NameKind { NKplain, NKstatic, NKtype,
	NKaddress,			/* a dereferenced pointer */
    };
    NameKind m_nameKind;
    QString m_value;
    bool m_valueChanged;

    VarTree(const QString& name, NameKind kind);
    virtual ~VarTree();
public:
    void paintValue(QPainter* painter);
    int valueWidth();
    QString computeExpr() const;
    bool isToplevelExpr() const;
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
    /** removes an expression from the topmost level*/
    void removeExpr(const char* name);
    /** clears the list of pointers needing updates */
    void clearUpdatePtrs();
    /** returns a pointer to update (or 0) and removes it from the list */
    VarTree* nextUpdatePtr();

protected:
    bool updateExprRec(VarTree* display, VarTree* newValues);
    void replaceChildren(VarTree* display, VarTree* newValues);
    virtual void paintCell(QPainter* painter, int row, int col);
    virtual int cellWidth(int col);
    void updateValuesWidth();
    static bool getMaxValueWidth(KTreeViewItem* item, void* user);
    int maxValueWidth;
    QPixmap m_pixPointer;

    QList<VarTree> m_updatePtrs;	/* dereferenced pointers that need update */

protected slots:
    void slotExpandOrCollapse(int);
};

#endif // EXPRWND_H
