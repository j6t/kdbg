/*
 * Copyright Johannes Sixt
 * This file is licensed under the GNU General Public License Version 2.
 * See the file COPYING in the toplevel directory of the source directory.
 */

#include <QDebug>
#include <assert.h>
#include "config.h"

#ifdef ASSERT
#undef ASSERT
#endif

#ifdef NDEBUG
# define ASSERT(x) do {} while (0)
#else
# define ASSERT(x) if (x) ; else void(qDebug() << "assertion failed: " #x "\n")
#endif

#ifdef WANT_TRACE_OUTPUT
# define TRACE(x) qDebug() << (x)
#else
# define TRACE(x) do {} while (0)
#endif
