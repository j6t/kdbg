#1########################################################!@#$
####   Makefile.am generated automatically by KDEStudio   ####
#### WARNING! All changes made in this part will be lost! ####
##############################################################

noinst_LIBRARIES = libdock_library.a



INCLUDES = $(all_includes)\
		-I$(top_srcdir)/STabCtl

libdock_library_a_SOURCES = \
		dockmainwindow.cpp\
		dockmanager.cpp\
		dockmovemanager.cpp\
		docksplitter.cpp\
		knewpanner.cpp \
		mocs.cpp

EXTRA_DIST = \
		close.xpm\
		dock_close_bottom.xpm\
		dock_close_left.xpm\
		dock_close_right.xpm\
		dock_close_top.xpm\
		dockmainwindow.cpp\
		dockmainwindow.h\
		dockmanager.cpp\
		dockmanager.h\
		dockmovemanager.cpp\
		dockmovemanager.h\
		docksplitter.cpp\
		docksplitter.h\
		notclose.xpm\
		ChangeLog\
		knewpanner.cpp\
		knewpanner.h
##############################################################
####                  END KDEStudio part                  ####
#2########################################################!@#$

mocs.cpp: dockmainwindow.moc dockmanager.moc \
	dockmovemanager.moc docksplitter.moc \
	knewpanner.moc
dockmainwindow.moc: $(srcdir)/dockmainwindow.h
	$(MOC) $(srcdir)/dockmainwindow.h -o dockmainwindow.moc

dockmanager.moc: $(srcdir)/dockmanager.h
	$(MOC) $(srcdir)/dockmanager.h -o dockmanager.moc

dockmovemanager.moc: $(srcdir)/dockmovemanager.h
	$(MOC) $(srcdir)/dockmovemanager.h -o dockmovemanager.moc

docksplitter.moc: $(srcdir)/docksplitter.h
	$(MOC) $(srcdir)/docksplitter.h -o docksplitter.moc

knewpanner.moc: $(srcdir)/knewpanner.h
	$(MOC) $(srcdir)/knewpanner.h -o knewpanner.moc
