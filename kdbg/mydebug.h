/*
 * Copyright Johannes Sixt
 * This file is licensed under the GNU General Public License Version 2.
 * See the file COPYING in the toplevel directory of the source directory.
 */

#include <kdebug.h>
#include <assert.h>
#include "config.h"

#ifdef ASSERT
#undef ASSERT
#endif

#ifdef NDEBUG
# define ASSERT(x) do {} while (0)
#else
# define ASSERT(x) kDebug(!(x)) << "assertion failed: " #x "\n"
#endif

#ifdef WANT_TRACE_OUTPUT
# define TRACE(x) kDebug() << (x)
#else
# define TRACE(x) do {} while (0)
#endif
