#ifndef USZRAM_BASIC_H
#define USZRAM_BASIC_H


#include <stdlib.h>

#include "../alloc-api.h"
#include "../uszram-page.h"


static int maybe_reallocate(struct page *pg, size_type old_size,
			    size_type new_size)
{
	if (old_size == new_size)
		return 0;
	if (new_size == 0) {
		free(pg->data);
		pg->data = NULL;
	} else if (new_size < old_size) {
		pg->data = realloc(pg->data, new_size);
	} else {
		free(pg->data);
		pg->data = malloc(new_size);
	}
	return new_size - old_size;
}


#endif // USZRAM_BASIC_H
