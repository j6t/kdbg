## $Id$

# this 10 paths are KDE specific. Use them:
# kde_htmldir       Where your docs should go to. (contains lang subdirs)
# kde_appsdir       Where your application file (.kdelnk) should go to. 
# kde_icondir       Where your icon should go to.
# kde_datadir       Where you install application data. (Use a subdir)
# kde_locale        Where translation files should go to.(contains lang subdirs)
# kde_cgidir        Where cgi-bin executables should go to.
# kde_confdir       Where config files should go to.
# kde_mimedir       Where mimetypes should go to.
# kde_toolbardir    Where general toolbar icons should go to.
# kde_wallpaperdir  Where general wallpapers should go to.

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
	ktreeview.cpp \
	textvw.cpp \
	debugger.cpp \
	dbgdriver.cpp \
	gdbdriver.cpp \
	brkpt.cpp \
	exprwnd.cpp \
	regwnd.cpp \
	updateui.cpp \
	winstack.cpp \
	typetable.cpp \
	prefdebugger.cpp \
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
	textvw.h \
	valarray.h \
	debugger.h \
	dbgdriver.h \
	gdbdriver.h \
	brkpt.h \
	exprwnd.h \
	regwnd.h \
	ktreeview.h \
	updateui.h \
	winstack.h \
	typetable.h \
	prefdebugger.h \
	mainwndbase.h \
	dbgmainwnd.h \
	envvar.h \
	commandids.h \
	mydebug.h

# just to make sure, automake makes them
BUILTSOURCES =	\
	pgmargs.moc \
	textvw.moc \
	debugger.moc \
	dbgdriver.moc \
	gdbdriver.moc \
	brkpt.moc \
	exprwnd.moc \
	regwnd.moc \
	ktreeview.moc \
	winstack.moc \
	mainwndbase.moc \
	dbgmainwnd.moc \
	updateui.moc


# if you "make clean", this files get removed. If you want to remove
# them while "make distclean", use DISTCLEANFILES
CLEANFILES = $(kdbg_METASOURCES)


apps_DATA = kdbg.kdelnk
appsdir = $(kde_appsdir)/Development


# make messages.po. Move this one to ../po/ and "make merge" in po
# the -x is for skipping messages already translated in kdelibs
messages:
	$(XGETTEXT) -C -ki18n -x $(includedir)/kde.pot $(kdbg_SOURCES) && mv messages.po ../po/kdbg.pot


EXTRA_DIST = $(apps_DATA)

# add a dependency for every moc file to be full portable
# I've added a key binding to emacs for this. 
$(srcdir)/debugger.cpp: debugger.moc
debugger.moc: $(srcdir)/debugger.h
	$(MOC) $(srcdir)/debugger.h -o debugger.moc

$(srcdir)/dbgdriver.cpp: dbgdriver.moc
dbgdriver.moc: $(srcdir)/dbgdriver.h
	$(MOC) $(srcdir)/dbgdriver.h -o dbgdriver.moc

$(srcdir)/gdbdriver.cpp: gdbdriver.moc
gdbdriver.moc: $(srcdir)/gdbdriver.h
	$(MOC) $(srcdir)/gdbdriver.h -o gdbdriver.moc

$(srcdir)/exprwnd.cpp: exprwnd.moc
exprwnd.moc: $(srcdir)/exprwnd.h
	$(MOC) $(srcdir)/exprwnd.h -o exprwnd.moc

$(srcdir)/regwnd.cpp: regwnd.moc
regwnd.moc: $(srcdir)/regwnd.h
	$(MOC) $(srcdir)/regwnd.h -o regwnd.moc

$(srcdir)/textvw.cpp: textvw.moc
textvw.moc: $(srcdir)/textvw.h
	$(MOC) $(srcdir)/textvw.h -o textvw.moc

$(srcdir)/ktreeview.cpp: ktreeview.moc
ktreeview.moc: $(srcdir)/ktreeview.h
	$(MOC) $(srcdir)/ktreeview.h -o ktreeview.moc

$(srcdir)/winstack.cpp: winstack.moc
winstack.moc: $(srcdir)/winstack.h
	$(MOC) $(srcdir)/winstack.h -o winstack.moc

$(srcdir)/updateui.cpp: updateui.moc
updateui.moc: $(srcdir)/updateui.h
	$(MOC) $(srcdir)/updateui.h -o updateui.moc

$(srcdir)/brkpt.cpp: brkpt.moc
brkpt.moc: $(srcdir)/brkpt.h
	$(MOC) $(srcdir)/brkpt.h -o brkpt.moc

$(srcdir)/pgmargs.cpp: pgmargs.moc
pgmargs.moc: $(srcdir)/pgmargs.h
	$(MOC) $(srcdir)/pgmargs.h -o pgmargs.moc

$(srcdir)/mainwndbase.cpp: mainwndbase.moc
mainwndbase.moc: $(srcdir)/mainwndbase.h
	$(MOC) $(srcdir)/mainwndbase.h -o mainwndbase.moc

$(srcdir)/dbgmainwnd.cpp: dbgmainwnd.moc
dbgmainwnd.moc: $(srcdir)/dbgmainwnd.h
	$(MOC) $(srcdir)/dbgmainwnd.h -o dbgmainwnd.moc
