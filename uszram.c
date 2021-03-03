#include <stdlib.h>
#include <string.h>
#include <threads.h>


#include "uszram.h"


#define BLK_PER_PG (1 << (USZRAM_PAGE_SHIFT - USZRAM_BLOCK_SHIFT)) // Power of 2

/* Use a trick to correctly make PAGE_COUNT the ceiling of
 * USZRAM_BLOCK_COUNT / BLK_PER_PG; similar for LOCK_COUNT
 */
#define PAGE_COUNT (USZRAM_BLOCK_COUNT / BLK_PER_PG	\
		    + (USZRAM_BLOCK_COUNT % BLK_PER_PG != 0))
#define LOCK_COUNT (PAGE_COUNT / USZRAM_PG_PER_LOCK	\
		    + (PAGE_COUNT % USZRAM_PG_PER_LOCK != 0))

/* The lower FLAG_SHIFT bits of pgtbl[page_no].flags are for compressed size;
 * the higher bits are for zram_pageflags. From zram source: "zram is mainly
 * used for memory efficiency so we want to keep memory footprint small so we
 * can squeeze size and flags into a field."
 */
#define FLAG_SHIFT 28
#define GET_SIZE(pg) (pg->flags & ((1 << FLAG_SHIFT) - 1))

// Flags for uszram pages (pgtbl[page_no].flags)
enum uszram_pgflags {
	HUGE_PAGE = FLAG_SHIFT,	// Incompressible page
};


struct uszram_page {
	char *data;
	unsigned long flags;
};

struct uszram_stats {
	unsigned long long compr_data_size;	// compressed size of pages stored
	unsigned long long num_reads;		// failed + successful
	unsigned long long num_writes;		// --do--
	unsigned long long failed_reads;	// can happen when memory is too low
	unsigned long long failed_writes;	// can happen when memory is too low
	unsigned long long invalid_io;		// non-page-aligned I/O requests
	unsigned long long notify_free;		// no. of swap slot free notifications
	unsigned long long huge_pages;		// no. of huge pages
	unsigned long long huge_pages_since;	// no. of huge pages since zram set up
	unsigned long long pages_stored;	// no. of pages currently stored
	unsigned long long max_used_pages;	// no. of maximum pages stored
	unsigned long long writestall;		// no. of write slow paths
	unsigned long long miss_free;		// no. of missed free
};

static struct uszram {
	struct uszram_page pgtbl[PAGE_COUNT];
	mtx_t locks[LOCK_COUNT];
	struct uszram_stats stats;

	/* This is the limit on amount of *uncompressed* worth of data we can
	 * store in a disk.
	 */
	/* unsigned long long disksize;	// bytes */
	/* char compressor[CRYPTO_MAX_ALG_NAME]; */
	// zram is claimed so open request will be failed
	/* _Bool claim; // Protected by bdev->bd_mutex */
} uszram;

int uszram_init(void)
{
	return 0;
}

int uszram_read_blk(unsigned long blk_addr, char data[static USZRAM_BLOCK_SIZE])
{
	if (blk_addr >= USZRAM_BLOCK_COUNT)
		return -1;

	unsigned long pg_addr = blk_addr / BLK_PER_PG;
	unsigned long lock_addr = pg_addr / USZRAM_PG_PER_LOCK;
	unsigned long byte_offset
		= blk_addr % BLK_PER_PG * USZRAM_BLOCK_SIZE;
	struct uszram_page *pg = uszram.pgtbl + pg_addr;
	char pg_buf[USZRAM_PAGE_SIZE];

	// Lock uszram.locks[lock_addr] as reader
	int ret = LZ4_decompress_safe_partial(pg->data, pg_buf, GET_SIZE(pg),
					      byte_offset + USZRAM_BLOCK_SIZE,
					      USZRAM_PAGE_SIZE);
	// Unlock uszram.locks[lock_addr] as reader
	ret -= byte_offset;
	if (ret < 0)
		return ret;
	memcpy(data, pg_buf + byte_offset, USZRAM_BLOCK_SIZE);
	return ret;
}

int uszram_read_pg(unsigned long pg_addr, char data[static USZRAM_PAGE_SIZE])
{
	if (pg_addr >= PAGE_COUNT)
		return -1;

	unsigned long lock_addr = pg_addr / USZRAM_PG_PER_LOCK;
	struct uszram_page *pg = uszram.pgtbl + pg_addr;

	// Lock uszram.locks[lock_addr] as reader
	int ret = LZ4_decompress_safe(pg->data, data, GET_SIZE(pg),
				      USZRAM_PAGE_SIZE);
	// Unlock uszram.locks[lock_addr] as reader
	return ret;
}

int uszram_write_blk(unsigned long blk_addr,
		     const char data[static USZRAM_BLOCK_SIZE])
{
	if (blk_addr >= USZRAM_BLOCK_COUNT)
		return -1;

	unsigned long pg_addr = blk_addr / BLK_PER_PG;
	unsigned long byte_offset
		= blk_addr % BLK_PER_PG * USZRAM_BLOCK_SIZE;
	char pg_buf[USZRAM_PAGE_SIZE];

	int read = uszram_read_pg(pg_addr, pg_buf);
	if (read < 0)
		return read;
	memcpy(pg_buf + byte_offset, data, USZRAM_BLOCK_SIZE);
	return uszram_write_pg(pg_addr, pg_buf);
}

int uszram_write_pg(unsigned long pg_addr,
		    const char data[static USZRAM_PAGE_SIZE])
{
	if (pg_addr >= PAGE_COUNT)
		return -1;

	unsigned long lock_addr = pg_addr / USZRAM_PG_PER_LOCK;
	struct uszram_page *pg = uszram.pgtbl + pg_addr;
	_Bool present = pg->data, huge;
	unsigned long size = GET_SIZE(pg);
	char pg_buf[USZRAM_PAGE_SIZE];

	unsigned new_size = LZ4_compress_default(data, pg_buf,
						 USZRAM_PAGE_SIZE,
						 USZRAM_PAGE_SIZE);
	// Lock uszram.locks[lock_addr] as writer
	if (new_size == 0) {
		new_size = USZRAM_PAGE_SIZE;
		pg->flags |= 1 << HUGE_PAGE;
		huge = 1;
	}
	if (!present || new_size != size) {
		if (present)
			free(pg->data);
		pg->data = malloc(new_size);
	}
	memcpy(pg->data, huge ? data : pg_buf, new_size);
	// Unlock uszram.locks[lock_addr] as writer
	return new_size;
}
