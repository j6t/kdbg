// $Id$

// Copyright by Johannes Sixt
// This file is under GPL, the GNU General Public Licence


#include <kapp.h>
#include <kmsgbox.h>
#include <kstdaccel.h>
#include "debugger.h"
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#ifdef HAVE_FCNTL_H
#include <fcntl.h>			/* open(2) */
#endif
#include <kdebug.h>

static void ioWinProg(const char* fifoName);

QString thisExecName;

int main(int argc, char** argv)
{
    
    // check for special invocation as dummy terminal window holder
    const char* fifoName = getenv("_KDBG_TTYIOWIN");
    if (fifoName != 0) {
	ioWinProg(fifoName);
	return 0;
    }

    thisExecName = argv[0];

    KApplication app(argc, argv, "kdbg");

    keys = new KStdAccel(app.getConfig());

    KDebugger debugger("debugger");

    app.setMainWidget(&debugger);

    if (argc > 1) {
	if (!debugger.debugProgram(argv[1])) {
	    // failed
	    TRACE("cannot start debugger");
	    KMsgBox::message(&debugger, kapp->appName(),
			     klocale->translate("Cannot start debugger.\nExiting..."),
			     KMsgBox::STOP,
			     klocale->translate("OK"));
//	    delete keys;
	    return 1;
	}
    } else {
	/*
	 * Ask for executable; this is a workaround since the menu
	 * doesn't work (kdbg simply exits; this appears to be a bug in Qt).
	 */
	debugger.menuCallbackExecutable();
    }

    debugger.show();

    int rc = app.exec();
//    delete keys;
    return rc;
}

/*
 * The following is stolen from xxgdb. This function prints the tty name to
 * fifoName, then falls into an endless loop. kdbg uses the tty name
 * to run gdb's executable on it.
 */
static void ioWinProg(const char* fifoName)
{
    // get tty name of stdin channel
    char* myname = ttyname(0);
    
#ifndef NDEBUG
    if (myname != 0) {
	write(1, myname, strlen(myname));
    } else {
	write(1, "stdin not a tty", 15);
    }
    write(1, "\n\n", 2);
#endif

    int f = open(fifoName, O_WRONLY);
    if (f < 0) {
	QString error;
	error.sprintf("cannot open %s\n", fifoName);
	write(2, error, error.length());
	return;
    }

    // send over ttyname
    QString tty;
    tty.sprintf("%s,%d", myname, getpid());
    write(f, tty, tty.length());
    close(f);

    // ignore some signals
    struct sigaction act;
    act.sa_handler = SIG_IGN;
    sigemptyset(&act.sa_mask);
    act.sa_flags = 0;			/* not SA_ONESHOT */
    sigaction(SIGINT, &act, 0);
    sigaction(SIGQUIT, &act, 0);
    sigaction(SIGTSTP, &act, 0);

    // start a new process group
#ifdef HAVE_SETPGID
    setpgid(0,0);
#else
    setpgrp(0,0);
#endif

    // dont need input and output anymore
    close(0);
    close(1);

    // go to eternal sleep
    while (1) {
	pause();
    }
}
