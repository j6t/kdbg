// do some floatingpoint computations
#include <math.h>
#include <stdio.h>

double deg2rad(double a)
{
	double result;
	result = a * 3.141592653589793 / 180.0;
	return result;
}

int main(int argc, char** argv)
{
	double a = 17.4;
	double b = deg2rad(a);
	double sine = sin(b);

	printf("angle=%f degrees (%f radians), sine is %f\n",
		a, b, sine);
}
