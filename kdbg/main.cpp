// $Id$

// Copyright by Johannes Sixt
// This file is under GPL, the GNU General Public Licence


#include <kapp.h>
#if QT_VERSION >= 200
#include <klocale.h>			/* i18n */
#include <kmessagebox.h>
#include <kglobal.h>
#include <kstddirs.h>
#include <kcmdlineargs.h> 
#include <kaboutdata.h>
#include <khelpmenu.h> 
#else // QT_VERSION < 200
#include <kmsgbox.h>
#endif
#include <kstdaccel.h>
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
#if QT_VERSION < 200
    KApplication app(argc, argv, "kdbg");

    QString about = "KDbg " VERSION " - ";
    about += i18n("A Debugger\n"
		  "by Johannes Sixt <Johannes.Sixt@telecom.at>\n"
		  "with the help of many others");
#else
    static const char* description =
	i18n("A Debugger");

    KAboutData aboutData("kdbg", "KDbg",
			 VERSION,
			 description,
			 KAboutData::License_GPL, 
			 "(c) 1998-2000 Johannes Sixt",
			 0,		/* any text */
			 "http://members.telecom.at/~johsixt/kdbg.html",
			 "Johannes.Sixt@telecom.at");
    aboutData.addAuthor("Johannes Sixt", 0, "Johannes.Sixt@telecom.at");
    aboutData.addCredit("Judin Max",
			i18n("Docking windows"),
			"novaprint@mtu-net.ru");
    KCmdLineArgs::init( argc, argv, &aboutData );

    static KCmdLineOptions options[] = {
	{ "t <file>", I18N_NOOP("transcript of conversation with the debugger"), 0 },
	{ "r <device>", I18N_NOOP("remote debugging via <device>"), 0 },
	{ "+[program]", I18N_NOOP("path of executable to debug"), 0 },
	{ "+[core]", I18N_NOOP("a core file to use"), 0}
    };
    KCmdLineArgs::addCmdLineOptions(options);
    
    KApplication app;

    KGlobal::dirs()->addResourceType("types", "share/apps/kdbg/types");
#endif

    keys = new KStdAccel();

    DebuggerMainWnd debugger("kdbg_main");

    // insert help menu
    QPopupMenu* helpMenu;
#if QT_VERSION < 200
    helpMenu = kapp->getHelpMenu(false, about);
#else
    KHelpMenu* khm = new KHelpMenu(&debugger, &aboutData, false);
    helpMenu = khm->menu();
#endif
    debugger.menuBar()->insertSeparator();
    debugger.menuBar()->insertItem(i18n("&Help"), helpMenu);

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

    // handle options

#if QT_VERSION < 200
    extern char *optarg;
    extern int optind;
    int ch;

    const char* transcript = 0;
    while ((ch = getopt(argc, argv, "r:t:")) != -1) {
	switch (ch) {
	case 'r':
	    debugger.setRemoteDevice(optarg);
	    break;
	case 't':
	    transcript = optarg;
	    break;
	default:
	    TRACE(QString().sprintf("ignoring option -%c", ch));
	}
    }
    argc -= optind - 1;
    argv += optind - 1;
#else
    KCmdLineArgs *args = KCmdLineArgs::parsedArgs();
    QCString t = args->getOption("t");
    const char* transcript = t.data();
    QString remote = args->getOption("r");
    if (!remote.isEmpty())
	debugger.setRemoteDevice(remote);
#endif
    // check environment variable for transcript file name
    if (transcript == 0) {
	transcript = getenv("KDBG_TRANSCRIPT");
    }
    debugger.setTranscript(transcript);

#if QT_VERSION < 200
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
	}
    }
#else
    if (!restored && args->count() > 0) {
	// check for core file
	if (args->count() > 1) {
	    debugger.setCoreFile(args->arg(1));
	}
	if (!debugger.debugProgram(args->arg(0))) {
	    // failed
	    TRACE("cannot start debugger");
	    KMessageBox::error(&debugger, i18n("Cannot start debugger."));
	    debugger.setCoreFile("");
	}
    }
#endif

    int rc = app.exec();
    return rc;
}
