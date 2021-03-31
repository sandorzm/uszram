#include <stdatomic.h>

#include "allocators/uszram-basic.h"

#ifdef USZRAM_LZ4
#  include "compressors/uszram-lz4.h"
#else
#  include "compressors/uszram-zapi.h"
#endif

#ifdef USZRAM_PTH_RW
#  include "locks/uszram-pth-rw.h"
#elif defined USZRAM_PTH_MTX
#  include "locks/uszram-pth-mtx.h"
#else
#  include "locks/uszram-std-mtx.h"
#endif


#define CEIL_DIV(a, b) (a / b + (a % b != 0))


static struct page pgtbl[USZRAM_PAGE_COUNT];
static struct lock lktbl[LOCK_COUNT];
static struct stats {
	atomic_uint_least64_t compr_data_size;	// Total compressed data on heap
	atomic_uint_least64_t num_reads;	// Failed + successful
	atomic_uint_least64_t num_writes;	// Failed + successful
	atomic_uint_least64_t failed_reads;	// Due to invalid address
	atomic_uint_least64_t failed_writes;	// Bad address or insufficient memory
	atomic_uint_least64_t huge_pages;	// Number of huge pages
	atomic_uint_least64_t huge_pages_since;	// Failed compressions since uszram init
	atomic_uint_least64_t pages_stored;	// Number of pages currently stored
	atomic_uint_least64_t max_used_pages;	// Maximum of pages_stored since init
} stats;

inline static void delete_pg(struct page *pg)
{
	free_reachable(pg);
	maybe_reallocate(pg, get_size_primary(pg), 0);
}

inline static int read_pg(struct page *pg, char data[static USZRAM_PAGE_SIZE],
			  struct lock *lock)
{
	int ret = 0;

	if (pg->data == NULL) {
		memset(data, 0, USZRAM_PAGE_SIZE);
		return ret;
	}

	lock_as_reader(lock);
	if (is_huge(pg))
		memcpy(data, pg->data, USZRAM_PAGE_SIZE);
	else
		ret = decompress(pg, USZRAM_PAGE_SIZE, data);
	unlock_as_reader(lock);

	return ret;
}

inline static int read_blk(struct page *pg, size_type offset, size_type blocks,
			   char data[static blocks * USZRAM_BLOCK_SIZE],
			   struct lock *lock)
{
	offset *= USZRAM_BLOCK_SIZE;
	blocks *= USZRAM_BLOCK_SIZE;
	int ret = 0;

	if (pg->data == NULL) {
		memset(data, 0, blocks);
		return ret;
	}

	lock_as_reader(lock);
	if (is_huge(pg)) {
		memcpy(data, pg->data + offset, blocks);
	} else {
		char raw_pg[USZRAM_PAGE_SIZE];
		ret = decompress(pg, offset + blocks, raw_pg);
		if (ret == 0)
			memcpy(data, raw_pg + offset, blocks);
	}
	unlock_as_reader(lock);

	return ret;
}

inline static size_type write_pg(struct page *pg, size_type compr_size,
				 const char compr_pg[static compr_size],
				 const char raw_pg[static USZRAM_PAGE_SIZE])
{
	if (compr_size == 0) {
		compr_size = USZRAM_PAGE_SIZE;
		if (is_huge(pg))
			return compr_size;
		compr_pg = raw_pg;
	}
	maybe_reallocate(pg, get_size_primary(pg), compr_size);
	write_compressed(pg, compr_size, compr_pg);
	return compr_size;
}

inline static int write_blk(struct page *pg, size_type offset, size_type blocks,
			    const char data[static blocks * USZRAM_BLOCK_SIZE],
			    struct lock *lock)
{
	size_type new_size = 0;

	if (pg->data == NULL) {
		char raw_pg[USZRAM_PAGE_SIZE] = {0}, compr_pg[MAX_NON_HUGE];
		memcpy(raw_pg + offset * USZRAM_BLOCK_SIZE, data,
		       blocks * USZRAM_BLOCK_SIZE);
		new_size = compress(raw_pg, compr_pg);

		lock_as_writer(lock);
		write_pg(pg, new_size, compr_pg, raw_pg);
		unlock_as_writer(lock);

		return new_size;
	}

	lock_as_writer(lock);
	if (is_huge(pg)) {
		if (read_modify(pg, offset, blocks, data, NULL)) {
			char compr_pg[MAX_NON_HUGE];
			new_size = compress(pg->data, compr_pg);
			write_pg(pg, new_size, compr_pg, pg->data);
		}
	} else {
		char raw_pg[USZRAM_PAGE_SIZE], compr_pg[MAX_NON_HUGE];
		new_size = read_modify(pg, offset, blocks, data, raw_pg);
		if (new_size == 1) {
			new_size = compress(raw_pg, compr_pg);
			write_pg(pg, new_size, compr_pg, raw_pg);
		}
	}
	unlock_as_writer(lock);

	return new_size;
}

int uszram_delete_all(void)
{
	uint_least64_t pg_addr = 0, pg_next = 0;
	for (uint_least64_t lk_addr = 0; lk_addr != LOCK_COUNT; ++lk_addr) {
		pg_next += PG_PER_LOCK;
		lock_as_writer(lktbl + lk_addr);
		for (; pg_addr != pg_next; ++pg_addr)
			delete_pg(pgtbl + pg_addr);
		unlock_as_writer(lktbl + lk_addr);
	}
	return 0;
}

int uszram_init(void)
{
	for (uint_least64_t i = 0; i != LOCK_COUNT; ++i)
		initialize_lock(lktbl + i);
	return 0;
}

int uszram_exit(void)
{
	uszram_delete_all();
	for (uint_least64_t i = 0; i != LOCK_COUNT; ++i)
		destroy_lock(lktbl + i);
	return 0;
}

int uszram_read_pg(uint_least32_t pg_addr, uint_least32_t pages,
		   char data[static pages * USZRAM_PAGE_SIZE])
{
	if ((uint_least64_t)pg_addr + pages > USZRAM_PAGE_COUNT)
		return -1;

	const uint_least32_t pg_end = pg_addr + pages; // May overflow to 0
	const uint_least32_t lk_end = CEIL_DIV(pg_end, PG_PER_LOCK);
	uint_least32_t lk_addr = pg_addr / PG_PER_LOCK;
	pages = lk_addr * PG_PER_LOCK;

	for (; lk_addr != lk_end; ++lk_addr) {
		if (lk_addr == (uint_least32_t)(lk_end - 1))
			pages = pg_end;
		else
			pages += PG_PER_LOCK;
		for (; pg_addr != pages; ++pg_addr) {
			read_pg(pgtbl + pg_addr, data, lktbl + lk_addr);
			data += USZRAM_PAGE_SIZE;
		}
	}

	return 0;
}

int uszram_read_blk(uint_least32_t blk_addr, uint_least32_t blocks,
		    char data[static blocks * USZRAM_BLOCK_SIZE])
{
	if ((uint_least64_t)blk_addr + blocks > USZRAM_BLOCK_COUNT)
		return -1;

	const uint_least32_t blk_end = blk_addr + blocks; // May overflow to 0
	const uint_least32_t pg_end = CEIL_DIV(blk_end, BLK_PER_PG);
	const uint_least32_t lk_end = CEIL_DIV(pg_end, PG_PER_LOCK);
	uint_least32_t pg_addr = blk_addr / BLK_PER_PG;
	uint_least32_t lk_addr = pg_addr / PG_PER_LOCK;
	uint_least32_t pg_next = lk_addr * PG_PER_LOCK;
	blocks = pg_addr * BLK_PER_PG;

	for (; lk_addr != lk_end; ++lk_addr) {
		if (lk_addr == (uint_least32_t)(lk_end - 1))
			pg_next = pg_end;
		else
			pg_next += PG_PER_LOCK;
		for (; pg_addr != pg_next; ++pg_addr) {
			if (pg_addr == (uint_least32_t)(pg_end - 1))
				blocks = blk_end;
			else
				blocks += BLK_PER_PG;
			read_blk(pgtbl + pg_addr, blk_addr % BLK_PER_PG,
				 blocks - blk_addr, data, lktbl + lk_addr);
			data += blocks - blk_addr;
			blk_addr = blocks;
		}
	}

	return 0;

}

int uszram_write_pg(uint_least32_t pg_addr, uint_least32_t pages,
		    const char data[static pages * USZRAM_PAGE_SIZE])
{
	if ((uint_least64_t)pg_addr + pages > USZRAM_PAGE_COUNT)
		return -1;

	const uint_least32_t pg_end = pg_addr + pages; // May overflow to 0
	const uint_least32_t lk_end = CEIL_DIV(pg_end, PG_PER_LOCK);
	uint_least32_t lk_addr = pg_addr / PG_PER_LOCK;
	pages = lk_addr * PG_PER_LOCK;
	size_type new_size = 0;
	char compr_pg[MAX_NON_HUGE];

	for (; lk_addr != lk_end; ++lk_addr) {
		if (lk_addr == (uint_least32_t)(lk_end - 1))
			pages = pg_end;
		else
			pages += PG_PER_LOCK;
		for (; pg_addr != pages; ++pg_addr) {
			new_size = compress(data, compr_pg);
			lock_as_writer(lktbl + lk_addr);
			write_pg(pgtbl + pg_addr, new_size, compr_pg, data);
			unlock_as_writer(lktbl + lk_addr);
			data += USZRAM_PAGE_SIZE;
		}
	}

	return 0;
}

int uszram_write_blk(uint_least32_t blk_addr, uint_least32_t blocks,
		     const char data[static blocks * USZRAM_BLOCK_SIZE])
{
	if ((uint_least64_t)blk_addr + blocks > USZRAM_BLOCK_COUNT)
		return -1;

	const uint_least32_t blk_end = blk_addr + blocks; // May overflow to 0
	const uint_least32_t pg_end = CEIL_DIV(blk_end, BLK_PER_PG);
	const uint_least32_t lk_end = CEIL_DIV(pg_end, PG_PER_LOCK);
	uint_least32_t pg_addr = blk_addr / BLK_PER_PG;
	uint_least32_t lk_addr = pg_addr / PG_PER_LOCK;
	uint_least32_t pg_next = lk_addr * PG_PER_LOCK;
	blocks = pg_addr * BLK_PER_PG;

	for (; lk_addr != lk_end; ++lk_addr) {
		if (lk_addr == (uint_least32_t)(lk_end - 1))
			pg_next = pg_end;
		else
			pg_next += PG_PER_LOCK;
		for (; pg_addr != pg_next; ++pg_addr) {
			if (pg_addr == (uint_least32_t)(pg_end - 1))
				blocks = blk_end;
			else
				blocks += BLK_PER_PG;
			write_blk(pgtbl + pg_addr, blk_addr % BLK_PER_PG,
				  blocks - blk_addr, data, lktbl + lk_addr);
			data += blocks - blk_addr;
			blk_addr = blocks;
		}
	}

	return 0;
}

int uszram_delete_pg(uint_least32_t pg_addr, uint_least32_t pages)
{
	if ((uint_least64_t)pg_addr + pages > USZRAM_PAGE_COUNT)
		return -1;

	const uint_least32_t pg_end = pg_addr + pages; // May overflow to 0
	const uint_least32_t lk_end = CEIL_DIV(pg_end, PG_PER_LOCK);
	uint_least32_t lk_addr = pg_addr / PG_PER_LOCK;
	pages = lk_addr * PG_PER_LOCK;

	for (; lk_addr != lk_end; ++lk_addr) {
		if (lk_addr == (uint_least32_t)(lk_end - 1))
			pages = pg_end;
		else
			pages += PG_PER_LOCK;
		for (; pg_addr != pages; ++pg_addr) {
			/* Could move lock/unlock outside this loop to increase
			 * speed but also block other threads for longer */
			lock_as_writer  (lktbl + lk_addr);
			delete_pg       (pgtbl + pg_addr);
			unlock_as_writer(lktbl + lk_addr);
		}
	}

	return 0;
}
