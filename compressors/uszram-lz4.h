#ifndef USZRAM_LZ4_H
#define USZRAM_LZ4_H


#include <lz4.h>

#include "compr-with-meta.h"


static inline size_type compress(const char src[static PAGE_SIZE],
				 char dest[static MAX_NON_HUGE])
{
	return LZ4_compress_default(src, dest, PAGE_SIZE, MAX_NON_HUGE);
}

static inline int decompress(const struct page *pg, size_type bytes,
			     char dest[static PAGE_SIZE])
{
	const int ret = LZ4_decompress_safe_partial(
		pg->data, dest, get_size(pg), bytes, PAGE_SIZE);
	return ret < 0 ? ret : 0;
}


#endif // USZRAM_LZ4_H
