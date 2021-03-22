#ifndef ZAPI_ALLOC_H
#define ZAPI_ALLOC_H


#include "../compr-api.h"
// #include "zapi/src/zapi.h"


inline static _Bool is_huge(struct page *pg)
{
	return pg->compr_data.updates & 0x80;
}

inline static size_type get_size(struct page *pg)
{
	return is_huge(pg) ? USZRAM_PAGE_SIZE
			   : zapi_get_size(pg->data);
}

static size_type compress(const char src[static USZRAM_PAGE_SIZE],
			  char dest[static MAX_NON_HUGE])
{
	return zapi_generate_page(src, dest);
}

static int decompress(struct page *pg, char *dest, size_type bytes)
{
	return zapi_decompress_page(pg->data, dest);
}

static void write_compressed(struct page *pg, const char *src, size_type bytes)
{
	memcpy(pg->data, src, bytes);
}

static _Bool needs_recompress(struct page *pg, size_type blocks)
{
	unsigned char mask = (1 << 6) - 1;
	unsigned char updates = (pg->compr_data.updates & mask) + blocks;
	if (updates >= USZRAM_HUGE_WAIT) {
		pg->compr_data.updates &= ~mask;
		return 1;
	}
	++pg->compr_data.updates;
	return 0;
}

static int read_modify(struct page *pg,
		       const char new_data[static USZRAM_BLOCK_SIZE],
		       size_type offset, size_type blocks, char *raw_pg)
{
	if (is_huge(pg)) {
		memcpy(pg->data + offset, new_data, blocks * USZRAM_BLOCK_SIZE);
		return needs_recompress(pg, blocks);
	}
	return zapi_update_block(pg->data, new_data, offset, blocks, raw_pg);
}


#endif // ZAPI_ALLOC_H
