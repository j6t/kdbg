CXXFLAGS=-O0 -g

QT3 = /usr/lib64/qt3
QT3INC = -I$(QT3)/include
QT3LIB = $(QT3)/lib64/libqt-mt.so

PROGS = \
	anonstruct \
	locals \
	maths \
	nestedclass \
	qt \
	repeats \
	std \
	templates \
	testfile \
	widechar

all: $(PROGS)

anonstruct: anonstruct.cpp
	g++ -o $@ $(CXXFLAGS) $^

locals: locals.cpp
	g++ -o $@ $(CXXFLAGS) $^

maths: maths.cpp
	g++ -o $@ $(CXXFLAGS) $^

nestedclass: nestedclass.cpp
	g++ -o $@ $(CXXFLAGS) $^

qt: qt.cpp
	g++ -o $@ $(CXXFLAGS) $^ $(QT3INC) $(QT3LIB)

repeats: repeats.cpp
	g++ -o $@ $(CXXFLAGS) $^ $(QT3INC) $(QT3LIB)

std: std.cpp
	g++ -o $@ $(CXXFLAGS) $^

templates: templates.cpp
	g++ -o $@ $(CXXFLAGS) $^

testfile: testfile.cpp
	g++ -o $@ $(CXXFLAGS) $^ $(QT3INC) $(QT3LIB)

widechar: widechar.cpp
	g++ -o $@ $(CXXFLAGS) $^
