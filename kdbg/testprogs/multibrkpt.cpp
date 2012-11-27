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
	bothobj.PrintV();
}
