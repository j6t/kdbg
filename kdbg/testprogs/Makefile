CXXFLAGS=-O0 -g

QT3 = /usr/lib64/qt3
QT3INC = -I$(QT3)/include
QT3LIB = $(QT3)/lib64/libqt-mt.so

PROGS = \
	anonstruct \
	lambda \
	locals \
	longdouble \
	maths \
	multibrkpt \
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

lambda: lambda.cpp
	g++ --std=c++0x -o $@ $(CXXFLAGS) $^

locals: locals.cpp
	g++ -o $@ $(CXXFLAGS) $^

longdouble: longdouble.c
	gcc -o $@ $(CXXFLAGS) $^

maths: maths.cpp
	g++ -o $@ $(CXXFLAGS) $^

multibrkpt: multibrkpt.cpp
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


clean:
	$(RM) $(PROGS)
