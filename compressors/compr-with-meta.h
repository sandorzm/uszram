#ifndef COMPR_WITH_META_H
#define COMPR_WITH_META_H


#include "../compr-api.h"


inline static _Bool is_huge(struct page *pg)
{
	return pg->compr_data.size & (1 << USZRAM_PAGE_SHIFT);
}

inline static size_type get_size(struct page *pg)
{
	return is_huge(pg) ? USZRAM_PAGE_SIZE : pg->compr_data.size;
}

static void write_compressed(struct page *pg, const char *src, size_type bytes)
{
	memcpy(pg->data, src, bytes);
	pg->compr_data.size = bytes;
}

static _Bool needs_recompress(struct page *pg, size_type blocks)
{
	unsigned char mask = (1 << 6) - 1;
	unsigned char updates = (pg->compr_data.size & mask) + blocks;
	if (updates >= USZRAM_HUGE_WAIT) {
		pg->compr_data.size &= ~mask;
		return 1;
	}
	++pg->compr_data.size;
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
	int ret = decompress(pg, raw_pg, USZRAM_PAGE_SIZE);
	if (ret < 0)
		return ret;
	memcpy(raw_pg + offset, new_data, blocks * USZRAM_BLOCK_SIZE);
	return 1;
}


#endif // COMPR_WITH_META_H
