/*
 * Copyright Johannes Sixt
 * This file is licensed under the GNU General Public License Version 2.
 * See the file COPYING in the toplevel directory of the source directory.
 */

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
