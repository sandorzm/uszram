#ifndef BASIC_MALLOC_H
#define BASIC_MALLOC_H


#include "../uszram-private.h"
#include "allocators.h"


#if defined(USZRAM_MALLOC) || defined(USZRAM_JEMALLOC)

#ifdef USZRAM_JEMALLOC
#  include <jemalloc/jemalloc.h>
#else
#  include <stdlib.h>
#endif


void maybe_realloc(struct uszram_page *pg, int new_size)
{
	new_size = get_class_size(new_size);
	if (!pg->data || get_class_size(GET_PG_SIZE(pg)) != new_size) {
		free(pg->data);
		pg->data = calloc(new_size, 1);
	}
}


#endif // USZRAM_MALLOC || USZRAM_JEMALLOC


#endif // BASIC_MALLOC_H
