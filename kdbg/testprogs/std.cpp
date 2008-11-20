#include <string>
#include <vector>
#include <iostream>

template<typename T>
struct V : std::vector<T>
{
	V(const T& v) : std::vector<T>(10, v) {}
	void anotherone(const T& v)
	{
		push_back(v);
	}
};

void test_vector(std::vector<std::string>& v)
{
	v.push_back("xyz");
}

int main()
{
	std::string s = "abc";
	V<std::string> v(s);

	test_vector(v);
	v.anotherone("ABC");

	std::cout << s << std::endl;
}
