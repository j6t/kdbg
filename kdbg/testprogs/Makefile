CXXFLAGS=-O0 -g

QT5INC = -I/usr/include/qt5 -fPIC
QT5LIB = /usr/lib64/libQt5Core.so

PROGS = \
	anonstruct \
	helloworld \
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
	tooltips \
	widechar

all: $(PROGS)

anonstruct: anonstruct.cpp
	g++ -o $@ $(CXXFLAGS) $^

helloworld: helloworld.asm
	nasm -f elf64 -F dwarf -g $^ -o $(^:.asm=.o)
	ld -g -o $@ $(^:.asm=.o)

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
	g++ -o $@ $(CXXFLAGS) $^ $(QT5INC) $(QT5LIB)

repeats: repeats.cpp
	g++ -o $@ $(CXXFLAGS) $^ $(QT5INC) $(QT5LIB)

std: std.cpp
	g++ -o $@ $(CXXFLAGS) $^

templates: templates.cpp
	g++ -std=c++0x -o $@ $(CXXFLAGS) $^

testfile: testfile.cpp
	g++ -o $@ $(CXXFLAGS) $^ $(QT5INC) $(QT5LIB)

tooltips: tooltips.cpp
	g++ -o $@ $(CXXFLAGS) $^

widechar: widechar.cpp
	g++ -o $@ $(CXXFLAGS) $^


clean:
	$(RM) $(PROGS)
