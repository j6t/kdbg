// test anonymous structs and unions

#include <cstdio>
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

int main()
{
	pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
	T t = {};
	union {
		char a;
		int b;
	};
	a = 'X';
	b = t.TestPopup();
	std::fprintf(stderr, "%zu, %zu, b=%d\n", sizeof(mutex), sizeof(t), b);
}
