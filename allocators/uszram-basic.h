#ifndef USZRAM_BASIC_H
#define USZRAM_BASIC_H


#include "../alloc-api.h"


static void maybe_reallocate(struct page *pg, int old_size, int new_size)
{
	if (pg->data == NULL || old_size != new_size) {
		free(pg->data);
		pg->data = calloc(new_size, 1);
	}
}


#endif // USZRAM_BASIC_H
