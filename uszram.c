#include <string.h>
#include <threads.h>

#include <lz4.h>

#ifdef USZRAM_LZ4
#  include "compressors/uszram-lz4.h"
#else
#  include "compressors/uszram-zapi.h"
#endif


struct stats {
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
	struct page pgtbl[(int_least64_t)PG_ADDR_MAX + 1];
	mtx_t locks[(int_least64_t)LOCK_ADDR_MAX + 1];
	struct stats stats;
} uszram;

inline static int write_compressed(struct page *pg, const char *raw_pg,
				   const char *compr_pg, int compr_size)
{
	if (compr_size == 0) {
		compr_size = USZRAM_PAGE_SIZE;
		if (get_pg_raw(pg))
			goto out;
		compr_pg = raw_pg;
	}
	maybe_realloc(pg, compr_size);
	pg->flags = compr_size;		// Change when more flags are added
	memcpy(pg->data, compr_pg, compr_size);
out:
	return compr_size;
}

inline static int change_blk_write(struct page *pg, char *raw_pg,
				   int offset, const char *new_blk)
{
	char compr_pg[USZRAM_PAGE_SIZE];
	memcpy(raw_pg + offset, new_blk, USZRAM_BLOCK_SIZE);
	int new_size = LZ4_compress_default(raw_pg, compr_pg,
					    USZRAM_PAGE_SIZE,
					    USZRAM_PAGE_SIZE);
	return write_compressed(pg, raw_pg, compr_pg, new_size);
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
	struct page *pg = uszram.pgtbl + pg_addr;

	if (!get_pg_alloc(pg)) {
		memset(data, 0, USZRAM_BLOCK_SIZE);
		return 0;
	}

	uint_least32_t lock_addr = pg_addr / USZRAM_PG_PER_LOCK;
	int offset = blk_addr % BLK_PER_PG * USZRAM_BLOCK_SIZE;
	int ret = 0;

	// Lock uszram.locks[lock_addr] as reader
	char *raw_pg = get_pg_raw(pg);
	if (raw_pg) {
		memcpy(data, raw_pg + offset, USZRAM_BLOCK_SIZE);
	} else {
		char raw_pg[USZRAM_PAGE_SIZE];
		ret = decompress(pg, raw_pg, offset + USZRAM_BLOCK_SIZE);
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

	struct page *pg = uszram.pgtbl + pg_addr;

	if (!get_pg_alloc(pg)) {
		memset(data, 0, USZRAM_PAGE_SIZE);
		return 0;
	}

	uint_least32_t lock_addr = pg_addr / USZRAM_PG_PER_LOCK;
	int ret = 0;

	// Lock uszram.locks[lock_addr] as reader
	char *raw_pg = get_pg_raw(pg);
	if (raw_pg)
		memcpy(data, raw_pg, USZRAM_PAGE_SIZE);
	else
		ret = decompress(pg, data, USZRAM_PAGE_SIZE);
	// Unlock uszram.locks[lock_addr] as reader
	return ret;
}

int uszram_write_blk(uint_least32_t blk_addr,
		     const char data[static USZRAM_BLOCK_SIZE])
{
	if (blk_addr > USZRAM_BLK_ADDR_MAX)
		return -1;

	uint_least32_t pg_addr = blk_addr / BLK_PER_PG;
	struct page *pg = uszram.pgtbl + pg_addr;

	uint_least32_t lock_addr = pg_addr / USZRAM_PG_PER_LOCK;
	int offset = blk_addr % BLK_PER_PG * USZRAM_BLOCK_SIZE;
	int new_size;

	// Lock uszram.pgtbl[lock_addr] as writer
	char *raw_pg = get_pg_raw(pg);
	if (raw_pg) {
		new_size = change_blk_write(pg, raw_pg, offset, data);
	} else {
		char raw_pg[USZRAM_PAGE_SIZE] = {0};
		if (pg->data) {
			new_size = decompress(pg, raw_pg, USZRAM_PAGE_SIZE);
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

	struct page *pg = uszram.pgtbl + pg_addr;
	uint_least32_t lock_addr = pg_addr / USZRAM_PG_PER_LOCK;
	char compr_pg[USZRAM_PAGE_SIZE];

	int new_size = LZ4_compress_default(data, compr_pg, USZRAM_PAGE_SIZE,
					    USZRAM_PAGE_SIZE);
	// Lock uszram.locks[lock_addr] as writer
	new_size = write_compressed(pg, data, compr_pg, new_size);
	// Unlock uszram.locks[lock_addr] as writer
	return new_size;
}
