#ifndef USZRAM_LZ4_H
#define USZRAM_LZ4_H

#include <lz4.h>

#include "compr-with-meta.h"


static size_type compress(const char src[static USZRAM_PAGE_SIZE],
			  char dest[static MAX_NON_HUGE])
{
	return LZ4_compress_default(src, dest, USZRAM_PAGE_SIZE, MAX_NON_HUGE);
}

static int decompress(struct page *pg, char *dest, size_type bytes)
{
	return LZ4_decompress_safe_partial(pg->data, dest, get_size(pg), bytes,
					   USZRAM_PAGE_SIZE);
}


#endif // USZRAM_LZ4_H
