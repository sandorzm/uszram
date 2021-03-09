#include <stdlib.h>
#include <string.h>
#include <threads.h>

#include <lz4.h>

#include "uszram.h"


#define BLK_PER_PG    (1 << (USZRAM_PAGE_SHIFT - USZRAM_BLOCK_SHIFT))
#define PG_ADDR_MAX   (USZRAM_BLK_ADDR_MAX / BLK_PER_PG)
#define LOCK_ADDR_MAX (PG_ADDR_MAX / USZRAM_PG_PER_LOCK)

/* The lower FLAG_SHIFT bits of pgtbl[page_no].flags are for compressed size;
 * the higher bits are for zram_pageflags. From zram source: "zram is mainly
 * used for memory efficiency so we want to keep memory footprint small so we
 * can squeeze size and flags into a field."
 */
#define FLAG_SHIFT 28
#define GET_PG_SIZE(pg) ((int)(pg->flags		\
			       & (((uint_least32_t)1 << FLAG_SHIFT) - 1)))

// Flags for uszram pages (pgtbl[page_no].flags)
enum uszram_pgflags {
	HUGE_PAGE = FLAG_SHIFT,	// Incompressible page
};


struct uszram_page {
	char *_Atomic data;
	uint_least32_t flags;
};

struct uszram_stats {
	uint_least64_t compr_data_size;		// compressed size of pages stored
	uint_least64_t num_reads;		// failed + successful
	uint_least64_t num_writes;		// --do--
	uint_least64_t failed_reads;		// can happen when memory is too low
	uint_least64_t failed_writes;		// can happen when memory is too low
	uint_least64_t huge_pages;		// no. of huge pages
	uint_least64_t huge_pages_since;	// no. of huge pages since zram set up
	uint_least64_t pages_stored;		// no. of pages currently stored
	uint_least64_t max_used_pages;		// no. of maximum pages stored
};

static struct uszram {
	struct uszram_page pgtbl[(int_least64_t)PG_ADDR_MAX + 1];
	mtx_t locks[(int_least64_t)LOCK_ADDR_MAX + 1];
	struct uszram_stats stats;
} uszram;

inline static int write_no_comp(struct uszram_page *pg, const char *raw_pg,
				const char *compr_pg, int compr_size)
{
	if (!compr_size) {
		compr_size = USZRAM_PAGE_SIZE;
		if (pg->flags & ((uint_least32_t)1 << HUGE_PAGE))
			goto out;
		pg->flags |= (uint_least32_t)1 << HUGE_PAGE;
		compr_pg = raw_pg;
	} else {
		if (!(pg->flags & ((uint_least32_t)1 << HUGE_PAGE))
		    && compr_size == GET_PG_SIZE(pg))
			goto out_cpy;
		pg->flags = compr_size;
	}
	free(pg->data);
	pg->data = calloc(compr_size, 1);
out_cpy:
	memcpy(pg->data, compr_pg, compr_size);
out:
	return compr_size;
}

inline static int change_blk_write(struct uszram_page *pg, char *raw_pg,
				   int offset, const char *new_blk)
{
	char compr_pg[USZRAM_PAGE_SIZE];
	memcpy(raw_pg + offset, new_blk, USZRAM_BLOCK_SIZE);
	int new_size = LZ4_compress_default(raw_pg, compr_pg,
					    USZRAM_PAGE_SIZE,
					    USZRAM_PAGE_SIZE);
	return write_no_comp(pg, raw_pg, compr_pg, new_size);
}

int uszram_init(void)
{
	return 0;
}

int uszram_read_blk(uint_least32_t blk_addr, char data[static USZRAM_BLOCK_SIZE])
{
	if (blk_addr > USZRAM_BLK_ADDR_MAX)
		return -1;

	uint_least32_t pg_addr = blk_addr / BLK_PER_PG;
	struct uszram_page *pg = uszram.pgtbl + pg_addr;

	if (!pg->data) {
		memset(data, 0, USZRAM_BLOCK_SIZE);
		return 0;
	}

	uint_least32_t lock_addr = pg_addr / USZRAM_PG_PER_LOCK;
	int offset = blk_addr % BLK_PER_PG * USZRAM_BLOCK_SIZE;
	int ret = 0;

	// Lock uszram.locks[lock_addr] as reader
	if (pg->flags & ((uint_least32_t)1 << HUGE_PAGE)) {
		memcpy(data, pg->data + offset, USZRAM_BLOCK_SIZE);
	} else {
		char raw_pg[USZRAM_PAGE_SIZE];
		ret = LZ4_decompress_safe_partial(
			pg->data,
			raw_pg,
			GET_PG_SIZE(pg),
			offset + USZRAM_BLOCK_SIZE,
			USZRAM_PAGE_SIZE
		);
		if (ret < 0)
			goto out;
		memcpy(data, raw_pg + offset, USZRAM_BLOCK_SIZE);
	}
out:
	// Unlock uszram.locks[lock_addr] as reader
	return ret;
}

int uszram_read_pg(uint_least32_t pg_addr, char data[static USZRAM_PAGE_SIZE])
{
	if (pg_addr > PG_ADDR_MAX)
		return -1;

	struct uszram_page *pg = uszram.pgtbl + pg_addr;

	if (!pg->data) {
		memset(data, 0, USZRAM_PAGE_SIZE);
		return 0;
	}

	uint_least32_t lock_addr = pg_addr / USZRAM_PG_PER_LOCK;
	int ret = 0;

	// Lock uszram.locks[lock_addr] as reader
	if (pg->flags & ((uint_least32_t)1 << HUGE_PAGE))
		memcpy(data, pg->data, USZRAM_PAGE_SIZE);
	else
		ret = LZ4_decompress_safe(pg->data, data, GET_PG_SIZE(pg),
					  USZRAM_PAGE_SIZE);
	// Unlock uszram.locks[lock_addr] as reader
	return ret;
}

int uszram_write_blk(uint_least32_t blk_addr,
		     const char data[static USZRAM_BLOCK_SIZE])
{
	if (blk_addr > USZRAM_BLK_ADDR_MAX)
		return -1;

	uint_least32_t pg_addr = blk_addr / BLK_PER_PG;
	struct uszram_page *pg = uszram.pgtbl + pg_addr;

	uint_least32_t lock_addr = pg_addr / USZRAM_PG_PER_LOCK;
	int offset = blk_addr % BLK_PER_PG * USZRAM_BLOCK_SIZE;
	int new_size;

	// Lock uszram.pgtbl[lock_addr] as writer
	if (pg->flags & ((uint_least32_t)1 << HUGE_PAGE)) {
		new_size = change_blk_write(pg, pg->data, offset, data);
	} else {
		char raw_pg[USZRAM_PAGE_SIZE] = {0};
		if (pg->data) {
			new_size = LZ4_decompress_safe(pg->data, raw_pg,
						       GET_PG_SIZE(pg),
						       USZRAM_PAGE_SIZE);
			if (new_size < 0)
				goto out;
		}
		new_size = change_blk_write(pg, raw_pg, offset, data);
	}
out:
	// Unlock uszram.pgtbl[lock_addr] as writer
	return new_size;
}

int uszram_write_pg(uint_least32_t pg_addr,
		    const char data[static USZRAM_PAGE_SIZE])
{
	if (pg_addr > PG_ADDR_MAX)
		return -1;

	struct uszram_page *pg = uszram.pgtbl + pg_addr;
	uint_least32_t lock_addr = pg_addr / USZRAM_PG_PER_LOCK;
	char compr_pg[USZRAM_PAGE_SIZE];

	int new_size = LZ4_compress_default(data, compr_pg, USZRAM_PAGE_SIZE,
					    USZRAM_PAGE_SIZE);
	// Lock uszram.locks[lock_addr] as writer
	new_size = write_no_comp(pg, data, compr_pg, new_size);
	// Unlock uszram.locks[lock_addr] as writer
	return new_size;
}
