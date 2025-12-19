#include <stdio.h>


// a function that has args but no locals

static void nolocals(int argc, const char** argv)
{
	printf("argc=%d, argv[0]=%s\n", argc, argv[0]);
}


// a function that has no args but locals

static void noargs()
{
	int c = 1;
	const char* pgm[] = { "foo", 0 };
	nolocals(c, pgm);

	// test hidden local variables
	{
		const char* c = "inner c";
		printf("%s\n", c);
		{
			double c = 9.81;
			for (int c = 42; c > 0; c /= 7)
			    printf("c = %d, ", c);
			printf("g = %g\n", c);
		}
	}
}


int main(int argc, const char** argv)
{
	noargs();
	nolocals(argc, argv);
}
