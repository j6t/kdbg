// $Id$

// Copyright by Johannes Sixt
// This file is under GPL, the GNU General Public Licence

class VarTree;

// parse string s for a variable tree
// if successful, return a tree (caller must delete)
extern VarTree* parseVar(const char*& s);

void skipNestedWithString(const char*& s, char opening, char closing);
void skipNested(const char*& s, char opening, char closing);
