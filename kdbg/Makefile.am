## $Id$

# set the include path for X, qt and KDE
INCLUDES= $(all_includes)

# claim, which subdirectories you want to install
SUBDIRS = doc pics typetables testprogs

####### This part is very kdbg specific
# you can add here more. This one gets installed 
bin_PROGRAMS = 	kdbg

# Which sources should be compiled for kdbg.
kdbg_SOURCES = \
	pgmargs.cpp \
	procattach.cpp \
	procattachbase.ui \
	tableview.cpp \
	ktreeview.cpp \
	textvw.cpp \
	debugger.cpp \
	programconfig.cpp \
	dbgdriver.cpp \
	gdbdriver.cpp \
	xsldbgdriver.cpp \
	brkpt.cpp \
	exprwnd.cpp \
	regwnd.cpp \
	memwindow.cpp \
	threadlist.cpp \
	sourcewnd.cpp \
	winstack.cpp \
	ttywnd.cpp \
	typetable.cpp \
	prefdebugger.cpp \
	prefmisc.cpp \
	pgmsettings.cpp \
	mainwndbase.cpp \
	dbgmainwnd.cpp \
	main.cpp

BUILTSOURCES = \
	procattachbase.cpp

# the library search path
kdbg_LDFLAGS = \
	$(all_libraries) $(KDE_RPATH)

# the libraries to link against.
if BUILD_FOR_KDE3
MYKDELIBS=$(LIB_KIO)
else
MYKDELIBS=$(LIB_KFILE)
endif

kdbg_LDADD = $(MYKDELIBS)

# this option you can leave out. Just, if you use "make dist", you need it
noinst_HEADERS = \
	pgmargs.h \
	procattach.h \
	procattachbase.h \
	tableview.h \
	textvw.h \
	valarray.h \
	debugger.h \
	programconfig.h \
	dbgdriver.h \
	gdbdriver.h \
	xsldbgdriver.h \
	brkpt.h \
	exprwnd.h \
	regwnd.h \
	memwindow.h \
	threadlist.h \
	ktreeview.h \
	sourcewnd.h \
	winstack.h \
	ttywnd.h \
	typetable.h \
	prefdebugger.h \
	prefmisc.h \
	pgmsettings.h \
	mainwndbase.h \
	dbgmainwnd.h \
	envvar.h \
	commandids.h \
	mydebug.h

kdbg_METASOURCES = AUTO

# if you "make clean", this files get removed. If you want to remove
# them while "make distclean", use DISTCLEANFILES
CLEANFILES = $(kdbg_METASOURCES)

xdg_apps_DATA = kdbg.desktop

rcdir = $(kde_datadir)/kdbg
rc_DATA = kdbgui.rc

# Make messages.po and move it to $(top_srcdir)/po. "make merge" there.
# the -x is for skipping messages already translated in kdelibs
messages: rc.cpp
	LIST=`find . -name \*.h -o -name \*.hh -o -name \*.H -o -name \*.hxx -o -name \*.hpp -o -name \*.cpp -o -name \*.cc -o -name \*.cxx -o -name \*.ecpp -o -name \*.C`; \
	if test -n "$$LIST"; then \
	  $(XGETTEXT) -C -ki18n -kI18N_NOOP -x $(includedir)/kde.pot \
		$$LIST && mv messages.po $(top_srcdir)/po/kdbg.pot; \
	fi

EXTRA_DIST = kdbg.desktop
