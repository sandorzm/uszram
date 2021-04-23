#ifndef COMPR_WITH_META_H
#define COMPR_WITH_META_H


#include <string.h>

#include "../compr-api.h"
#include "../uszram-page.h"


inline static _Bool is_huge(struct page *pg)
{
	return pg->compr_data.size & (1u << USZRAM_PAGE_SHIFT);
}

inline static size_type get_size(struct page *pg)
{
	return is_huge(pg) ? USZRAM_PAGE_SIZE : pg->compr_data.size;
}

inline static size_type get_size_primary(struct page *pg)
{
	return get_size(pg);
}

inline static size_type free_reachable(struct page *pg)
{
	(void)pg;
	return 0;
}

inline static void write_compressed(struct page *pg, size_type bytes,
				    const char *src)
{
	if (bytes)
		memcpy(pg->data, src, bytes);
	pg->compr_data.size = bytes;
}

inline static _Bool needs_recompress(struct page *pg, size_type blocks)
{
	unsigned char mask = (1 << 6) - 1;
	size_type updates = (pg->compr_data.size & mask) + blocks;
	if (updates >= USZRAM_HUGE_WAIT) {
		pg->compr_data.size &= ~mask;
		return 1;
	}
	++pg->compr_data.size;
	return 0;
}

static int read_modify(struct page *pg, size_type offset, size_type blocks,
		       const char *new_data, char *raw_pg)
{
	offset *= USZRAM_BLOCK_SIZE;
	size_type bytes = blocks * USZRAM_BLOCK_SIZE;
	if (is_huge(pg)) {
		if (new_data == NULL)
			memset(pg->data + offset, 0, bytes);
		else
			memcpy(pg->data + offset, new_data, bytes);
		return needs_recompress(pg, blocks);
	}
	if (raw_pg == NULL)
		return 0;
	int ret = decompress(pg, USZRAM_PAGE_SIZE, raw_pg);
	if (ret < 0)
		return ret;
	if (new_data == NULL)
		memset(raw_pg + offset, 0, bytes);
	else
		memcpy(raw_pg + offset, new_data, bytes);
	return 1;
}


#endif // COMPR_WITH_META_H
