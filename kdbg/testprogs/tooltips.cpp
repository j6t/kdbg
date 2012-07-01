#include <iostream>

struct B
{
    int c;
};

struct S
{
    int i;   /* to overlap local i variable */
    int i_i;
    int i1;
    int _i;
    int j;

    static int f;

    B*  pb;
};

int S::f = 5;

int g_i = 5;

class C
{
    int m_i;

public:
    C() : m_i(0) { }

    int get_i() { return m_i; }
};

namespace inner {
    int g_i = 2;

    void test()
    {
        int g_i = 0;

        for (int i = 0; i < ::g_i; ++i)
        {
            g_i += i + inner::g_i + ::S::f;
        }

	C  c;
	C *pc = &c;

	int i1 = c.get_i();
	i1 = pc->get_i();
    }
}

int main(int /* argc */, char ** /* argv */)
{
    std::cout << "Started..." << std::endl;

    inner::test();

    S  s  = { 0, 0, 0, 0, 0, 0 };
    s.pb = new B;
    s.pb->c = 0;

    S *ps = &s;

    char a = 'a';
    const char *b = "b";
    b = &a;

    const unsigned int i_max = 0 - 1;
    for (unsigned int i = 0; i < i_max; ++i)
    {
        if (i % 1000000 == 0)
            std::cout << i << s.i << s.i_i << s.j << ps->j << std::endl;

        s.i     = i;
        s.i_i   = s.i   + 10;
        s.i1    = s.i_i + 20;
        s._i    = s.i1  + s.i_i;
        s.j     = s._i;

        s.pb->c = i + 1;

        ps->j     = ps->j + 1;
        ps->pb->c = i + 2;

        S::f += 1;

        // these constructions are still not supported
        (*ps).i = (*ps).i + 1;
        (&s)->i = (&s)->i + (&s)->i_i;
    }

    std::cout << "Finished" << std::endl;

    return 0;
}
