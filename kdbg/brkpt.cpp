// $Id$

// Copyright by Johannes Sixt
// This file is under GPL, the GNU General Public Licence

#include <kapp.h>			/* i18n */
#include <kiconloader.h>
#include <ksimpleconfig.h>
#include <qkeycode.h>
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
	m_list(this, "bptable"),
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

    m_btAdd.setText(i18n("&Add"));
    m_btAdd.setMinimumSize(m_btAdd.sizeHint());
    connect(&m_btAdd, SIGNAL(clicked()), this, SLOT(addBP()));

    m_btRemove.setText(i18n("&Remove"));
    m_btRemove.setMinimumSize(m_btRemove.sizeHint());
    connect(&m_btRemove, SIGNAL(clicked()), this, SLOT(removeBP()));

    m_btViewCode.setText(i18n("&View Code"));
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
	m_list.appendItem(bp);
    } else {
	// update list
	m_list.changeItem(i, bp);
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
    if (m_debugger.enqueueCmd(KDebugger::DCbreak, "break " + bpText, true)) {
	// clear text if successfully set
	m_bpEdit.setText("");
    }
}

void BreakpointTable::removeBP()
{
    int sel = m_list.currentItem();
    if (sel < 0 || uint(sel) >= m_brkpts.size())
	return;
    
    Breakpoint* bp = m_brkpts[sel];
    QString cmdString(30);
    cmdString.sprintf("delete %d", bp->num);
    m_debugger.enqueueCmd(KDebugger::DCdelete, cmdString);
}

// this handles the menu entry: toggles breakpoint on and off
void BreakpointTable::doBreakpoint(QString file, int lineNo, bool temporary)
{
    Breakpoint* bp = breakpointByFilePos(file, lineNo);
    if (bp == 0)
    {
	// no such breakpoint, so set a new one
	// strip off directory part of file name
	file.detach();
	int offset = file.findRev("/");
	if (offset >= 0) {
	    file.remove(0, offset+1);
	}
	QString cmdString(file.length() + 30);
	cmdString.sprintf("%sbreak %s:%d", temporary ? "t" : "",
			  file.data(), lineNo+1);
	m_debugger.enqueueCmd(KDebugger::DCbreak, cmdString);
    }
    else
    {
	/*
	 * If the breakpoint is disabled, enable it; if it's enabled,
	 * delete that breakpoint.
	 */
	QString cmdString(30);
	if (bp->enabled) {
	    cmdString.sprintf("delete %d", bp->num);
	    m_debugger.enqueueCmd(KDebugger::DCdelete, cmdString);
	} else {
	    cmdString.sprintf("enable %d", bp->num);
	    m_debugger.enqueueCmd(KDebugger::DCenable, cmdString);
	}
    }
}

void BreakpointTable::doEnableDisableBreakpoint(const QString& file, int lineNo)
{
    Breakpoint* bp = breakpointByFilePos(file, lineNo);
    if (bp == 0)
	return;

    // toggle enabled/disabled state
    QString cmdString(30);
    if (bp->enabled) {
	cmdString.sprintf("disable %d", bp->num);
	m_debugger.enqueueCmd(KDebugger::DCdisable, cmdString);
    } else {
	cmdString.sprintf("enable %d", bp->num);
	m_debugger.enqueueCmd(KDebugger::DCenable, cmdString);
    }
}



Breakpoint* BreakpointTable::breakpointByFilePos(QString file, int lineNo)
{
    // look for exact file name match
    int i;
    for (i = m_brkpts.size()-1; i >= 0; i--) {
	if (m_brkpts[i]->lineNo == lineNo &&
	    m_brkpts[i]->fileName == file)
	{
	    return m_brkpts[i];
	}
    }
    // not found, so try basename
    // strip off directory part of file name
    file.detach();
    int offset = file.findRev("/");
    if (offset < 0) {
	// that was already the basename, no need to scan the list again
	return 0;
    }
    file.remove(0, offset+1);

    for (i = m_brkpts.size()-1; i >= 0; i--) {
	if (m_brkpts[i]->lineNo == lineNo &&
	    m_brkpts[i]->fileName == file)
	{
	    return m_brkpts[i];
	}
    }

    // not found
    return 0;
}

// look if there is at least one temporary breakpoint
bool BreakpointTable::haveTemporaryBP() const
{
    for (int i = m_brkpts.size()-1; i >= 0; i--) {
	if (m_brkpts[i]->temporary)
	    return true;
    }
    return false;
}

/*
 * Breakpoints are saved one per group.
 */
const char BPGroup[] = "Breakpoint %d";
const char File[] = "File";
const char Line[] = "Line";
const char Temporary[] = "Temporary";
const char Enabled[] = "Enabled";

void BreakpointTable::saveBreakpoints(KSimpleConfig* config)
{
    QString groupName(30);
    int i;
    for (i = 0; uint(i) < m_brkpts.size(); i++) {
	groupName.sprintf(BPGroup, i);
	config->setGroup(groupName);
	Breakpoint* bp = m_brkpts[i];
	config->writeEntry(File, bp->fileName);
	config->writeEntry(Line, bp->lineNo);
	config->writeEntry(Temporary, bp->temporary);
	config->writeEntry(Enabled, bp->enabled);
    }
    // delete remaining groups
    // we recognize that a group is present if there is an Enabled entry
    for (;; i++) {
	groupName.sprintf(BPGroup, i);
	config->setGroup(groupName);
	if (!config->hasKey(Enabled)) {
	    /* group not present, assume that we've hit them all */
	    break;
	}
	config->deleteGroup(groupName);
    }
}

void BreakpointTable::restoreBreakpoints(KSimpleConfig* config)
{
    QString groupName(30);
    QString fileName;
    int lineNo;
    bool enabled, temporary;
    /*
     * We recognize the end of the list if there is no Enabled entry
     * present.
     */
    for (int i = 0;; i++) {
	groupName.sprintf(BPGroup, i);
	config->setGroup(groupName);
	if (!config->hasKey(Enabled)) {
	    /* group not present, assume that we've hit them all */
	    break;
	}
	fileName = config->readEntry(File);
	lineNo = config->readNumEntry(Line, -1);
	if (lineNo < 0 || fileName.isEmpty())
	    continue;
	enabled = config->readBoolEntry(Enabled, true);
	temporary = config->readBoolEntry(Temporary, false);
	/*
	 * Add the breakpoint. We assume that we have started a new
	 * instance of gdb, because we assign the breakpoint ids ourselves,
	 * starting with 1. Then we use this id to disable the breakpoint,
	 * if necessary. If this assignment of ids doesn't work, (maybe
	 * because this isn't a fresh gdb at all), we disable the wrong
	 * breakpoint! Oh well... for now it works.
	 */
	QString cmdString(fileName.length() + 30);
	cmdString.sprintf("%sbreak %s:%d", temporary ? "t" : "",
			  fileName.data(), lineNo+1);
	m_debugger.enqueueCmd(KDebugger::DCbreak, cmdString);
	if (!enabled) {
	    cmdString.sprintf("disable %d", i+1);
	    m_debugger.enqueueCmd(KDebugger::DCdisable, cmdString);
	}

	cmdString.sprintf("%s:%d", fileName.data(), lineNo);
	insertBreakpoint(i+1, !temporary, enabled,
			 cmdString, fileName, lineNo);
    }
}


BreakpointListBox::BreakpointListBox(QWidget* parent, const char* name) :
	KTabListBox(parent, name, 2)
{
    setColumn(0, i18n("E"),		/* Enabled/disabled */
	      16, KTabListBox::PixmapColumn);
    setColumn(1, i18n("Location"), 300);

    setMinimumSize(200, 100);
    /* work around a non-feature of KTabListBox: */
    setFocusPolicy(QWidget::StrongFocus);

    setSeparator('\t');

    // add pixmaps
    KIconLoader* loader = kapp->getIconLoader();
    dict().insert("E", new QPixmap(loader->loadIcon("green-bullet.xpm")));
    dict().insert("D", new QPixmap(loader->loadIcon("red-bullet.xpm")));
}

BreakpointListBox::~BreakpointListBox()
{
}

void BreakpointListBox::appendItem(Breakpoint* bp)
{
    QString t = constructListText(bp);
    KTabListBox::appendItem(t);
}

void BreakpointListBox::changeItem(int id, Breakpoint* bp)
{
    QString t = constructListText(bp);
    KTabListBox::changeItem(t, id);
}

QString BreakpointListBox::constructListText(Breakpoint* bp)
{
    static const char ED[][4] = { "D\t\0", "E\t\0" };
    return ED[bp->enabled] + bp->location;
}

/* Grrr! Why doesn't do KTablistBox the keyboard handling? */
void BreakpointListBox::keyPressEvent(QKeyEvent* e)
{
    if (numRows() == 0)
	return;

//    int newX;
    switch (e->key()) {
    case Key_Up:
	// make previous item current, scroll up if necessary
	if (currentItem() > 0) {
	    setCurrentItem(currentItem() - 1);
	    if (currentItem() < topItem() || currentItem() > lastRowVisible()) {
		setTopItem(currentItem());
	    }
	}
	break;
    case Key_Down:
	// make next item current, scroll down if necessary
	if (currentItem() < numRows() - 1) {
	    setCurrentItem(currentItem() + 1);
	    if (currentItem() > lastRowVisible()) {
		setTopItem(topItem() + currentItem() - lastRowVisible());
	    } else if (currentItem() < topItem()) {
		setTopItem(currentItem());
	    }
	}
	break;

#if 0
    // scroll left and right by a few pixels
    case Key_Left:
	newX = xOffset() - viewWidth()/10;
	setXOffset(QMAX(newX, 0));
	break;
    case Key_Right:
	newX = xOffset() + viewWidth()/10;
	setXOffset(QMIN(newX, maxXOffset()));
	break;
#endif

    default:
	QWidget::keyPressEvent(e);
	break;
    }
}
