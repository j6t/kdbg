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

# just set the variable
APPSDIR = $(kde_appsdir)/Applications
# make double sure that the following directories are not common locations;
# "make uninstall" removes them!
PICSDIR = $(kde_datadir)/kdbg/pics
TYPESDIR = $(kde_datadir)/kdbg/types

# set the include path for X, qt and KDE
INCLUDES= $(all_includes)

# claim, which subdirectories you want to install
SUBDIRS = doc

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
	main.cpp

# the library search path
kdbg_LDFLAGS = $(all_libraries) $(KDE_RPATH)
#kdbg_LDFLAGS = -Wl,-static $(all_libraries)

# the libraries to link against. Be aware of the order. First the libraries,
# that depend on the following ones.
kdbg_LDADD   = -lkfile -lkfm -lkdeui -lkdecore -lqt -lXext -lX11 $(LIBSOCKET)

# this option you can leave out. Just, if you use "make dist", you need it
noinst_HEADERS = \
	brkpt.h \
	pgmargs.h \
	procattach.h \
	parsevar.h \
	textvw.h \
	debugger.h \
	exprwnd.h \
	ktreeview.h \
	updateui.h \
	winstack.h \
	typetable.h \
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

# types in libraries that we understand
TYPES = \
	qt.kdbgtt \
	kdecore.kdbgtt \
	X11.kdbgtt

# if you "make clean", this files get removed. If you want to remove
# them while "make distclean", use DISTCLEANFILES
CLEANFILES = $(kdbg_METASOURCES)

EXTRA_DIST = testfile.cpp $(XPMS) $(TYPES) kdbg.xpm mini-kdbg.xpm kdbg.kdelnk

# make messages.po. Move this one to ../po/ and "make merge" in po
# the -x is for skipping messages already translated in kdelibs
messages:
	$(XGETTEXT) -C -ki18n -x $(includedir)/kde.pot $(kdbg_SOURCES) && mv messages.po ../po/kdbg.pot

# just install datas here. Use install-exec-local for scripts and etc.
# the binary itself is already installed from automake
# use mkinstalldirs, not "install -d"
# don't install a list of file. Just one file per install.
# if you have more of them, create a subdirectory with an extra Makefile 
install-data-local: 
	$(mkinstalldirs) $(APPSDIR)
	$(INSTALL_DATA) kdbg.kdelnk $(APPSDIR)
	$(mkinstalldirs) $(kde_icondir)
	$(INSTALL_DATA) kdbg.xpm $(kde_icondir)
	$(mkinstalldirs) $(kde_minidir)
	$(INSTALL_DATA) mini-kdbg.xpm $(kde_minidir)/kdbg.xpm
	$(mkinstalldirs) $(PICSDIR)
	@for i in $(XPMS); do \
		echo $(INSTALL_DATA) $$i $(PICSDIR)/$$i; \
		$(INSTALL_DATA) $$i $(PICSDIR)/$$i; \
	done
	$(mkinstalldirs) $(TYPESDIR)
	@for i in $(TYPES); do \
		echo $(INSTALL_DATA) $$i $(TYPESDIR)/$$i; \
		$(INSTALL_DATA) $$i $(TYPESDIR)/$$i; \
	done

# remove ALL you have installed in install-data-local or install-exec-local
uninstall-local:
	-rm -f $(APPSDIR)/kdbg.kdelnk
	-rm -f $(kde_icondir)/kdbg.xpm
	-rm -f $(kde_minidir)/kdbg.xpm
	-rm -rf $(PICSDIR)
	-rm -rf $(TYPESDIR)


# compile a testfile
noinst_PROGRAMS = testfile
testfile_SOURCES = testfile.cpp
testfile_LDFLAGS = $(all_libraries) $(KDE_RPATH)
testfile_LDADD = -lqt -lXext -lX11 $(LIBSOCKET)

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
