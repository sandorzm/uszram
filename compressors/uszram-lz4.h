#ifndef USZRAM_LZ4_H
#define USZRAM_LZ4_H

#include <lz4.h>

#include "compr-with-meta.h"


static size_type compress(const char src[static USZRAM_PAGE_SIZE],
			  char dest[static MAX_NON_HUGE])
{
	return LZ4_compress_default(src, dest, USZRAM_PAGE_SIZE, MAX_NON_HUGE);
}

static void write_compressed(struct page *pg, const char *src, size_type bytes)
{
	memcpy(pg->data, src, bytes);
	pg->compr_data.size = bytes;
}

static int decompress(struct page *pg, char *dest, size_type bytes)
{
	return LZ4_decompress_safe_partial(pg->data, dest, get_size(pg), bytes,
					   USZRAM_PAGE_SIZE);
}

static int maybe_recompress(struct page *pg, int blocks, char *dest)
{
	unsigned char mask = (1 << 6) - 1;
	unsigned char updates = (pg->compr_data.size & mask) + blocks;
	if (updates >= 64) {
		pg->compr_data.size &= ~mask;
		return compress(get_raw(pg), dest);
	}
	++pg->compr_data.size;
	memcpy(dest, get_raw(pg), USZRAM_PAGE_SIZE); // Unnecessary & undesirable
	return USZRAM_PAGE_SIZE;
}

static int read_mod_write(struct page *pg,
			  const char new_data[static USZRAM_BLOCK_SIZE],
			  size_type offset, size_type blocks,
			  char dest[static USZRAM_PAGE_SIZE])
{
	if (!is_huge(pg)) {
		char raw_pg[USZRAM_PAGE_SIZE];
		decompress(pg, raw_pg, USZRAM_PAGE_SIZE);
		memcpy(raw_pg + offset, new_data, blocks * USZRAM_BLOCK_SIZE);
		return compress(raw_pg, dest);
	}
	memcpy(get_raw(pg) + offset, new_data, blocks * USZRAM_BLOCK_SIZE);
	return maybe_recompress(pg, blocks, dest);
}


#endif // USZRAM_LZ4_H
