// $Id$

#include <kdebug.h>
#include <assert.h>

#ifdef ASSERT
#undef ASSERT
#endif

#define ASSERT(x) KASSERT3((x), KDEBUG_INFO, 0, "assertion failed: %s in %s:%d", #x, __FILE__,__LINE__)
#ifdef WANT_TRACE_OUTPUT
# ifndef KDEBUG
#  define KDEBUG(Level, Area, String) kdebug((Level), (Area), (String));
# endif
#define TRACE(x) KDEBUG(KDEBUG_INFO,0,(x))
#else
#define TRACE(x) ((void)0)
#endif

// KDE 2 compatibility; placed here because this file is included everywhere
#if QT_VERSION >= 200
#define getCaption caption
#define getConfig config
#endif
