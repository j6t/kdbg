// $Id$

// Copyright by Johannes Sixt
// This file is under GPL, the GNU General Public Licence


#include <kapp.h>
#if QT_VERSION >= 200
#include <klocale.h>			/* i18n */
#endif
#include <kmsgbox.h>
#include <kstdaccel.h>
#include "dbgmainwnd.h"
#include "typetable.h"
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#ifdef HAVE_FCNTL_H
#include <fcntl.h>			/* open(2) */
#endif
#include "mydebug.h"


int main(int argc, char** argv)
{
    KApplication app(argc, argv, "kdbg");

    keys = new KStdAccel(app.getConfig());

    DebuggerMainWnd debugger("kdbg_main");

    /* type libraries */
    TypeTable::initTypeLibraries();

    // session management
    bool restored = false;
    if (app.isRestored()) {
	if (KTMainWindow::canBeRestored(1)) {
	    debugger.restore(1);
	    restored = true;
	}
    }

    app.setMainWidget(&debugger);

    debugger.show();

    if (!restored && argc > 1) {
	// check for core file
	if (argc > 2) {
	    debugger.setCoreFile(argv[2]);
	}
	if (!debugger.debugProgram(argv[1])) {
	    // failed
	    TRACE("cannot start debugger");
	    KMsgBox::message(&debugger, kapp->appName(),
			     i18n("Cannot start debugger."),
			     KMsgBox::STOP,
			     i18n("OK"));
	    debugger.setCoreFile("");
//	    delete keys;
	}
    }

    int rc = app.exec();
//    delete keys;
    return rc;
}
