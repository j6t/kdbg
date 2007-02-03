## $Id$

## This is the example XSL and XML files for debugging with xsldbg 
EXTRA_DIST = xsldoc.xsl xsldoc.xml

# set the include path for X, qt and KDE
INCLUDES= $(all_includes)
# always compile with debugging info switched on
AM_CXXFLAGS = -g

# Disable optimization to ensure that variables are not hidden by the
# compiler
CXXFLAGS:=$(patsubst -O%,,$(CXXFLAGS))

if BUILDTESTPROGS
PROGS = testfile locals maths repeats std templates anonstruct nestedclass widechar
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

templates_SOURCES = templates.cpp
templates_LDFLAGS = -g

anonstruct_SOURCES = anonstruct.cpp
anonstruct_LDFLAGS = -g

nestedclass_SOURCES = nestedclass.cpp
nestedclass_LDFLAGS = -g

widechar_SOURCES = widechar.cpp
widechar_LDFLAGS = -g
