#ifndef ALLOC_API_H
#define ALLOC_API_H


#include "uszram-def.h"


struct page;

/* maybe_reallocate() allocates, reallocates, or deallocates pg->data to hold
 * new_size bytes of new data, where it previously held old_size (as returned by
 * the compressor's get_size_primary()) bytes. Deallocates if new_size is zero.
 *
 * Returns the size of the new allocation minus that of the old one. This may be
 * different from new_size - old_size for some allocators.
 *
 * Affects pg->data but nothing else reachable from it that may have been
 * allocated by the compressor. For data other than pg->data itself, see the
 * compressor's free_reachable().
 */
static int maybe_reallocate(struct page *pg, size_type old_size,
			    size_type new_size);


#endif // ALLOC_API_H
