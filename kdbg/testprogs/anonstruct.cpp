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

struct OnlyUnions {
	union {
		struct {
			int count;
			double vals[24];
		} content;
		unsigned size[40];
		double alignment;
	};
	union {
		void (*callback)(int);
		struct {
			T* clo;
			int (T::*memcb)();
		};
	};
	struct {
		struct {
			int used;
			double avail;
			double* data;
		} member;
	};
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
	OnlyUnions only = {};

	a = 'X';
	b = t.TestPopup();
	std::fprintf(stderr, "%zu, %zu, b=%d\n", sizeof(mutex), sizeof(t), b);

	std::strcpy(u.shortname, "short!");
	u.third = 3.14;
	std::fprintf(stderr, "u.shortname=%s\n", u.shortname);

	only.content.count = 14;
	only.content.vals[10] = 12345;
	only.clo = &t;
	only.memcb = &T::TestPopup;
	std::fprintf(stderr, "only.t=%p, indir=%d\n", only.clo, (only.clo->*only.memcb)());

	// The check whether a container with only unnamed structured
	// values is an array or a struct with only anonymous members
	// has to look up names in the contained structured values.
	// This check must not trip over an array of empty structs.
	struct Empty {};
	Empty notEmpty[5];
	std::fprintf(stderr, "notEmpty=%p, size=%zu\n", notEmpty, sizeof(notEmpty));
}
