#include <stdio.h>


// a function that has args but no locals

static int nolocals(int argc, char** argv)
{
	printf("argc=%d, argv[0]=%s\n", argc, argv[0]);
}


// a function that has no args but locals

static int noargs()
{
	int c = 1;
	char* pgm[] = { "foo", 0 };
	nolocals(c, pgm);
}


int main(int argc, char** argv)
{
	noargs();
	nolocals(argc, argv);
}
