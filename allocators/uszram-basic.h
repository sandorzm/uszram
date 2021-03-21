#ifndef USZRAM_BASIC_H
#define USZRAM_BASIC_H


#include "../alloc-api.h"


static void maybe_reallocate(struct page *pg, size_type old_size,
			     size_type new_size)
{
	if (old_size == new_size)
		return;
	free(pg->data);
	pg->data = malloc(new_size);
}


#endif // USZRAM_BASIC_H
