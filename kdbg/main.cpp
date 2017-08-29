/*
 * Copyright Johannes Sixt
 * This file is licensed under the GNU General Public License Version 2.
 * See the file COPYING in the toplevel directory of the source directory.
 */


#include <klocalizedstring.h>			/* i18n */
#include <kaboutdata.h>
#include <kmessagebox.h>
#include <QApplication>
#include <QCommandLineParser>
#include <QCommandLineOption>
#include "dbgmainwnd.h"
#include "typetable.h"
#include "version.h"
#include <stdlib.h>			/* getenv(3) */
#include "mydebug.h"


int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("kdbg"));
    KLocalizedString::setApplicationDomain("kdbg");

    KAboutData aboutData("kdbg", i18n("KDbg"),
			 KDBG_VERSION,
			 i18n("A Debugger"),
			 KAboutLicense::GPL_V2,
			 i18n("(c) 1998-2017 Johannes Sixt"),
			 {},	/* any text */
			 "http://www.kdbg.org/",
			 "j6t@kdbg.org");
    aboutData.addAuthor(i18n("Johannes Sixt"), QString(), "j6t@kdbg.org");
    aboutData.addCredit(i18n("Keith Isdale"),
			i18n("XSLT debugging"),
			"k_isdale@tpg.com.au");
    aboutData.addCredit(i18n("Daniel Kristjansson"),
			i18n("Register groups and formatting"),
			"danielk@cat.nyu.edu");
    aboutData.addCredit(i18n("David Edmundson"),
			i18n("KDE4 porting"),
			"david@davidedmundson.co.uk");
    KAboutData::setApplicationData(aboutData);

    /* take component name and org. name from KAboutData */
    app.setApplicationName(aboutData.componentName());
    app.setApplicationDisplayName(aboutData.displayName());
    app.setOrganizationDomain(aboutData.organizationDomain());
    app.setApplicationVersion(aboutData.version());

    QApplication::setWindowIcon(QIcon::fromTheme(QLatin1String("kdbg")));

    QCommandLineParser parser;
    aboutData.setupCommandLine(&parser);
    parser.setApplicationDescription(aboutData.shortDescription());
    parser.addHelpOption();
    parser.addVersionOption();

    auto opt = [&](const char* opt, QString desc, const char* arg) {
	parser.addOption(QCommandLineOption(QStringList() << QLatin1String(opt), desc, QLatin1String(arg)));
    };
    auto opt0 = [&](const char* opt, QString desc) {
	parser.addOption(QCommandLineOption(QStringList() << QLatin1String(opt), desc));
    };
    opt("t", i18n("transcript of conversation with the debugger"), "file");
    opt("r", i18n("remote debugging via <device>"), "device");
    opt("l", i18n("specify language: C, XSLT"), "language");
    opt0("x", i18n("use language XSLT (deprecated)"));
    opt("a", i18n("specify arguments of debugged executable"), "args");
    opt("p", i18n("specify PID of process to debug"), "pid");
    parser.addPositionalArgument(QLatin1String("[program]"), i18n("path of executable to debug"));
    parser.addPositionalArgument(QLatin1String("[core]"), i18n("a core file to use"));

    parser.process(app);

    /* process standard options */
    aboutData.processCommandLine(&parser);

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

    QString transcript = parser.value("t");
    QString remote = parser.value("r");
    if (!remote.isEmpty())
	debugger->setRemoteDevice(remote);

    QString lang = parser.value("l");

    // deprecated option; overrides -l
    if (parser.isSet("x")){
         /* run in xsldbg mode  */
         lang = "xslt";
    }

    // check environment variable for transcript file name
    if (transcript.isEmpty()) {
	transcript = getenv("KDBG_TRANSCRIPT");
    }
    debugger->setTranscript(transcript);

    QString pid = parser.value("p");
    QString programArgs = parser.value("a");
    QStringList posArgs = parser.positionalArguments();

    if (!restored && posArgs.count() > 0) {
	// attach to process?
	if (!pid.isEmpty()) {
	    TRACE("pid: " + pid);
	    debugger->setAttachPid(pid);
	}
	// check for core file; use it only if we're not attaching to a process
	else if (posArgs.count() > 1 && pid.isEmpty()) {
	    debugger->setCoreFile(posArgs[1]);
	}
	if (!debugger->debugProgram(posArgs[0], lang)) {
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
