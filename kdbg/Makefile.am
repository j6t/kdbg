## $Id$

# set the include path for X, qt and KDE
INCLUDES= $(all_includes) -I$(top_srcdir)/STabCtl -I$(top_srcdir)/DockWidget

# claim, which subdirectories you want to install
SUBDIRS = doc pics typetables testprogs

####### This part is very kdbg specific
# you can add here more. This one gets installed 
bin_PROGRAMS = 	kdbg

# Which sources should be compiled for kdbg.
kdbg_SOURCES = \
	pgmargs.cpp \
	procattach.cpp \
	tableview.cpp \
	ktreeview.cpp \
	textvw.cpp \
	debugger.cpp \
	dbgdriver.cpp \
	gdbdriver.cpp \
	xsldbgdriver.cpp \
	brkpt.cpp \
	exprwnd.cpp \
	regwnd.cpp \
	memwindow.cpp \
	threadlist.cpp \
	updateui.cpp \
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

# the library search path
kdbg_LDFLAGS = \
	$(all_libraries) $(KDE_RPATH)

# the libraries to link against.
kdbg_LDADD   = \
	$(top_builddir)/DockWidget/libdock_library.a \
	$(top_builddir)/STabCtl/libtab_library.a \
	$(LIB_KFILE)

# this option you can leave out. Just, if you use "make dist", you need it
noinst_HEADERS = \
	pgmargs.h \
	procattach.h \
	tableview.h \
	textvw.h \
	valarray.h \
	debugger.h \
	dbgdriver.h \
	gdbdriver.h \
	xsldbgdriver.h \
	brkpt.h \
	exprwnd.h \
	regwnd.h \
	memwindow.h \
	threadlist.h \
	ktreeview.h \
	updateui.h \
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

apps_DATA = kdbg.desktop
appsdir = $(kde_appsdir)/Development


# make messages.po. Move this one to ../po/ and "make merge" in po
# the -x is for skipping messages already translated in kdelibs
messages: rc.cpp
	LIST=`find . -name \*.h -o -name \*.hh -o -name \*.H -o -name \*.hxx -o -name \*.hpp -o -name \*.cpp -o -name \*.cc -o -name \*.cxx -o -name \*.ecpp -o -name \*.C`; \
	if test -n "$$LIST"; then \
	 $(XGETTEXT) $$LIST -o $(podir)/kdbg.pot; \
	fi

EXTRA_DIST = kdbg.desktop
