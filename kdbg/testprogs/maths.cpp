// do some floatingpoint computations
#include <math.h>
#include <stdio.h>

double deg2rad(double a)
{
	double result;
	result = a * 3.141592653589793 / 180.0;
	return result;
}

#define pi  3.1415926535897932384626433832795028841971693993750

void longdouble(long double ld)
{
	long double x = 1000000.0 * ld*ld*pi;
	printf("long double: %Lf\n", x);
}

int main(int argc, char** argv)
{
	double a = 17.4;
	double b = deg2rad(a);
	double sine = sin(b);

	printf("angle=%f degrees (%f radians), sine is %f\n",
		a, b, sine);

	longdouble(17.0);
}
