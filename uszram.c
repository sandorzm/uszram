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


static struct page pgtbl[USZRAM_PAGE_COUNT];
static struct lock lktbl[LOCK_COUNT];
static struct stats {
	atomic_uint_least64_t compr_data_size;	// Total heap data except locks
	atomic_uint_least64_t pages_stored;	// # of pages currently stored
	atomic_uint_least64_t huge_pages;	// # of huge pages
	atomic_uint_least64_t num_compr;	// # of compression attempts
	atomic_uint_least64_t failed_compr;	// Attempts resulting in huge pages
} stats;

inline static uint_least32_t ceil_div(uint_least32_t a, uint_least32_t b)
{
	return a / b + (a % b != 0);
}

inline static void delete_pg(struct page *pg)
{
	if (pg->data == NULL)
		return;
	--stats.pages_stored;
	if (is_huge(pg))
		--stats.huge_pages;
	else
		stats.compr_data_size -= free_reachable(pg);
	stats.compr_data_size += maybe_reallocate(pg, get_size_primary(pg), 0);
	write_compressed(pg, 0, NULL);
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
		++stats.failed_compr;
		compr_size = USZRAM_PAGE_SIZE;
		if (is_huge(pg))
			return compr_size;
		++stats.huge_pages;
		compr_pg = raw_pg;
	} else if (is_huge(pg)) {
		--stats.huge_pages;
	}
	stats.compr_data_size
		+= maybe_reallocate(pg, get_size_primary(pg), compr_size);
	write_compressed(pg, compr_size, compr_pg);
	return compr_size;
}

inline static size_type write_blk_helper(
	struct page *pg, char compr_pg[static MAX_NON_HUGE],
	const char raw_pg[static USZRAM_PAGE_SIZE])
{
	size_type new_size = compress(raw_pg, compr_pg);
	++stats.num_compr;
	return write_pg(pg, new_size, compr_pg, raw_pg);
}

inline static int write_blk(struct page *pg, size_type offset, size_type blocks,
			    const char data[static blocks * USZRAM_BLOCK_SIZE],
			    char compr_pg[static MAX_NON_HUGE])
{
	if (pg->data == NULL) {
		++stats.pages_stored;
		char raw_pg[USZRAM_PAGE_SIZE] = {0};
		memcpy(raw_pg + offset * USZRAM_BLOCK_SIZE, data,
		       blocks * USZRAM_BLOCK_SIZE);
		return write_blk_helper(pg, compr_pg, raw_pg);
	}

	if (is_huge(pg)) {
		if (read_modify(pg, offset, blocks, data, NULL, NULL))
			return write_blk_helper(pg, compr_pg, pg->data);
	} else {
		char raw_pg[USZRAM_PAGE_SIZE];
		int size_change = 0;
		int ret = read_modify(pg, offset, blocks, data, raw_pg,
				      &size_change);
		switch (ret) {
		case 1:
			stats.compr_data_size -= free_reachable(pg);
			return write_blk_helper(pg, compr_pg, raw_pg);
		case 0:
			stats.compr_data_size += size_change;
		default:
			return ret;
		}
	}

	return 0;
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
	for (uint_least64_t i = 0; i != LOCK_COUNT; ++i)
		destroy_lock(lktbl + i);
	uint_least64_t pg_addr = 0, pg_next = 0;
	for (uint_least64_t lk_addr = 0; lk_addr != LOCK_COUNT; ++lk_addr)
		for (pg_next += PG_PER_LOCK; pg_addr != pg_next; ++pg_addr)
			delete_pg(pgtbl + pg_addr);
	return 0;
}

int uszram_read_pg(uint_least32_t pg_addr, uint_least32_t pages,
		   char data[static pages * USZRAM_PAGE_SIZE])
{
	if ((uint_least64_t)pg_addr + pages > USZRAM_PAGE_COUNT)
		return -1;

	const uint_least32_t pg_end = pg_addr + pages; // May overflow to 0
	const uint_least32_t lk_end = ceil_div(pg_end, PG_PER_LOCK);
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
	const uint_least32_t pg_end = ceil_div(blk_end, BLK_PER_PG);
	const uint_least32_t lk_end = ceil_div(pg_end, PG_PER_LOCK);
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
	const uint_least32_t lk_end = ceil_div(pg_end, PG_PER_LOCK);
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
			++stats.num_compr;

			lock_as_writer(lktbl + lk_addr);
			stats.pages_stored += (pgtbl[pg_addr].data == NULL);
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
	const uint_least32_t pg_end = ceil_div(blk_end, BLK_PER_PG);
	const uint_least32_t lk_end = ceil_div(pg_end, PG_PER_LOCK);
	uint_least32_t pg_addr = blk_addr / BLK_PER_PG;
	uint_least32_t lk_addr = pg_addr / PG_PER_LOCK;
	uint_least32_t pg_next = lk_addr * PG_PER_LOCK;
	blocks = pg_addr * BLK_PER_PG;
	char compr_pg[USZRAM_PAGE_SIZE];

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

			lock_as_writer(lktbl + lk_addr);
			write_blk(pgtbl + pg_addr, blk_addr % BLK_PER_PG,
				  blocks - blk_addr, data, compr_pg);
			unlock_as_writer(lktbl + lk_addr);

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
	const uint_least32_t lk_end = ceil_div(pg_end, PG_PER_LOCK);
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

int uszram_pg_size(uint_least32_t pg_addr)
{
	if (pg_addr > (uint_least32_t)(USZRAM_PAGE_COUNT - 1))
		return -1;

	size_type size = sizeof (struct page);
	struct lock *lock = lktbl + pg_addr / PG_PER_LOCK;

	lock_as_reader(lock);
	size += get_size(pgtbl + pg_addr);
	unlock_as_reader(lock);

	return size;
}

_Bool uszram_pg_exists(uint_least32_t pg_addr)
{
	if (pg_addr > (uint_least32_t)(USZRAM_PAGE_COUNT - 1))
		return 0;
	return pgtbl[pg_addr].data != NULL;
}

_Bool uszram_pg_is_huge(uint_least32_t pg_addr)
{
	if (pg_addr > (uint_least32_t)(USZRAM_PAGE_COUNT - 1))
		return 0;
	return is_huge(pgtbl + pg_addr);
}

uint_least64_t uszram_total_size(void)
{
	return sizeof pgtbl + sizeof lktbl + stats.compr_data_size;
}

uint_least64_t uszram_pages_stored(void)
{
	return stats.pages_stored;
}

uint_least64_t uszram_huge_pages(void)
{
	return stats.huge_pages;
}

uint_least64_t num_compr(void)
{
	return stats.num_compr;
}

uint_least64_t uszram_failed_compr(void)
{
	return stats.failed_compr;
}
