#ifndef USZRAM_LZ4_H
#define USZRAM_LZ4_H

#include <lz4.h>

#include "compr-with-meta.h"


inline static size_type compress(const char src[static USZRAM_PAGE_SIZE],
				 char dest[static MAX_NON_HUGE])
{
	return LZ4_compress_default(src, dest, USZRAM_PAGE_SIZE, MAX_NON_HUGE);
}

inline static int decompress(struct page *pg, size_type bytes,
			     char dest[static bytes])
{
	int ret = LZ4_decompress_safe_partial(pg->data, dest, get_size(pg),
					      bytes, USZRAM_PAGE_SIZE);
	return ret <= 0 ? ret : 0;
}


#endif // USZRAM_LZ4_H
