## $Id$

# set the include path for X, qt and KDE
INCLUDES= $(all_includes)
# always compile with debugging info switched on
CXXFLAGS = -g

if BUILDTESTPROGS
PROGS = testfile
else
PROGS =
endif

noinst_PROGRAMS = $(PROGS)

testfile_SOURCES = testfile.cpp
testfile_LDFLAGS = -g $(all_libraries) $(KDE_RPATH)
testfile_LDADD = -lqt -lXext -lX11 $(LIBSOCKET)
