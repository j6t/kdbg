#1########################################################!@#$
####   Makefile.am generated automatically by KDEStudio   ####
#### WARNING! All changes made in this part will be lost! ####
##############################################################

noinst_LIBRARIES = libtab_library.a



INCLUDES = $(all_includes)
DEFS = -I. -I$(srcdir) -I$(top_srcdir)
libtab_library_a_SOURCES = \
		stabbar.cpp\
		stabctl.cpp \
		mocs.cpp

EXTRA_DIST = \
		b_left.xpm\
		stabbar.cpp\
		stabbar.h\
		stabctl.cpp\
		stabctl.h\
		ChangeLog
##############################################################
####                  END KDEStudio part                  ####
#2########################################################!@#$

mocs.cpp: stabctl.moc stabbar.moc
stabctl.moc: $(srcdir)/stabctl.h
	$(MOC) $(srcdir)/stabctl.h -o stabctl.moc

stabbar.moc: $(srcdir)/stabbar.h
	$(MOC) $(srcdir)/stabbar.h -o stabbar.moc
