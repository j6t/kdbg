#include <string>
#include <vector>
#include <list>
#include <map>
#include <iostream>
#include <sstream>
#include <algorithm>
#include <iterator>

template<typename T>
struct V : std::vector<T>
{
	V(const T& v) : std::vector<T>(10, v) {}
	void anotherone(const T& v)
	{
		push_back(v);
	}
};

template<typename C, typename T>
void test_container(C& v, T x)
{
	v.push_back(x);
};

void test_sstream(std::basic_ostringstream<char>& str)
{
	str << "Example:\n ";
}

int main()
{
	std::string s = "abc";
	V<std::string> v(s);
	test_container(v, "xyz");
	v.anotherone("ABC");

	std::vector<bool> vb;
	vb.push_back(true);
	test_container(vb, false);
	vb[0] = vb[1];
	std::cout << vb.size() << std::endl;

	std::list<int> l;
	l.push_front(-88);
	test_container(l, 42);
	std::cout << l.size() << std::endl;

	std::map<std::string,double> m;
	m["example"] = 47.11;
	m.insert(std::make_pair("kdbg", 3.14));
	std::cout << m.size() << std::endl;

	std::ostringstream dump;
	test_sstream(dump);
	std::copy(v.begin(), v.end(), std::ostream_iterator<std::string>(dump, "\n "));
	std::cout << dump.str() << std::endl;
}
