// $Id$

// Copyright by Johannes Sixt
// This file is under GPL, the GNU General Public Licence

#include <kapp.h>			/* i18n */
#include "debugger.h"			/* #includes brkpt.h */
#include "brkpt.moc"
#include <ctype.h>
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include "mydebug.h"


BreakpointTable::BreakpointTable(KDebugger& deb) :
	QDialog(0, "breakpoints"),
	m_debugger(deb),
	m_bpEdit(this, "bpedit"),
	m_list(this, "bptable", 1),
	m_btAdd(this, "add"),
	m_btRemove(this, "remove"),
	m_btViewCode(this, "view"),
	m_btClose(this, "close"),
	m_layout(this, 8),
	m_listandedit(8),
	m_buttons(8)
{
    m_bpEdit.setMinimumSize(m_bpEdit.sizeHint());
    connect(&m_bpEdit, SIGNAL(returnPressed()), this, SLOT(addBP()));

    m_list.setColumn(0, i18n("Location"));
    m_list.setColumnWidth(0, 300);
    m_list.setMinimumSize(200, 100);

    m_btAdd.setText(i18n("Add"));
    m_btAdd.setMinimumSize(m_btAdd.sizeHint());
    connect(&m_btAdd, SIGNAL(clicked()), this, SLOT(addBP()));

    m_btRemove.setText(i18n("Remove"));
    m_btRemove.setMinimumSize(m_btRemove.sizeHint());

    m_btViewCode.setText(i18n("View Code"));
    m_btViewCode.setMinimumSize(m_btViewCode.sizeHint());

    m_btClose.setText(i18n("Close"));
    m_btClose.setMinimumSize(m_btClose.sizeHint());
    connect(&m_btClose, SIGNAL(clicked()), this, SLOT(hide()));

    m_layout.addLayout(&m_listandedit, 10);
    m_layout.addLayout(&m_buttons);
    m_listandedit.addWidget(&m_bpEdit);
    m_listandedit.addWidget(&m_list, 10);
    m_buttons.addWidget(&m_btAdd);
    m_buttons.addWidget(&m_btRemove);
    m_buttons.addWidget(&m_btViewCode);
    m_buttons.addWidget(&m_btClose);
    m_buttons.addStretch(10);

    m_layout.activate();

    resize(350, 300);

    m_bpEdit.setFocus();
}

BreakpointTable::~BreakpointTable()
{
    // delete breakpoint objects
    for (int i = m_brkpts.size()-1; i >= 0; i--) {
	delete m_brkpts[i];
    }
}

void BreakpointTable::updateBreakList(const char* output)
{
    int i;

    // mark all breakpoints as being updated
    for (i = m_brkpts.size()-1; i >= 0; i--) {
	m_brkpts[i]->del = true;
    }

    // get the new list
    parseBreakList(output);

    // delete all untouched breakpoints
    for (i = m_brkpts.size()-1; i >= 0; i--) {
	if (m_brkpts[i]->del) {
	    Breakpoint* bp = m_brkpts[i];
	    // delete the pointer and the object
	    for (uint j = i+1; j < m_brkpts.size(); j++) {
		m_brkpts[j-1] = m_brkpts[j];
	    }
	    m_brkpts.resize(m_brkpts.size()-1);
	    delete bp;
	    m_list.removeItem(i);
	}
    }
}

void BreakpointTable::parseBreakList(const char* output)
{
    // skip first line, which is the headline
    const char* p = strchr(output, '\n');
    if (p == 0)
	return;
    p++;
    if (*p == '\0')
	return;
    // split up a line
    QString location;
    char* end;
    while (*p != '\0') {
	// get Num
	long bpNum = strtol(p, &end, 10);	/* don't care about overflows */
	p = end;
	// skip Type
	while (isspace(*p))
	    p++;
	while (*p != '\0' && !isspace(*p))	/* "breakpoint" */
	    p++;
	while (isspace(*p))
	    p++;
	if (*p == '\0')
	    break;
	// get Disp
	char disp = *p++;
	while (*p != '\0' && !isspace(*p))	/* "keep" or "del" */
	    p++;
	while (isspace(*p))
	    p++;
	if (*p == '\0')
	    break;
	// get Enb
	char enable = *p++;
	while (*p != '\0' && !isspace(*p))	/* "y" or "n" */
	    p++;
	while (isspace(*p))
	    p++;
	if (*p == '\0')
	    break;
	// remainder is location
	end = strchr(p, '\n');
	if (end == 0) {
	    location = p;
	    p += location.length();
	} else {
	    location = QString(p, end-p+1);
	    p = end+1;			/* skip over \n */
	    // next line may contain number of hits
	    if (isspace(*p)) {
		end = strchr(p, '\n');
		if (end == 0) {
		    p += strlen(p);
		} else {
		    p = end+1;
		}
	    }
	}
	insertBreakpoint(bpNum, disp, enable, location);
    }
}

void BreakpointTable::insertBreakpoint(int num, const QString& fileName, int lineNo)
{
    QString str(fileName.length()+20);
    str.sprintf("%s:%d", fileName.data(), lineNo);
    insertBreakpoint(num, 'k', 'y',	/* keep, enabled */
		     str, fileName, lineNo);
}

void BreakpointTable::insertBreakpoint(int num, char disp, char enable, const char* location,
				       const char* fileName, int lineNo)
{
    assert(lineNo != 0);		/* line numbers are 1-based */
    TRACE("insert bp " + QString().setNum(num));
    int numBreaks = m_brkpts.size();
    int i;
    for (i = numBreaks-1; i >= 0; i--) {
	if (m_brkpts[i]->num == num)
	    break;
    }
    Breakpoint* bp;
    if (i < 0) {
	// not found, insert a new one
	bp = new Breakpoint;
	m_brkpts.resize(numBreaks+1);
	m_brkpts[numBreaks] = bp;
	bp->lineNo = -1;
    } else {
	bp = m_brkpts[i];
    }
    bp->num = num;
    bp->temporary = disp == 'd';	/* "del" */
    bp->enabled = enable == 'y';
    bp->location = location;
    if (fileName != 0)
	bp->fileName = fileName;
    if (lineNo > 0)
	bp->lineNo = lineNo-1;
    bp->del = false;
    if (i < 0) {
	// add to list
	m_list.appendItem(location);
    } else {
	// update list
	m_list.changeItem(location, i);
    }
}

void BreakpointTable::parseBreakpoint(const char* output)
{
    const char* o = output;
    // skip lines of that begin with "(Cannot find"
    while (strncmp(o, "(Cannot find", 12) == 0) {
	o = strchr(o, '\n');
	if (o == 0)
	    return;
	o++;				/* skip newline */
    }

    if (strncmp(o, "Breakpoint ", 11) != 0)
	return;
    
    // breakpoint id
    output += 11;			/* skip "Breakpoint " */
    char* p;
    int num = strtoul(output, &p, 10);
    if (p == o)
	return;
    
    // file name
    char* fileStart = strstr(p, "file ");
    if (fileStart == 0)
	return;
    fileStart += 5;
    
    // line number
    char* numStart = strstr(fileStart, ", line ");
    QString fileName(fileStart, numStart-fileStart+1);
    numStart += 7;
    int lineNo = strtoul(numStart, &p, 10);
    if (numStart == p)
	return;
    
    insertBreakpoint(num, fileName, lineNo);
}

void BreakpointTable::closeEvent(QCloseEvent* ev)
{
    QDialog::closeEvent(ev);
    emit closed();
}

void BreakpointTable::hide()
{
    QDialog::hide();
    emit closed();
}

void BreakpointTable::addBP()
{
    // set a breakpoint at the specified text
    QString bpText = m_bpEdit.text();
    bpText = bpText.stripWhiteSpace();
    if (m_debugger.setBreakpoint(bpText)) {
	// clear text if successfully set
	m_bpEdit.setText("");
    }
}
