// test anonymous structs and unions

#include <cstdio>
#include <pthread.h>

struct T {
	pthread_mutex_t mutex;	// contains anonymous union on Linux
	struct {
		int a;
	};
	union {
		int b;
		long c;
	};
};

int main()
{
	pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
	T t;
	union {
		char a;
		int b;
	};
	a = 'X';
	std::fprintf(stderr, "%d, %d, a=%d, b=%d\n", sizeof(mutex), sizeof(t), a, b);
}
