// $Id$

// Copyright by Johannes Sixt
// This file is under GPL, the GNU General Public Licence


#include <kapp.h>
#if QT_VERSION >= 200
#include <klocale.h>			/* i18n */
/*
 * Presence or absence of I18N_NOOP distinguishes between KRASH release and
 * "modern" KDE 2.
 */
#ifdef I18N_NOOP
# define HAVE_KDE2_POST_KRASH 1
#else
# undef HAVE_KDE2_POST_KRASH
#endif

#include <kmessagebox.h>
#include <kglobal.h>
#include <kstddirs.h>

#ifdef HAVE_KDE2_POST_KRASH
# include <kcmdlineargs.h> 
# include <kaboutdata.h>
# include <khelpmenu.h> 
#endif

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
#ifndef HAVE_KDE2_POST_KRASH
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
    
    KApplication app;
#endif
    extern char *optarg;
    extern int optind;
    int ch;

#if QT_VERSION >= 200
    KGlobal::dirs()->addResourceType("types", "share/apps/kdbg/types");
#endif

    keys = new KStdAccel();

    DebuggerMainWnd debugger("kdbg_main");

    // insert help menu
    QPopupMenu* helpMenu;
#ifndef HAVE_KDE2_POST_KRASH
# if QT_VERSION < 200
    helpMenu = kapp->getHelpMenu(false, about);
# else
    helpMenu = debugger.helpMenu(about, false);
# endif
#else // KDE2 post-KRASH
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

    // handle optional arguments
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

    // check environment variable for transcript file name
    if (transcript == 0) {
	transcript = getenv("KDBG_TRANSCRIPT");
    }
    debugger.setTranscript(transcript);

    if (!restored && argc > 1) {
	// check for core file
	if (argc > 2) {
	    debugger.setCoreFile(argv[2]);
	}
	if (!debugger.debugProgram(argv[1])) {
	    // failed
	    TRACE("cannot start debugger");
#if QT_VERSION < 200
	    KMsgBox::message(&debugger, kapp->appName(),
			     i18n("Cannot start debugger."),
			     KMsgBox::STOP,
			     i18n("OK"));
#else
	    KMessageBox::error(&debugger, i18n("Cannot start debugger."));
#endif
	    debugger.setCoreFile("");
	}
    }

    int rc = app.exec();
    return rc;
}
