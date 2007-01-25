#include <iostream>

void foo()
{
	struct Whiz
	{
		Whiz()
		{
			std::cerr << __PRETTY_FUNCTION__ << std::endl;
		}
		void inner(Whiz*)
		{
			struct Inner
			{
				void bar()
				{
					std::cerr << __PRETTY_FUNCTION__ << std::endl;
				}
			};
			Inner z;
			z.bar();
		}
	};
	Whiz w;
	w.inner(&w);
}

int main()
{
	foo();
}
