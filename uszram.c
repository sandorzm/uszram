#include "allocators/uszram-basic.h"

#ifdef USZRAM_LZ4
#  include "compressors/uszram-lz4.h"
#else
#  include "compressors/uszram-zapi.h"
#endif

#ifdef USZRAM_POSIX_RW
#  include "locks/uszram-posix-rw.h"
#elif defined USZRAM_POSIX_MTX
#  include "locks/uszram-posix-mtx.h"
#else
#  include "locks/uszram-iso-mtx.h"
#endif


/*
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
*/

static struct uszram {
	struct page pgtbl[(int_least64_t)PG_ADDR_MAX + 1];
	struct lock locks[(int_least64_t)LOCK_ADDR_MAX + 1];
	// struct stats stats;
} uszram;

inline static int write_page(struct page *pg, const char *raw_pg,
			     const char *compr_pg, int compr_size)
{
	if (compr_size == 0) {
		compr_size = USZRAM_PAGE_SIZE;
		if (is_huge(pg))
			return compr_size;
		compr_pg = raw_pg;
	}
	maybe_reallocate(pg, get_size(pg), compr_size);
	write_compressed(pg, compr_pg, compr_size);
	return compr_size;
}

int uszram_init(void)
{
	for (uint_least32_t i = 0; i <= LOCK_ADDR_MAX; ++i)
		initialize_lock(uszram.locks + i);
	return 0;
}

int uszram_exit(void)
{
	for (uint_least32_t i = 0; i <= LOCK_ADDR_MAX; ++i)
		destroy_lock(uszram.locks + i);
	return 0;
}

int uszram_read_blk(uint_least32_t blk_addr, char data[static USZRAM_BLOCK_SIZE])
{
	if (blk_addr > USZRAM_BLK_ADDR_MAX)
		return -1;

	uint_least32_t pg_addr = blk_addr / BLK_PER_PG;
	struct page *pg = uszram.pgtbl + pg_addr;

	if (pg->data == NULL) {
		memset(data, 0, USZRAM_BLOCK_SIZE);
		return 0;
	}

	struct lock *lock = uszram.locks + pg_addr / USZRAM_PG_PER_LOCK;
	size_type offset = blk_addr % BLK_PER_PG * USZRAM_BLOCK_SIZE;
	int ret = 0;

	lock_as_reader(lock);
	if (is_huge(pg)) {
		memcpy(data, pg->data + offset, USZRAM_BLOCK_SIZE);
	} else {
		char raw_pg[USZRAM_PAGE_SIZE];
		ret = decompress(pg, raw_pg, offset + USZRAM_BLOCK_SIZE);
		if (ret >= 0)
			memcpy(data, raw_pg + offset, USZRAM_BLOCK_SIZE);
	}
	unlock_as_reader(lock);
	return ret;
}

int uszram_read_pg(uint_least32_t pg_addr, char data[static USZRAM_PAGE_SIZE])
{
	if (pg_addr > PG_ADDR_MAX)
		return -1;

	struct page *pg = uszram.pgtbl + pg_addr;

	if (pg->data == NULL) {
		memset(data, 0, USZRAM_PAGE_SIZE);
		return 0;
	}

	struct lock *lock = uszram.locks + pg_addr / USZRAM_PG_PER_LOCK;
	int ret = 0;

	lock_as_reader(lock);
	if (is_huge(pg))
		memcpy(data, pg->data, USZRAM_PAGE_SIZE);
	else
		ret = decompress(pg, data, USZRAM_PAGE_SIZE);
	unlock_as_reader(lock);
	return ret;
}

int uszram_write_blk(uint_least32_t blk_addr,
		     const char data[static USZRAM_BLOCK_SIZE])
{
	if (blk_addr > USZRAM_BLK_ADDR_MAX)
		return -1;

	uint_least32_t pg_addr = blk_addr / BLK_PER_PG;
	struct page *pg = uszram.pgtbl + pg_addr;
	int offset = blk_addr % BLK_PER_PG * USZRAM_BLOCK_SIZE;
	struct lock *lock = uszram.locks + pg_addr / USZRAM_PG_PER_LOCK;
	int new_size;

	if (pg->data == NULL) {
		char raw_pg[USZRAM_PAGE_SIZE] = {0}, compr_pg[MAX_NON_HUGE];
		memcpy(raw_pg + offset, data, USZRAM_BLOCK_SIZE);
		new_size = compress(raw_pg, compr_pg);
		lock_as_writer(lock);
		write_page(pg, raw_pg, compr_pg, new_size);
		goto out;
	}

	lock_as_writer(lock);
	if (is_huge(pg)) {
		if (read_modify(pg, data, offset, 1, NULL)) {
			char compr_pg[MAX_NON_HUGE];
			new_size = compress(pg->data, compr_pg);
			write_page(pg, pg->data, compr_pg, new_size);
		}
	} else {
		char raw_pg[USZRAM_PAGE_SIZE], compr_pg[MAX_NON_HUGE];
		new_size = read_modify(pg, data, offset, 1, raw_pg);
		if (new_size > 0) {
			new_size = compress(raw_pg, compr_pg);
			write_page(pg, raw_pg, compr_pg, new_size);
		}
	}
out:
	unlock_as_writer(lock);
	return new_size;
}

int uszram_write_pg(uint_least32_t pg_addr,
		    const char data[static USZRAM_PAGE_SIZE])
{
	if (pg_addr > PG_ADDR_MAX)
		return -1;

	struct page *pg = uszram.pgtbl + pg_addr;
	struct lock *lock = uszram.locks + pg_addr / USZRAM_PG_PER_LOCK;
	char compr_pg[MAX_NON_HUGE];

	int new_size = compress(data, compr_pg);

	lock_as_writer(lock);
	write_page(pg, data, compr_pg, new_size);
	unlock_as_writer(lock);
	return new_size;
}
