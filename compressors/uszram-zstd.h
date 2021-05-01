#ifndef USZRAM_ZSTD_H
#define USZRAM_ZSTD_H


#include <zstd.h>

#include "compr-with-meta.h"


static inline size_type compress(const char src[static PAGE_SIZE],
				 char dest[static MAX_NON_HUGE])
{
	const size_t ret = ZSTD_compress(dest, MAX_NON_HUGE, src, PAGE_SIZE, 1);
	return ZSTD_isError(ret) ? 0 : ret;
}

static inline int decompress(const struct page *pg, size_type bytes,
			     char dest[static PAGE_SIZE])
{
	(void)bytes;
	const size_t ret = ZSTD_decompress(dest, PAGE_SIZE, pg->data,
					   get_size(pg));
	return ZSTD_isError(ret) ? -1 : 0;
}


#endif // USZRAM_ZSTD_H
