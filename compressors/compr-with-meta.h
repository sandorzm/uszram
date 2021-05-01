#ifndef COMPR_WITH_META_H
#define COMPR_WITH_META_H


#include <string.h>

#include "../compr-api.h"
#include "../uszram-page.h"


static inline _Bool is_huge(const struct page *pg)
{
	return pg->compr_data.size >> SIZE_SHIFT;
}

static inline size_type get_size(const struct page *pg)
{
	return is_huge(pg) ? PAGE_SIZE : pg->compr_data.size;
}

static inline size_type get_size_primary(const struct page *pg)
{
	return get_size(pg);
}

static inline size_type free_reachable(const struct page *pg)
{
	(void)pg;
	return 0;
}

static inline void write_compressed(struct page *pg, size_type bytes,
				    const char *src)
{
	if (src)
		memcpy(pg->data, src, bytes);
	pg->compr_data.size = bytes >> USZRAM_PAGE_SHIFT
			      ? 1u << SIZE_SHIFT
			      : bytes;
}

static inline _Bool needs_recompress(struct page *pg, size_type blocks)
{
	const unsigned char mask = (1 << 6) - 1;
	const size_type updates = (pg->compr_data.size & mask) + blocks;
	if (updates >= USZRAM_HUGE_WAIT)
		return 1;
	pg->compr_data.size += blocks;
	return 0;
}

static int read_helper(const struct page *pg, struct range blk,
		       const char *new_data, char raw_pg[static PAGE_SIZE])
{
	const struct range byte = {
		.offset = blk.offset * BLOCK_SIZE,
		.count  = blk.count  * BLOCK_SIZE,
	};
	const int ret = decompress(
		pg,
		byte.offset + byte.count == PAGE_SIZE ? byte.offset : PAGE_SIZE,
		raw_pg);
	if (ret)
		return ret;
	if (new_data)
		memcpy(raw_pg + byte.offset, new_data, byte.count);
	else
		memset(raw_pg + byte.offset, 0, byte.count);
	return 1;
}

static inline int read_modify(const struct page *pg, struct range blk,
			      const char *new_data, const char *orig,
			      char raw_pg[static PAGE_SIZE])
{
	(void)orig;
	return read_helper(pg, blk, new_data, raw_pg);
}

static inline int read_delete(const struct page *pg, struct range blk,
			      char raw_pg[static PAGE_SIZE])
{
	return read_helper(pg, blk, NULL, raw_pg);
}


#endif // COMPR_WITH_META_H
