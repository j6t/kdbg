// $Id$

#include <kdebug.h>
#include <assert.h>

#ifdef ASSERT
#undef ASSERT
#endif

#ifdef NDEBUG
# define ASSERT(x) ((void)0)
#else
# define ASSERT(x) ((x) ? void(0) : void(kdDebug() << \
					(QString("assertion failed: ") + #x).ascii() << "\n"))
#endif

#ifdef WANT_TRACE_OUTPUT
# define TRACE(x) (kdDebug() << (const char*)(x) << "\n")
#else
# define TRACE(x) ((void)0)
#endif
