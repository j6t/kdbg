// test lambdas and other function names

#include <algorithm>
#include <initializer_list>
#include <iostream>
#include <vector>

template<typename T>
struct X
{
	X() = default;
	X(std::initializer_list<T> l) : m_v(l)
	{
		for (auto i = l.begin(); i != l.end(); ++i)
		{
			std::cout << *i << std::endl;
		}
	}
	void outmult(int factor)
	{
		auto f = [&factor](const T& a) {
			std::cout << (a*factor) << std::endl;
		};
		for_each(m_v.begin(), m_v.end(), f);
	}
	std::vector<T> m_v;

	int refqual() const &
	{
		return 3;
	}
	int refqual() &&
	{
		return 4;
	}
};

template<typename T>
auto anyoperator(const T& arg) -> T	// looks like operator< in the parser
{
	return arg;
}

int main()
{
	using fnptr = int(const int&);
	fnptr* fn = &anyoperator;	// not the last variable

	X<unsigned> x{ 1, 1, 2, 3,  5, 8 };
	x.outmult(3);

	std::cout << x.refqual() << std::endl;
	std::cout << X<int>().refqual() << std::endl;
	std::cout << fn(42) << " fn: " << fn << std::endl;
}
