// test anonymous structs and unions

#include <cstdio>
#include <cstring>
#include <pthread.h>

struct T {
	pthread_mutex_t mutex;	// contains anonymous union on Linux
	struct {
		int a = 1;
	};
	union {
		int b;
		long c;
	};
	int TestPopup()
	{
	    return a ? b - 15 : c + 42;
	}
};

struct UnionFirst {
	union {
		char shortname[32];
		const char* longname;
	};
	struct {
		int second;
		double third;
	};
	int regular;
};

int main()
{
	pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
	T t = {};
	union {
		char a;
		int b;
	};
	UnionFirst u = {};

	a = 'X';
	b = t.TestPopup();
	std::fprintf(stderr, "%zu, %zu, b=%d\n", sizeof(mutex), sizeof(t), b);

	std::strcpy(u.shortname, "short!");
	u.third = 3.14;
	std::fprintf(stderr, "u.shortname=%s\n", u.shortname);
}
