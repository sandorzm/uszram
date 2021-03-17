#ifndef ALLOC_H
#define ALLOC_H


#include <stddef.h>

#ifdef USZRAM_JEMALLOC
#  include <jemalloc/jemalloc.h>
#else
#  include <stdlib.h>
#endif

#include "../uszram-private.h"


static char *maybe_realloc(char *data, int data_size, int new_size);


#endif // ALLOC_H
