#ifndef ALLOC_API_H
#define ALLOC_API_H


#ifdef USZRAM_JEMALLOC
#  include <jemalloc/jemalloc.h>
#else
#  include <stdlib.h>
#endif

#include "../uszram-private.h"


static void maybe_realloc(struct page *pg, int old_size, int new_size);


#endif // ALLOC_API_H
