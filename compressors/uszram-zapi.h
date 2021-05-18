#ifndef USZRAM_ZAPI_H
#define USZRAM_ZAPI_H


#include <string.h>

#include "../compr-api.h"
#include "../uszram-page.h"
#include "../../zapi/src/zapi.h"


static page_opts pg_options = {
	.block_sz = BLOCK_SIZE,
	.blocks = BLK_PER_PG,
};

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
	return zapi_generate_page((BYTE *)src, (BYTE *)dest,
				  &pg_options, BLK_PER_PG);
}

static inline int decompress(const struct page *pg, size_type bytes,
			     char dest[static PAGE_SIZE])
{
	return -!zapi_decompress_page((BYTE *)pg->data, (BYTE *)dest,
				      &pg_options, bytes / BLOCK_SIZE);
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

static inline int read_modify(const struct page *pg,
			      unsigned char range_count,
			      const BlkRange *ranges,
			      char raw_pg[static PAGE_SIZE],
			      const char *new_data)
{

	uint_least16_t decompr = 0;
	for (int i = 0; i < range_count; ++i)
		if (decompr < ranges[i].offset + ranges[i].count)
			decompr = ranges[i].offset + ranges[i].count;
	int ret = decompress(pg, decompr * BLOCK_SIZE, raw_pg);
	if (ret)
		return ret;

	_Bool recompress = 0;
	for (unsigned char i = 0; i < range_count; ++i) {
		if (recompress) {
			const size_type offset = ranges[i].offset * BLOCK_SIZE,
					count  = ranges[i].count  * BLOCK_SIZE;
			memcpy(raw_pg + offset, new_data, count);
			new_data += count;
			continue;
		}
		for (uint_least16_t j = 0; j < ranges[i].count; ++j) {
			ret = zapi_update_block(
				(BYTE *)new_data, (BYTE *)pg->data,
				ranges[i].offset + j, &pg_options,
				(BYTE *)raw_pg, 512);
			new_data += BLOCK_SIZE;
			if (!ret)
 				continue;
			recompress = 1;
			const size_type
				offset = (ranges[i].offset + j) * BLOCK_SIZE,
				count  = (ranges[i].count  - j) * BLOCK_SIZE;
			memcpy(raw_pg + offset, new_data, count);
			new_data += count;
		}
	}
	return recompress;
}

static inline int read_modify_hint(const struct page *pg,
				   unsigned char range_count,
				   const BlkRange *ranges,
				   char raw_pg[static PAGE_SIZE],
				   const char *new_data,
				   const char *old_data)
{
	(void)old_data;
	int ret = decompress(pg, PAGE_SIZE, raw_pg);
	if (ret)
		return ret;
	for (unsigned char i = 0; i < range_count; ++i) {
		const size_type offset = ranges[i].offset * BLOCK_SIZE,
				count  = ranges[i].count  * BLOCK_SIZE;
		memcpy(raw_pg + offset, new_data, count);
		new_data += count;
	}
	return 1;
}

static inline int read_delete(const struct page *pg,
			      unsigned char range_count,
			      const BlkRange *ranges,
			      char raw_pg[static PAGE_SIZE])
{
	int ret = decompress(pg, PAGE_SIZE, raw_pg);
	if (ret)
		return ret;
	for (unsigned char i = 0; i < range_count; ++i)
		memset(raw_pg + ranges[i].offset * BLOCK_SIZE, 0,
		       ranges[i].count * BLOCK_SIZE);
	return 1;
}


#endif // USZRAM_ZAPI_H
