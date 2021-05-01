#ifndef USZRAM_ZAPI_H
#define USZRAM_ZAPI_H


#include <string.h>

#include "../compr-api.h"
#include "../uszram-page.h"
#include "../../zapi/src/zapi.h"


static inline _Bool is_huge(const struct page *pg)
{
	return pg->compr_data.size >> SIZE_SHIFT;
}

static inline size_type get_size(const struct page *pg)
{
	return is_huge(pg) ? PAGE_SIZE : zapi_page_size((BYTE *)pg->data);
}

static inline size_type get_size_primary(const struct page *pg)
{
	return is_huge(pg) ? PAGE_SIZE : pg->compr_data.size;
}

static inline size_type free_reachable(const struct page *pg)
{
	const size_type old_size = zapi_page_size((BYTE *)pg->data);
	zapi_free_page((BYTE *)pg->data);
	return old_size - zapi_page_size((BYTE *)pg->data);
}

static inline size_type compress(const char src[static PAGE_SIZE],
				 char dest[static MAX_NON_HUGE])
{
	return zapi_generate_page(src, dest);
}

static inline int decompress(const struct page *pg, size_type bytes,
			     char dest[static PAGE_SIZE])
{
	return zapi_decompress_page(pg->data, dest);
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

static inline int read_modify(const struct page *pg, struct range blk,
			      const char *new_data, const char *orig,
			      char raw_pg[static PAGE_SIZE])
{
	if (orig)
		return zapi_update_block_hint(pg->data, new_data,
					      blk.offset, blk.count,
					      raw_pg, orig, MAX_NON_HUGE);
	return zapi_update_block(pg->data, new_data, blk.offset, blk.count,
				 raw_pg, MAX_NON_HUGE);
}

static inline int read_delete(const struct page *pg, struct range blk,
			      char raw_pg[static PAGE_SIZE])
{
	return zapi_delete_block(pg->data, blk.offset, blk.count, raw_pg,
				 MAX_NON_HUGE);
}


#endif // USZRAM_ZAPI_H
