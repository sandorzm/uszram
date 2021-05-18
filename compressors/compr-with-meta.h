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

static inline int read_helper(const struct page *pg,
			      unsigned char range_count,
			      const BlkRange *ranges,
			      char raw_pg[static PAGE_SIZE],
			      const char *new_data)
{
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

static inline int read_modify(const struct page *pg,
			      unsigned char range_count,
			      const BlkRange *ranges,
			      char raw_pg[static PAGE_SIZE],
			      const char *new_data)
{
	return read_helper(pg, range_count, ranges, raw_pg, new_data);
}

static inline int read_modify_hint(const struct page *pg,
				   unsigned char range_count,
				   const BlkRange *ranges,
				   char raw_pg[static PAGE_SIZE],
				   const char *new_data,
				   const char *old_data)
{
	(void)old_data;
	return read_modify(pg, range_count, ranges, raw_pg, new_data);
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


#endif // COMPR_WITH_META_H
