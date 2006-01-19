// This file test stack parsing capabilities of KDbg.
// Parsing function names can be quite tricky ;)
#include <iostream>
using namespace std;

struct S {
	void operator>>(int)
	{
		cout << __PRETTY_FUNCTION__ << endl;
	}
};

template<typename T>
struct templS {
	void operator>(T)
	{
		cout << __PRETTY_FUNCTION__ << endl;
	}
	void operator<(T)
	{
		cout << __PRETTY_FUNCTION__ << endl;
	}
};


namespace A {
namespace {
namespace B {
namespace {
namespace {
void g()
{
	cout << __PRETTY_FUNCTION__ << endl;
}
} // namespace
void Banong() { g(); }
} // namespace
void g() { Banong(); }
} // namespace B
void Aanong() { B::g(); }
} // namespace
void g() { Aanong(); }

void operator<<(int, S)
{
	cout << __PRETTY_FUNCTION__ << endl;
}

template<typename T>
void operator<(T, S)
{
	cout << __PRETTY_FUNCTION__ << endl;
}
} // namespace A

void operator<<(struct S&, int)
{
	cout << __PRETTY_FUNCTION__ << endl;
}

template<typename T, typename U>
void operator<<(T&, U)
{
	cout << __PRETTY_FUNCTION__ << endl;
}

void operator<(struct S&, int)
{
	cout << __PRETTY_FUNCTION__ << endl;
}

template<typename T, typename U>
void operator<(T&, U)
{
	cout << __PRETTY_FUNCTION__ << endl;
}

void f(const char* s)
{
	A::g();
	cout << s << endl;
}

template<typename T>
void indirect(T f, const char* s)
{
	f(s);
}

int main()
{
	S s1, s2;
	f("direct");
	s1 << 1;
	s1 << s2;
	s1 < 1;
	s1 < s2;

	A::operator<<(1, s1);
	A::operator<(1, s1);

	// the next lines test a templated function that accepts
	// as one of its parameters a templated function pointer
	void (*op1)(S&, S*) = operator<<;
	operator<<(op1, s2);
	void (*op2)(S&, S*) = operator<;
	operator<(op2, s2);
	indirect(f, "indirect");

	// pointer to member function
	void (S::*pm1)(int) = &S::operator>>;
	(s1.*pm1)(1);
	void (templS<int>::*pm2)(int) = &templS<int>::operator>;
	templS<int> tSi;
	(tSi.*pm2)(1);
	tSi.operator<(1);
}
