#include <stdio.h>
int main()
{
	register long double ld = -2.83090232717332442064e-324L;
	volatile double d;
	d = ld;
	printf("ld = %.21LG, d = %.17g\n",ld,d);
	ld = d;
	printf("ld = %.21LG, d = %.17g\n",ld,d);
}
