#ifndef ALLOC_API_H
#define ALLOC_API_H


#ifdef USZRAM_JEMALLOC
#  include <jemalloc/jemalloc.h>
#else
#  include <stdlib.h>
#endif

#include "uszram-page.h"


/* maybe_reallocate() allocates, reallocates, or deallocates pg->data to hold
 * new_size bytes of new data, where it previously held old_size bytes. If
 * pg->data is NULL, old_size must be zero.
 *
 * If new_size is zero, deallocates pg->data but nothing else reachable from it
 * that may have been allocated by the compressor.
 */
static void maybe_reallocate(struct page *pg, size_type old_size,
			     size_type new_size);


#endif // ALLOC_API_H
