## $Id$

# set the include path for X, qt and KDE
INCLUDES= $(all_includes)
# always compile with debugging info switched on
CXXFLAGS = -g

if BUILDTESTPROGS
PROGS = testfile locals maths repeats std
else
PROGS =
endif

noinst_PROGRAMS = $(PROGS)

testfile_SOURCES = testfile.cpp
testfile_LDFLAGS = -g $(all_libraries) $(KDE_RPATH)
testfile_LDADD = $(LIB_QT)

locals_SOURCES = locals.cpp
locals_LDFLAGS = -g

maths_SOURCES = maths.cpp
maths_LDFLAGS = -g

repeats_SOURCES = repeats.cpp
repeats_LDFLAGS = -g $(all_libraries) $(KDE_RPATH)
repeats_LDADD = $(LIB_QT)

std_SOURCES = std.cpp
std_LDFLAGS = -g
