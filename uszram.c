#include <stdlib.h>
#include <string.h>
#include <threads.h>

#include <lz4.h>

#include "uszram.h"


#define BLK_PER_PG  (1 << (USZRAM_PAGE_SHIFT - USZRAM_BLOCK_SHIFT)) // Power of 2
#define PG_ADDR_MAX (USZRAM_BLK_ADDR_MAX / BLK_PER_PG)
#define LOCK_COUNT  (PG_ADDR_MAX / USZRAM_PG_PER_LOCK + 1)

/* The lower FLAG_SHIFT bits of pgtbl[page_no].flags are for compressed size;
 * the higher bits are for zram_pageflags. From zram source: "zram is mainly
 * used for memory efficiency so we want to keep memory footprint small so we
 * can squeeze size and flags into a field."
 */
#define FLAG_SHIFT 28
#define GET_SIZE(pg) (pg->flags & (((uint_least32_t)1 << FLAG_SHIFT) - 1))

// Flags for uszram pages (pgtbl[page_no].flags)
enum uszram_pgflags {
	HUGE_PAGE = FLAG_SHIFT,	// Incompressible page
};


struct uszram_page {
	char *data;
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
	struct uszram_page pgtbl[PG_ADDR_MAX + 1];
	mtx_t locks[LOCK_COUNT];
	struct uszram_stats stats;
} uszram;

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

	// TODO Lock needed? Should uszram_page fields be _Atomic?
	if (!pg->data) {
		memset(data, 0, USZRAM_BLOCK_SIZE);
		return 0;
	}

	uint_least32_t lock_addr = pg_addr / USZRAM_PG_PER_LOCK;
	int byte_offset = blk_addr % BLK_PER_PG * USZRAM_BLOCK_SIZE;

	// Lock uszram.locks[lock_addr] as reader
	if (pg->flags & ((uint_least32_t)1 << HUGE_PAGE)) {
		memcpy(data, pg->data + byte_offset, USZRAM_BLOCK_SIZE);
		// Unlock uszram.locks[lock_addr] as reader
	} else {
		char pg_buf[USZRAM_PAGE_SIZE];
		int ret = LZ4_decompress_safe_partial(
			pg->data,
			pg_buf,
			GET_SIZE(pg),
			byte_offset + USZRAM_BLOCK_SIZE,
			USZRAM_PAGE_SIZE
		);
		// Unlock uszram.locks[lock_addr] as reader
		if (ret < 0)
			return ret;
		memcpy(data, pg_buf + byte_offset, USZRAM_BLOCK_SIZE);
	}
	return 0;
}

int uszram_read_pg(uint_least32_t pg_addr, char data[static USZRAM_PAGE_SIZE])
{
	if (pg_addr > PG_ADDR_MAX)
		return -1;

	struct uszram_page *pg = uszram.pgtbl + pg_addr;

	// TODO Lock needed? Should uszram_page fields be _Atomic?
	if (!pg->data) {
		memset(data, 0, USZRAM_PAGE_SIZE);
		return 0;
	}

	unsigned long lock_addr = pg_addr / USZRAM_PG_PER_LOCK;

	// Lock uszram.locks[lock_addr] as reader
	if (pg->flags & ((uint_least32_t)1 << HUGE_PAGE)) {
		memcpy(data, pg->data, USZRAM_PAGE_SIZE);
		// Unlock uszram.locks[lock_addr] as reader
	} else {
		int ret = LZ4_decompress_safe(pg->data, data, GET_SIZE(pg),
					      USZRAM_PAGE_SIZE);
		// Unlock uszram.locks[lock_addr] as reader
		if (ret < 0)
			return ret;
	}
	return 0;
}

int uszram_write_blk(uint_least32_t blk_addr,
		     const char data[static USZRAM_BLOCK_SIZE])
{
	if (blk_addr > USZRAM_BLK_ADDR_MAX)
		return -1;

	unsigned long pg_addr = blk_addr / BLK_PER_PG;
	unsigned long byte_offset
		= blk_addr % BLK_PER_PG * USZRAM_BLOCK_SIZE;
	char pg_buf[USZRAM_PAGE_SIZE];

	int ret = uszram_read_pg(pg_addr, pg_buf);
	if (ret < 0)
		return ret;
	memcpy(pg_buf + byte_offset, data, USZRAM_BLOCK_SIZE);
	return uszram_write_pg(pg_addr, pg_buf);
}

int uszram_write_pg(uint_least32_t pg_addr,
		    const char data[static USZRAM_PAGE_SIZE])
{
	if (pg_addr > PG_ADDR_MAX)
		return -1;

	struct uszram_page *pg = uszram.pgtbl + pg_addr;
	int size = GET_SIZE(pg);
	unsigned long lock_addr = pg_addr / USZRAM_PG_PER_LOCK;
	char pg_buf[USZRAM_PAGE_SIZE];
	const char *data_src = pg_buf;

	int new_size = LZ4_compress_default(data, pg_buf, USZRAM_PAGE_SIZE,
					    USZRAM_PAGE_SIZE);
	// Lock uszram.locks[lock_addr] as writer
	if (new_size) {
		pg->flags = new_size;
	} else {
		pg->flags |= (uint_least32_t)1 << HUGE_PAGE;
		new_size = USZRAM_PAGE_SIZE;
		data_src = data;
	}
	if (!(pg->data && new_size == size)) {
		free(pg->data);
		pg->data = calloc(new_size, 1);
	}
	memcpy(pg->data, data_src, new_size);
	// Unlock uszram.locks[lock_addr] as writer
	return new_size;
}
