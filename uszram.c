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

static struct page pgtbl[(uint_least64_t)PG_ADDR_MAX + 1];
static struct lock lktbl[(uint_least64_t)LOCK_ADDR_MAX + 1];
// static struct stats stats;

inline static uint_least32_t min(uint_least32_t a, uint_least32_t b)
{
	return a < b ? a : b;
}

inline static void delete_pg(struct page *pg)
{
	free_reachable(pg);
	maybe_reallocate(pg, get_size(pg), 0);
}

inline static int write_pg(struct page *pg,
			   const char raw_pg[static USZRAM_PAGE_SIZE],
			   size_type compr_size,
			   const char compr_pg[static compr_size])
{
	if (compr_size == 0) {
		compr_size = USZRAM_PAGE_SIZE;
		if (is_huge(pg))
			return compr_size;
		compr_pg = raw_pg;
	}
	maybe_reallocate(pg, get_size(pg), compr_size);
	write_compressed(pg, compr_size, compr_pg);
	return compr_size;
}

inline static int write_blk(struct page *pg, size_type blk_offset,
			    size_type blocks,
			    const char data[blocks * USZRAM_BLOCK_SIZE],
			    struct lock *lock)
{
	size_type offset = blk_offset * USZRAM_BLOCK_SIZE;
	size_type new_size = 0;

	if (pg->data == NULL) {
		char raw_pg[USZRAM_PAGE_SIZE] = {0}, compr_pg[MAX_NON_HUGE];
		memcpy(raw_pg + offset, data, USZRAM_BLOCK_SIZE);
		new_size = compress(raw_pg, compr_pg);
		lock_as_writer(lock);
		write_pg(pg, raw_pg, new_size, compr_pg);
		goto out;
	}

	lock_as_writer(lock);
	if (is_huge(pg)) {
		if (read_modify(pg, offset, blocks, data, NULL)) {
			char compr_pg[MAX_NON_HUGE];
			new_size = compress(pg->data, compr_pg);
			write_pg(pg, pg->data, new_size, compr_pg);
		}
	} else {
		char raw_pg[USZRAM_PAGE_SIZE], compr_pg[MAX_NON_HUGE];
		new_size = read_modify(pg, offset, blocks, data, raw_pg);
		if (new_size > 0) {
			new_size = compress(raw_pg, compr_pg);
			write_pg(pg, raw_pg, new_size, compr_pg);
		}
	}
out:
	unlock_as_writer(lock);

	return new_size;
}

int uszram_init(void)
{
	for (uint_least64_t i = 0; i <= LOCK_ADDR_MAX; ++i)
		initialize_lock(lktbl + i);
	return 0;
}

int uszram_exit(void)
{
	for (uint_least64_t i = 0; i <= LOCK_ADDR_MAX; ++i)
		destroy_lock(lktbl + i);
	for (uint_least64_t i = 0; i <= PG_ADDR_MAX; ++i)
		delete_pg(pgtbl + i);
	return 0;
}

int uszram_read_blk(uint_least64_t blk_addr, char data[static USZRAM_BLOCK_SIZE])
{
	if (blk_addr > BLK_ADDR_MAX)
		return -1;

	uint_least64_t pg_addr = blk_addr / BLK_PER_PG;
	struct page *pg = pgtbl + pg_addr;

	if (pg->data == NULL) {
		memset(data, 0, USZRAM_BLOCK_SIZE);
		return 0;
	}

	struct lock *lock = lktbl + pg_addr / PG_PER_LOCK;
	size_type offset = blk_addr % BLK_PER_PG * USZRAM_BLOCK_SIZE;
	int ret = 0;

	lock_as_reader(lock);
	if (is_huge(pg)) {
		memcpy(data, pg->data + offset, USZRAM_BLOCK_SIZE);
	} else {
		char raw_pg[USZRAM_PAGE_SIZE];
		ret = decompress(pg, offset + USZRAM_BLOCK_SIZE, raw_pg);
		if (ret >= 0)
			memcpy(data, raw_pg + offset, USZRAM_BLOCK_SIZE);
	}
	unlock_as_reader(lock);
	return ret;
}

int uszram_read_pg(uint_least64_t pg_addr, char data[static USZRAM_PAGE_SIZE])
{
	if (pg_addr > PG_ADDR_MAX)
		return -1;

	struct page *pg = pgtbl + pg_addr;

	if (pg->data == NULL) {
		memset(data, 0, USZRAM_PAGE_SIZE);
		return 0;
	}

	struct lock *lock = lktbl + pg_addr / PG_PER_LOCK;
	int ret = 0;

	lock_as_reader(lock);
	if (is_huge(pg))
		memcpy(data, pg->data, USZRAM_PAGE_SIZE);
	else
		ret = decompress(pg, USZRAM_PAGE_SIZE, data);
	unlock_as_reader(lock);
	return ret;
}

int uszram_write_blk(uint_least32_t blk_addr, uint_least32_t blocks,
		     const char data[static blocks * USZRAM_BLOCK_SIZE])
{
	if (blocks == 0)
		return 0;
	const uint_least32_t blk_last = blk_addr + blocks - 1;
	if (blk_last < blk_addr || blk_last > BLK_ADDR_MAX)
		return -1;

	const uint_least32_t pg_last = blk_last / BLK_PER_PG;
	const uint_least32_t lk_last = pg_last / PG_PER_LOCK;
	uint_least32_t pg_addr = blk_addr / BLK_PER_PG;
	uint_least32_t lk_addr = pg_addr / PG_PER_LOCK;
	uint_least32_t pg_next = lk_addr * PG_PER_LOCK - 1;
	uint_least32_t blk_next = pg_addr * BLK_PER_PG;

	for (; lk_addr <= lk_last; ++lk_addr) {
		if (lk_addr == lk_last)
			pg_next = pg_last;
		else
			pg_next += PG_PER_LOCK;
		for (; pg_addr <= pg_next; ++pg_addr) {
			if (pg_addr == pg_last)
				blk_next = blk_last + 1;
			else
				blk_next += BLK_PER_PG;
			write_blk(pgtbl + pg_addr, blk_addr % BLK_PER_PG,
				  blk_next - blk_addr, data, lktbl + lk_addr);
			data += blk_next - blk_addr;
			blk_addr = blk_next;
		}
	}

	return 0;
}

int uszram_write_pg(uint_least32_t pg_addr, uint_least32_t pages,
		    const char data[static pages * USZRAM_PAGE_SIZE])
{
	if (pages == 0)
		return 0;
	const uint_least32_t pg_last = pg_addr + pages - 1;
	if (pg_last < pg_addr || pg_last > PG_ADDR_MAX)
		return -1;

	uint_least32_t lk_addr = pg_addr / PG_PER_LOCK;
	uint_least32_t pg_next = lk_addr * PG_PER_LOCK - 1;
	size_type new_size = 0;
	char compr_pg[MAX_NON_HUGE];

	for (; lk_addr <= pg_last / PG_PER_LOCK; ++lk_addr) {
		pages = pg_next + PG_PER_LOCK;
		if (pg_next <= pages)
			pg_next = min(pages, pg_last);
		else
			pg_next = pg_last;
		for (; pg_addr <= pg_next; ++pg_addr) {
			new_size = compress(data, compr_pg);
			lock_as_writer(lktbl + lk_addr);
			write_pg(pgtbl + pg_addr, data, new_size, compr_pg);
			unlock_as_writer(lktbl + lk_addr);
		}
	}

	return 0;
}

int uszram_delete_pg(uint_least32_t pg_addr, uint_least32_t pages)
{
	if (pages == 0)
		return 0;
	const uint_least32_t pg_last = pg_addr + pages - 1;
	if (pg_last < pg_addr || pg_last > PG_ADDR_MAX)
		return -1;

	uint_least32_t lk_addr = pg_addr / PG_PER_LOCK;
	uint_least32_t pg_next = lk_addr * PG_PER_LOCK - 1;

	for (; lk_addr <= pg_last / PG_PER_LOCK; ++lk_addr) {
		pages = pg_next + PG_PER_LOCK;
		if (pg_next <= pages)
			pg_next = min(pages, pg_last);
		else
			pg_next = pg_last;
		for (; pg_addr <= pg_next; ++pg_addr) {
			lock_as_writer  (lktbl + lk_addr);
			delete_pg       (pgtbl + pg_addr);
			unlock_as_writer(lktbl + lk_addr);
		}
	}

	return 0;
}
