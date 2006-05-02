// $Id$

// Copyright by Johannes Sixt
// This file is under GPL, the GNU General Public Licence


#include <kapp.h>
#include <klocale.h>			/* i18n */
#include <kmessagebox.h>
#include <kglobal.h>
#include <kstddirs.h>
#include <kcmdlineargs.h> 
#include <kaboutdata.h>
#include <kpopupmenu.h>
#include <kmenubar.h>
#include "dbgmainwnd.h"
#include "typetable.h"
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#ifndef VERSION
#define VERSION ""
#endif
#ifdef HAVE_FCNTL_H
#include <fcntl.h>			/* open(2) */
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>			/* getopt(3) */
#endif
#include <stdlib.h>			/* getenv(3) */
#include "mydebug.h"


int main(int argc, char** argv)
{
    KAboutData aboutData("kdbg", "KDbg",
			 VERSION,
			 I18N_NOOP("A Debugger"),
			 KAboutData::License_GPL, 
			 "(c) 1998-2005 Johannes Sixt",
			 0,		/* any text */
			 "http://www.kdbg.org/",
			 "Johannes.Sixt@telecom.at");
    aboutData.addAuthor("Johannes Sixt", 0, "Johannes.Sixt@telecom.at");
    aboutData.addCredit("Keith Isdale",
			I18N_NOOP("XSLT debugging"),
			"k_isdale@tpg.com.au");
    aboutData.addCredit("Daniel Kristjansson",
			I18N_NOOP("Register groups and formating"),
			"danielk@cat.nyu.edu");
    KCmdLineArgs::init( argc, argv, &aboutData );

    static KCmdLineOptions options[] = {
	{ "t <file>", I18N_NOOP("transcript of conversation with the debugger"), 0 },
	{ "r <device>", I18N_NOOP("remote debugging via <device>"), 0 },
	{ "l <language>", I18N_NOOP("specify language: C, XSLT"), ""},
	{ "x", I18N_NOOP("use language XSLT (deprecated)"), 0 },
	{ "p <pid>", I18N_NOOP("specify PID of process to debug"), 0},
	{ "+[program]", I18N_NOOP("path of executable to debug"), 0 },
	{ "+[core]", I18N_NOOP("a core file to use"), 0},
	{ 0, 0, 0 }
    };
    KCmdLineArgs::addCmdLineOptions(options);
    
    KApplication app;

    KGlobal::dirs()->addResourceType("types", "share/apps/kdbg/types");

    DebuggerMainWnd debugger("kdbg_main");

    /* type libraries */
    TypeTable::initTypeLibraries();

    // session management
    bool restored = false;
    if (app.isRestored()) {
	if (KMainWindow::canBeRestored(1)) {
	    debugger.restore(1);
	    restored = true;
	}
    }

    app.setMainWidget(&debugger);

    debugger.show();

    // handle options

    KCmdLineArgs *args = KCmdLineArgs::parsedArgs();
    QCString t = args->getOption("t");
    const char* transcript = t.data();
    QString remote = args->getOption("r");
    if (!remote.isEmpty())
	debugger.setRemoteDevice(remote);

    QCString lang = args->getOption("l");

    // deprecated option; overrides -l
    if (args->isSet("x")){
         /* run in xsldbg mode  */
         lang = "xslt";
    }

    // check environment variable for transcript file name
    if (transcript == 0) {
	transcript = getenv("KDBG_TRANSCRIPT");
    }
    debugger.setTranscript(transcript);

    QString pid = args->getOption("p");

    if (!restored && args->count() > 0) {
	// attach to process?
	if (!pid.isEmpty()) {
	    TRACE("pid: " + pid);
	    debugger.setAttachPid(pid);
	}
	// check for core file; use it only if we're not attaching to a process
	else if (args->count() > 1 && pid.isEmpty()) {
	    debugger.setCoreFile(args->arg(1));
	}
	if (!debugger.debugProgram(args->arg(0), lang)) {
	    // failed
	    TRACE("cannot start debugger");
	    KMessageBox::error(&debugger, i18n("Cannot start debugger."));

	    debugger.setCoreFile(QString());
	    debugger.setAttachPid(QString());
	}
    }

    int rc = app.exec();
    return rc;
}
