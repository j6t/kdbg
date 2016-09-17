#include <iostream>
using namespace std;

template<class T>
struct Templated
{
	T val;
	Templated(T aval) : val(aval) {
		cout << __func__ << " Ctor" << endl;
	}
	~Templated() {
		cout << __func__ << " Dtor" << endl;
	}
	void PrintV() {
		cout << __func__ << " val=" << val << endl;
	}
};

struct MostDerived : Templated<int>, Templated<double>
{
	MostDerived() : Templated<int>(12), Templated<double>(3.14) {
		cout << "MostDerived Ctor" << endl;
	}
	~MostDerived() {
		cout << "MostDerived Dtor" << endl;
	}
	void PrintV() {
		Templated<int>::PrintV();
		Templated<double>::PrintV();
	}
};

int main()
{
	MostDerived bothobj;

	// test "this adjustment"
	void (Templated<int>::*pmf1)();
	void (Templated<double>::*pmf2)();
	void (MostDerived::*pmf3)();
	pmf1 = static_cast<void (Templated<int>::*)()>(&MostDerived::PrintV);
	// the following has a non-trivial "this adjustment"
	pmf2 = static_cast<void (Templated<double>::*)()>(&MostDerived::PrintV);
	pmf3 = &Templated<double>::PrintV;

	bothobj.PrintV();
}
