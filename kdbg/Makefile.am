## $Id$

# this 10 paths are KDE specific. Use them:
# kde_htmldir       Where your docs should go to. (contains lang subdirs)
# kde_appsdir       Where your application file (.kdelnk) should go to. 
# kde_icondir       Where your icon should go to.
# kde_minidir       Where your mini icon should go to.
# kde_datadir       Where you install application data. (Use a subdir)
# kde_locale        Where translation files should go to.(contains lang subdirs)
# kde_cgidir        Where cgi-bin executables should go to.
# kde_confdir       Where config files should go to.
# kde_mimedir       Where mimetypes should go to.
# kde_toolbardir    Where general toolbar icons should go to.
# kde_wallpaperdir  Where general wallpapers should go to.

# set the include path for X, qt and KDE
INCLUDES= $(all_includes)

# claim, which subdirectories you want to install
SUBDIRS = doc typetables

####### This part is very kdbg specific
# you can add here more. This one gets installed 
bin_PROGRAMS = 	kdbg

# Which sources should be compiled for kdbg.
kdbg_SOURCES = \
	brkpt.cpp \
	pgmargs.cpp \
	procattach.cpp \
	ktreeview.cpp \
	parsevar.cpp \
	textvw.cpp \
	debugger.cpp \
	exprwnd.cpp \
	updateui.cpp \
	winstack.cpp \
	typetable.cpp \
	prefdebugger.cpp \
	mainwndbase.cpp \
	dbgmainwnd.cpp \
	main.cpp

# the library search path
kdbg_LDFLAGS = $(all_libraries) $(KDE_RPATH)
#kdbg_LDFLAGS = -Wl,-static $(all_libraries)

# the libraries to link against.
kdbg_LDADD   = $(LIB_KFILE)

# this option you can leave out. Just, if you use "make dist", you need it
noinst_HEADERS = \
	brkpt.h \
	pgmargs.h \
	procattach.h \
	parsevar.h \
	textvw.h \
	valarray.h \
	debugger.h \
	exprwnd.h \
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
	exprwnd.moc \
	ktreeview.moc \
	winstack.moc \
	mainwndbase.moc \
	dbgmainwnd.moc \
	updateui.moc

# toolbar icons and other pixmaps
XPMS = \
	brkpt.xpm \
	pgmfinish.xpm \
	pgmnext.xpm \
	pgmrun.xpm \
	pgmstep.xpm \
	execopen.xpm \
	brkena.xpm \
	brkdis.xpm \
	brktmp.xpm \
	brkcond.xpm \
	pcinner.xpm \
	pcup.xpm \
	search.xpm \
	pointer.xpm


# if you "make clean", this files get removed. If you want to remove
# them while "make distclean", use DISTCLEANFILES
CLEANFILES = $(kdbg_METASOURCES)


apps_DATA = kdbg.kdelnk
appsdir = $(kde_appsdir)/Development
icon_DATA = kdbg.xpm
icondir = $(kde_icondir)
miniicon_DATA = mini-kdbg.xpm
miniicondir = $(kde_minidir)
pics_DATA = $(XPMS)
picsdir = $(kde_datadir)/kdbg/pics


# make messages.po. Move this one to ../po/ and "make merge" in po
# the -x is for skipping messages already translated in kdelibs
messages:
	$(XGETTEXT) -C -ki18n -x $(includedir)/kde.pot $(kdbg_SOURCES) && mv messages.po ../po/kdbg.pot


# compile a testfile
noinst_PROGRAMS = testfile
testfile_SOURCES = testfile.cpp
testfile_LDFLAGS = $(all_libraries) $(KDE_RPATH)
testfile_LDADD = -lqt -lXext -lX11 $(LIBSOCKET)

EXTRA_DIST = $(pics_DATA) \
	$(icon_DATA) $(miniicon_DATA) $(apps_DATA)

# add a dependency for every moc file to be full portable
# I've added a key binding to emacs for this. 
debugger.cpp: debugger.moc
debugger.moc: debugger.h
	$(MOC) debugger.h -o debugger.moc

exprwnd.cpp: exprwnd.moc
exprwnd.moc: exprwnd.h
	$(MOC) exprwnd.h -o exprwnd.moc

textvw.cpp: textvw.moc
textvw.moc: textvw.h
	$(MOC) textvw.h -o textvw.moc

ktreeview.cpp: ktreeview.moc
ktreeview.moc: ktreeview.h
	$(MOC) ktreeview.h -o ktreeview.moc

winstack.cpp: winstack.moc
winstack.moc: winstack.h
	$(MOC) winstack.h -o winstack.moc

updateui.cpp: updateui.moc
updateui.moc: updateui.h
	$(MOC) updateui.h -o updateui.moc

brkpt.cpp: brkpt.moc
brkpt.moc: brkpt.h
	$(MOC) brkpt.h -o brkpt.moc

pgmargs.cpp: pgmargs.moc
pgmargs.moc: pgmargs.h
	$(MOC) pgmargs.h -o pgmargs.moc

mainwndbase.cpp: mainwndbase.moc
mainwndbase.moc: mainwndbase.h
	$(MOC) mainwndbase.h -o mainwndbase.moc

dbgmainwnd.cpp: dbgmainwnd.moc
dbgmainwnd.moc: dbgmainwnd.h
	$(MOC) dbgmainwnd.h -o dbgmainwnd.moc
