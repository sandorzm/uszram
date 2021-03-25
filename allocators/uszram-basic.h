#ifndef USZRAM_BASIC_H
#define USZRAM_BASIC_H


#include "../alloc-api.h"


static void maybe_reallocate(struct page *pg, size_type old_size,
			     size_type new_size)
{
	if (old_size == new_size)
		return;
	if (new_size == 0) {
		free(pg->data);
	} else if (new_size < old_size) {
		pg->data = realloc(pg->data, new_size);
	} else {
		free(pg->data);
		pg->data = malloc(new_size);
	}
}


#endif // USZRAM_BASIC_H
