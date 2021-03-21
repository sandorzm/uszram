#ifndef ALLOC_API_H
#define ALLOC_API_H


#ifdef USZRAM_JEMALLOC
#  include <jemalloc/jemalloc.h>
#else
#  include <stdlib.h>
#endif

#include "uszram-page.h"


/* maybe_reallocate() allocates or reallocates the buffer at pg->data to hold
 * new_size bytes of new data, where it previously held old_size bytes. old_size
 * must be zero if pg->data is NULL, and new_size can't be zero. TODO Deallocate
 * if new_size is zero?
 */
static void maybe_reallocate(struct page *pg, size_type old_size,
			     size_type new_size);


#endif // ALLOC_API_H
