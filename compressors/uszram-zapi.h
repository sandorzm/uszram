#ifndef USZRAM_ZAPI_H
#define USZRAM_ZAPI_H


#include "../compr-api.h"
#include "../../zapi/src/zapi.h"


inline static _Bool is_huge(struct page *pg)
{
	return pg->compr_data.size & (1u << USZRAM_PAGE_SHIFT);
}

inline static size_type get_size(struct page *pg)
{
	return is_huge(pg) ? USZRAM_PAGE_SIZE
			   : zapi_page_size((BYTE *)pg->data);
}

inline static size_type get_size_primary(struct page *pg)
{
	return is_huge(pg) ? USZRAM_PAGE_SIZE : pg->compr_data.size;
}

inline static size_type free_reachable(struct page *pg)
{
	size_type old_size = zapi_page_size((BYTE *)pg->data);
	zapi_free_page((BYTE *)pg->data);
	return old_size - zapi_page_size((BYTE *)pg->data);
}

inline static size_type compress(const char src[static USZRAM_PAGE_SIZE],
				 char dest[static MAX_NON_HUGE])
{
	return zapi_generate_page(src, dest);
}

inline static int decompress(struct page *pg, size_type bytes,
			     char dest[static bytes])
{
	return zapi_decompress_page(pg->data, dest);
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
	if (new_data == NULL)
		return zapi_delete_block(pg->data, offset, blocks, raw_pg,
					 MAX_NON_HUGE);
	return zapi_update_block(pg->data, new_data, offset, blocks, raw_pg,
				 MAX_NON_HUGE);
}


#endif // USZRAM_ZAPI_H
