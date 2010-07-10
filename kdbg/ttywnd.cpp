/*
 * Copyright Johannes Sixt
 * This file is licensed under the GNU General Public License Version 2.
 * See the file COPYING in the toplevel directory of the source directory.
 */

#include "ttywnd.h"
#include <QSocketNotifier>
#include <QContextMenuEvent>
#include <QMenu>
#include <kglobalsettings.h>
#include <klocale.h>

#include "config.h"
#include <fcntl.h>
#include <unistd.h>			/* open, close, etc. */
#include <sys/ioctl.h>
#include <sys/stat.h>
#ifdef HAVE_PTY_H
#include <pty.h>			/* openpty on Linux */
#endif
#ifdef HAVE_LIBUTIL_H
#include <libutil.h>			/* openpty on FreeBSD */
#endif
#ifdef HAVE_UTIL_H			/* openpty on NetBSD, OpenBSD */
#include <util.h>
#endif
#include <errno.h>

#include "mydebug.h"


STTY::STTY() :
	QObject(),
	m_masterfd(-1),
	m_slavefd(-1),
	m_outNotifier(0)
{
    if (findTTY())
    {
	::fcntl(m_masterfd, F_SETFL, O_NDELAY);
	m_outNotifier = new QSocketNotifier(m_masterfd, QSocketNotifier::Read);
	connect(m_outNotifier, SIGNAL(activated(int)), SLOT(outReceived(int)));
    } else {
	m_slavetty = QString();
    }
}

STTY::~STTY()
{
    if (m_outNotifier) {
	::close(m_masterfd);
	if (m_slavefd >= 0)
	    ::close(m_slavefd);
	delete m_outNotifier;
    }
}

bool STTY::findTTY()
{
    m_masterfd  = -1;

#ifdef HAVE_FUNC_OPENPTY
    /* use glibc2's openpty */
    if (m_masterfd < 0)
    {
	if (::openpty(&m_masterfd, &m_slavefd, 0, 0, 0) == 0) {
	    const char* tname = ::ttyname(m_slavefd);
	    if (tname != 0) {
		m_slavetty = tname;
	    } else {
		::close(m_slavefd);
		::close(m_masterfd);
		m_masterfd = m_slavefd = -1;
	    }
	}
    }
#endif

    // resort to BSD-style terminals
    if (m_masterfd < 0)
    {
	const char* s3;
	const char* s4;

	char ptynam[] = "/dev/ptyxx";
	char ttynam[] = "/dev/ttyxx";
	static const char ptyc3[] = "pqrstuvwxyzabcde";
	static const char ptyc4[] = "0123456789abcdef";

	// Find a master pty that we can open
	for (s3 = ptyc3; *s3 != 0 && m_masterfd < 0; s3++)
	{
	    for (s4 = ptyc4; *s4 != 0; s4++)
	    {
		ptynam[8] = ttynam[8] = *s3;
		ptynam[9] = ttynam[9] = *s4;
		if ((m_masterfd = ::open(ptynam,O_RDWR)) >= 0)
		{
		    if (::geteuid() == 0 || ::access(ttynam,R_OK|W_OK) == 0)
		    {
			m_slavetty = ttynam;
			break;
		    }
		    ::close(m_masterfd);
		    m_masterfd = -1;
		}
	    }
	}
    }

    return m_masterfd >= 0;
}

void STTY::outReceived(int f)
{
    for (;;) {
	char buf[1024];
	int n = ::read(f, buf, sizeof(buf));
	if (n < 0) {
	    if (errno != EAGAIN) {	/* this is not an error */
		// ugh! error! somebody disconnect this signal please!
	    }
	    break;
	}
	emit output(buf, n);
	if (n == 0)
	    break;
    }
}



TTYWindow::TTYWindow(QWidget* parent) :
	QPlainTextEdit(parent),
	m_tty(0),
	m_pos(document())
{
    setFont(KGlobalSettings::fixedFont());
    setReadOnly(true);
    setWordWrapMode(QTextOption::NoWrap);
    setUndoRedoEnabled(false);
    m_pos.setPosition(0);
}

TTYWindow::~TTYWindow()
{
    if (m_tty)
	deactivate();
}


QString TTYWindow::activate()
{
    // allocate a pseudo terminal
    m_tty = new STTY;

    QString ttyName = m_tty->slaveTTY();
    if (ttyName.isEmpty()) {
	// failed to allocate terminal
	delete m_tty;
	m_tty = 0;
	return QString();
    } else {
	connect(m_tty, SIGNAL(output(char*,int)), SLOT(slotAppend(char*,int)));
	return ttyName;
    }
}

void TTYWindow::deactivate()
{
    delete m_tty;
    m_tty = 0;
}

void TTYWindow::slotAppend(char* buffer, int count)
{
    // parse off lines
    char* start = buffer;
    while (count > 0) {
	int len = 0;
	while (count > 0 && start[len] != '\n' && start[len] != '\r') {
	    --count;
	    ++len;
	}
	if (len > 0) {
	    QString str = QString::fromLatin1(start, len);
	    // replace text in the last line
	    // this selection is non-empty only after a '\r' that was not
	    // followed by a '\n'
	    m_pos.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor, len);
	    m_pos.insertText(str);
	    start += len;
	    len = 0;
	}
	if (count > 0 && *start == '\r') {
	    ++start;
	    --count;
	    m_pos.movePosition(QTextCursor::StartOfLine);
	}
	if (count > 0 && *start == '\n') {
	    ++start;
	    --count;
	    m_pos.movePosition(QTextCursor::End);
	    m_pos.insertText(QString('\n'));
	}
    }
}

void TTYWindow::contextMenuEvent(QContextMenuEvent *event)
{
    QMenu* menu = createStandardContextMenu();
    menu->addSeparator();
    menu->addAction(i18n("&Clear"),this, SLOT(slotClear()));
    menu->exec(event->globalPos());
    delete menu;
}

void TTYWindow::slotClear()
{
    clear();
    m_pos.movePosition(QTextCursor::End);
}

#include "ttywnd.moc"
