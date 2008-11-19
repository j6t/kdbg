#include <string>
#include <vector>
#include <iostream>

void test_vector(std::vector<std::string>& v)
{
	v.push_back("xyz");
}

int main()
{
	std::string s = "abc";
	std::vector<std::string> v(10, s);

	test_vector(v);

	std::cout << s << std::endl;
}
