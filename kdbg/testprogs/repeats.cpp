// test <repeats 30 times> in arrays

#include <qstring.h>

struct Big {
    struct bog {
	short b[40];
    } a[40];
    short c[40][40];
};

static void f(int)
{
}


int main()
{
    struct Big big = {{{ 2,}}};
    big.a[0].b[39]=7;
    big.a[38].b[39]=6;

    // array of pointer to function
    void (*apf[30])(int);

    for (int i = 1; i < 29; i++)
	apf[i] = f;

    QString s[300];

    for (int i = 0; i < 300; i++)
	s[i].sprintf("String %d", i);

    s[21] = s[48];

    int many[300];
    for (int i = 0; i < 300; i++)
	    many[i] = i;

    return 0;
}
