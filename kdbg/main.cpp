/*
 * Copyright Johannes Sixt
 * This file is licensed under the GNU General Public License Version 2.
 * See the file COPYING in the toplevel directory of the source directory.
 */

#include <kapplication.h>
#include <klocale.h>			/* i18n */
#include <kmessagebox.h>
#include <kglobal.h>
#include <kstandarddirs.h>
#include <kcmdlineargs.h> 
#include <kaboutdata.h>
#include "dbgmainwnd.h"
#include "typetable.h"
#include "version.h"
#include <stdlib.h>			/* getenv(3) */
#include "mydebug.h"


int main(int argc, char** argv)
{
    KAboutData aboutData("kdbg", "kdbg", ki18n("KDbg"),
			 KDBG_VERSION,
			 ki18n("A Debugger"),
			 KAboutData::License_GPL, 
			 ki18n("(c) 1998-2015 Johannes Sixt"),
			 KLocalizedString(),	/* any text */
			 "http://www.kdbg.org/",
			 "j6t@kdbg.org");
    aboutData.addAuthor(ki18n("Johannes Sixt"), KLocalizedString(), "j6t@kdbg.org");
    aboutData.addCredit(ki18n("Keith Isdale"),
			ki18n("XSLT debugging"),
			"k_isdale@tpg.com.au");
    aboutData.addCredit(ki18n("Daniel Kristjansson"),
			ki18n("Register groups and formatting"),
			"danielk@cat.nyu.edu");
    aboutData.addCredit(ki18n("David Edmundson"),
			ki18n("KDE4 porting"),
			"david@davidedmundson.co.uk");
    KCmdLineArgs::init( argc, argv, &aboutData );

    KCmdLineOptions opts;
    opts.add("t <file>", ki18n("transcript of conversation with the debugger"));
    opts.add("r <device>", ki18n("remote debugging via <device>"));
    opts.add("l <language>", ki18n("specify language: C, XSLT"));
    opts.add("x", ki18n("use language XSLT (deprecated)"));
    opts.add("a <args>", ki18n("specify arguments of debugged executable"));
    opts.add("p <pid>", ki18n("specify PID of process to debug"));
    opts.add("+[program]", ki18n("path of executable to debug"));
    opts.add("+[core]", ki18n("a core file to use"));
    KCmdLineArgs::addCmdLineOptions(opts);

    KApplication app;

    KGlobal::dirs()->addResourceType("types", "data", "kdbg/types");
    KGlobal::dirs()->addResourceType("sessions", "data", "kdbg/sessions");

    DebuggerMainWnd* debugger = new DebuggerMainWnd;
    debugger->setObjectName("mainwindow");

    /* type libraries */
    TypeTable::initTypeLibraries();

    // session management
    bool restored = false;
    if (app.isSessionRestored()) {
	if (KMainWindow::canBeRestored(1)) {
	    debugger->restore(1);
	    restored = true;
	}
    }

    debugger->show();

    // handle options

    KCmdLineArgs *args = KCmdLineArgs::parsedArgs();
    QString transcript = args->getOption("t");
    QString remote = args->getOption("r");
    if (!remote.isEmpty())
	debugger->setRemoteDevice(remote);

    QString lang = args->getOption("l");

    // deprecated option; overrides -l
    if (args->isSet("x")){
         /* run in xsldbg mode  */
         lang = "xslt";
    }

    // check environment variable for transcript file name
    if (transcript.isEmpty()) {
	transcript = getenv("KDBG_TRANSCRIPT");
    }
    debugger->setTranscript(transcript);

    QString pid = args->getOption("p");
    QString programArgs = args->getOption("a");

    if (!restored && args->count() > 0) {
	// attach to process?
	if (!pid.isEmpty()) {
	    TRACE("pid: " + pid);
	    debugger->setAttachPid(pid);
	}
	// check for core file; use it only if we're not attaching to a process
	else if (args->count() > 1 && pid.isEmpty()) {
	    debugger->setCoreFile(args->arg(1));
	}
	if (!debugger->debugProgram(args->arg(0), lang)) {
	    // failed
	    TRACE("cannot start debugger");
	    KMessageBox::error(debugger, i18n("Cannot start debugger."));

	    debugger->setCoreFile(QString());
	    debugger->setAttachPid(QString());
	} else {
	    if (!programArgs.isEmpty()) {
		debugger->overrideProgramArguments(programArgs);
	    }
	}
    }

    int rc = app.exec();
    return rc;
}
