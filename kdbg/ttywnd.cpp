// $Id$

// Copyright by Johannes Sixt
// This file is under GPL, the GNU General Public Licence

#include <qsocketnotifier.h>
#include "ttywnd.h"
#if QT_VERSION >= 200
#include <kglobal.h>
#include <klocale.h>
#else
#include <kapp.h>
#endif

#include "config.h"
#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>			/* open, close, etc. */
#endif
#ifdef HAVE_SYS_IOCTL_H
#include <sys/ioctl.h>
#endif
#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif
#ifdef HAVE_PTY_H
#include <pty.h>			/* openpty */
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
		int e = errno;
		TRACE(QString().sprintf("error reading tty: %d", e));
	    }
	    break;
	}
	emit output(buf, n);
	if (n == 0)
	    break;
    }
}



TTYWindow::TTYWindow(QWidget* parent, const char* name) :
	KTextView(parent, name),
	m_tty(0)
{
#if QT_VERSION < 200
    setFont(kapp->fixedFont);
#else
    setFont(KGlobal::fixedFont());
#endif
    clear();
    setFocusPolicy(StrongFocus);

    // create a context menu
    m_popmenu.insertItem(i18n("&Clear"), this, SLOT(clear()));
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
    // is last line visible?
    bool bottomVisible = lastRowVisible() == m_texts.size()-1;

    // parse off lines
    char* start = buffer;
    while (count > 0) {
	int len = 0;
	while (count > 0 && start[len] != '\n') {
	    --count;
	    ++len;
	}
	if (len > 0) {
#if QT_VERSION < 200
	    QString str(start, len+1);
#else
	    QString str = QString::fromLatin1(start, len);
#endif
	    // update last line
	    str = m_texts[m_texts.size()-1] + str;
	    replaceLine(m_texts.size()-1, str);
	    start += len;
	    len = 0;
	}
	if (count > 0 && *start == '\n') {
	    insertLine(QString());
	    ++start;
	    --count;
	}
    }

    // if last row was visible, scroll down to make it visible again
    if (bottomVisible)
	setTopCell(m_texts.size()-1);
}

void TTYWindow::clear()
{
    m_texts.setSize(0);
    m_width = 300; m_height = 14;	/* Same as in KTextView::KTextView */
    insertLine(QString());
}

void TTYWindow::mousePressEvent(QMouseEvent* mouseEvent)
{
    // Check if right button was clicked.
    if (mouseEvent->button() == RightButton)
    {
	if (m_popmenu.isVisible()) {
	    m_popmenu.hide();
	} else {
	    m_popmenu.popup(mapToGlobal(mouseEvent->pos()));
	}
    } else {
	QWidget::mousePressEvent(mouseEvent);
    }
}

#include "ttywnd.moc"
