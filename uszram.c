#include "allocators/uszram-basic.h"

#ifdef USZRAM_LZ4
#  include "compressors/uszram-lz4.h"
#else
#  include "compressors/uszram-zapi.h"
#endif


static atomic_bool initialized;
static struct page pgtbl[USZRAM_PAGE_COUNT];
static struct {
	atomic_uint_least64_t compr_data_size;	// Total heap data except locks
	atomic_uint_least64_t pages_stored;	// # of pages currently stored
	atomic_uint_least64_t huge_pages;	// # of huge pages
	atomic_uint_least64_t num_compr;	// # of compression attempts
	atomic_uint_least64_t failed_compr;	// Attempts resulting in huge pages
} stats;

struct blkloop {
	const uint_least32_t blk_end, pg_last;
	uint_least32_t pg_addr;
};

inline static struct blkloop make_blkloop(uint_least32_t blk_addr,
					  uint_least32_t blocks)
{
	const uint_least32_t blk_end = blk_addr + blocks,
			     pg_last = (blk_end - 1u) / BLK_PER_PG,
			     pg_addr = blk_addr / BLK_PER_PG;
	return (struct blkloop){
		.blk_end = blk_end,
		.pg_last = pg_last,
		.pg_addr = pg_addr,
	};
}

static void delete_pg(struct page *pg)
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

static int read_pg(uint_least32_t pg_addr, char data[static USZRAM_PAGE_SIZE])
{
	struct page *const pg = pgtbl + pg_addr;
	int ret = 0;

	if (pg->data == NULL) {
		memset(data, 0, USZRAM_PAGE_SIZE);
		return ret;
	}

	lock_as_reader(&pg->lock);
	if (is_huge(pg))
		memcpy(data, pg->data, USZRAM_PAGE_SIZE);
	else
		ret = decompress(pg, USZRAM_PAGE_SIZE, data);
	unlock_as_reader(&pg->lock);

	return ret;
}

static int read_blk(uint_least32_t pg_addr, size_type offset, size_type blocks,
		    char data[static blocks * USZRAM_BLOCK_SIZE])
{
	struct page *const pg = pgtbl + pg_addr;
	offset *= USZRAM_BLOCK_SIZE;
	blocks *= USZRAM_BLOCK_SIZE;
	int ret = 0;

	if (pg->data == NULL) {
		memset(data, 0, blocks);
		return ret;
	}

	lock_as_reader(&pg->lock);
	if (is_huge(pg)) {
		memcpy(data, pg->data + offset, blocks);
	} else {
		char raw_pg[USZRAM_PAGE_SIZE];
		ret = decompress(pg, offset + blocks, raw_pg);
		if (ret >= 0)
			memcpy(data, raw_pg + offset, blocks);
	}
	unlock_as_reader(&pg->lock);

	return ret;
}

static size_type write_pg_common(struct page *pg, size_type compr_size,
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

static size_type write_pg(uint_least32_t pg_addr,
			  char compr_pg[static MAX_NON_HUGE],
			  const char data[static USZRAM_PAGE_SIZE])
{
	struct page *const pg = pgtbl + pg_addr;
	size_type new_size = compress(data, compr_pg);
	++stats.num_compr;

	lock_as_writer(&pg->lock);
	stats.pages_stored += pg->data == NULL;
	write_pg_common(pg, new_size, compr_pg, data);
	unlock_as_writer(&pg->lock);

	return new_size;
}

inline static size_type write_blk_helper(
	struct page *pg,
	char compr_pg[static MAX_NON_HUGE],
	const char raw_pg[static USZRAM_PAGE_SIZE])
{
	const size_type new_size = compress(raw_pg, compr_pg);
	++stats.num_compr;
	return write_pg_common(pg, new_size, compr_pg, raw_pg);
}

static int write_blk(uint_least32_t pg_addr, size_type offset,
		     size_type blocks,
		     const char data[static blocks * USZRAM_BLOCK_SIZE],
		     char compr_pg[static MAX_NON_HUGE])
{
	struct page *const pg = pgtbl + pg_addr;
	int ret = 0;

	lock_as_writer(&pg->lock);
	if (pg->data == NULL) {
		++stats.pages_stored;
		char raw_pg[USZRAM_PAGE_SIZE] = {0};
		memcpy(raw_pg + offset * USZRAM_BLOCK_SIZE, data,
		       blocks * USZRAM_BLOCK_SIZE);
		ret = write_blk_helper(pg, compr_pg, raw_pg);
		goto out;
	}

	if (is_huge(pg)) {
		if (read_modify(pg, offset, blocks, data, NULL))
			ret = write_blk_helper(pg, compr_pg, pg->data);
	} else {
		char raw_pg[USZRAM_PAGE_SIZE];
		const int old_size = get_size(pg);
		ret = read_modify(pg, offset, blocks, data, raw_pg);
		switch (ret) {
		case 1:
			stats.compr_data_size -= free_reachable(pg);
			ret = write_blk_helper(pg, compr_pg, raw_pg);
			break;
		case 0:
			stats.compr_data_size += (int)get_size(pg) - old_size;
		}
	}
out:
	unlock_as_writer(&pg->lock);
	return 0;
}

static int delete_blk(uint_least32_t pg_addr, size_type offset,
		      size_type blocks, char compr_pg[static MAX_NON_HUGE])
{
	struct page *const pg = pgtbl + pg_addr;
	int ret = 0;

	if (pg->data == NULL)
		return ret;

	lock_as_writer(&pg->lock);
	if (is_huge(pg)) {
		switch (read_modify(pg, offset, blocks, NULL, NULL)) {
		case 2:
			delete_pg(pg);
			break;
		case 1:
			ret = write_blk_helper(pg, compr_pg, pg->data);
		}
	} else {
		char raw_pg[USZRAM_PAGE_SIZE];
		const int old_size = get_size(pg);
		ret = read_modify(pg, offset, blocks, NULL, raw_pg);
		switch (ret) {
		case 2:
			delete_pg(pg);
			break;
		case 1:
			stats.compr_data_size -= free_reachable(pg);
			ret = write_blk_helper(pg, compr_pg, raw_pg);
			break;
		case 0:
			stats.compr_data_size += (int)get_size(pg) - old_size;
		}
	}
	unlock_as_writer(&pg->lock);
	return ret;
}

int uszram_delete_all(void)
{

	for (uint_least64_t i = 0; i != USZRAM_PAGE_COUNT; ++i) {
		lock_as_writer(&pgtbl[i].lock);
		delete_pg(pgtbl + i);
		pgtbl[i].lock = 0;
	}
	return 0;
}

int uszram_init(void)
{
	if (initialized)
		return -1;
	for (uint_least64_t i = 0; i < USZRAM_PAGE_COUNT; ++i)
		initialize_lock(&pgtbl[i].lock);
	initialized = 1;
	return 0;
}

int uszram_exit(void)
{
	if (!initialized)
		return -1;
	initialized = 0;
	uszram_delete_all();
	stats.num_compr = stats.failed_compr = 0;
	return 0;
}

int uszram_read_pg(uint_least32_t pg_addr, uint_least32_t pages,
		   char data[static pages * USZRAM_PAGE_SIZE])
{
	if (pages == 0)
		return 0;
	if ((uint_least64_t)pg_addr + pages > USZRAM_PAGE_COUNT)
		return -1;

	const uint_least32_t pg_end = pg_addr + pages;
	for (; pg_addr != pg_end; ++pg_addr) {
		read_pg(pg_addr, data);
		data += USZRAM_PAGE_SIZE;
	}

	return 0;
}

int uszram_read_blk(uint_least32_t blk_addr, uint_least32_t blocks,
		    char data[static blocks * USZRAM_BLOCK_SIZE])
{
	if (blocks == 0)
		return 0;
	if ((uint_least64_t)blk_addr + blocks > USZRAM_BLOCK_COUNT)
		return -1;

	struct blkloop l = make_blkloop(blk_addr, blocks);
	const size_type offset = blk_addr % BLK_PER_PG;
	blocks = BLK_PER_PG - offset;

	if (l.pg_addr != l.pg_last) {
		read_blk(l.pg_addr, offset, blocks, data);
		data += blocks * USZRAM_BLOCK_SIZE;
		blk_addr += blocks;
		++l.pg_addr;
	}
	for (; l.pg_addr != l.pg_last; ++l.pg_addr) {
		read_blk(l.pg_addr, 0, BLK_PER_PG, data);
		data += USZRAM_PAGE_SIZE;
		blk_addr += BLK_PER_PG;
	}
	read_blk(l.pg_addr, blk_addr % BLK_PER_PG, l.blk_end - blk_addr, data);

	return 0;

}

int uszram_write_pg(uint_least32_t pg_addr, uint_least32_t pages,
		    const char data[static pages * USZRAM_PAGE_SIZE])
{
	if (pages == 0)
		return 0;
	if ((uint_least64_t)pg_addr + pages > USZRAM_PAGE_COUNT)
		return -1;

	const uint_least32_t pg_end = pg_addr + pages;
	for (; pg_addr != pg_end; ++pg_addr) {
		char compr_pg[MAX_NON_HUGE];
		write_pg(pg_addr, compr_pg, data);
		data += USZRAM_PAGE_SIZE;
	}

	return 0;
}

int uszram_write_blk(uint_least32_t blk_addr, uint_least32_t blocks,
		     const char data[static blocks * USZRAM_BLOCK_SIZE])
{
	if (blocks == 0)
		return 0;
	if ((uint_least64_t)blk_addr + blocks > USZRAM_BLOCK_COUNT)
		return -1;

	struct blkloop l = make_blkloop(blk_addr, blocks);
	const size_type offset = blk_addr % BLK_PER_PG;
	blocks = BLK_PER_PG - offset;
	char compr_pg[MAX_NON_HUGE];

	if (l.pg_addr != l.pg_last) {
		write_blk(l.pg_addr, offset, blocks, data, compr_pg);
		data += blocks * USZRAM_BLOCK_SIZE;
		blk_addr += blocks;
		++l.pg_addr;
	}
	for (; l.pg_addr != l.pg_last; ++l.pg_addr) {
		write_blk(l.pg_addr, 0, BLK_PER_PG, data, compr_pg);
		data += USZRAM_PAGE_SIZE;
		blk_addr += BLK_PER_PG;
	}
	write_blk(l.pg_addr, blk_addr % BLK_PER_PG, l.blk_end - blk_addr, data,
		  compr_pg);

	return 0;
}

int uszram_delete_pg(uint_least32_t pg_addr, uint_least32_t pages)
{
	if (pages == 0)
		return 0;
	if ((uint_least64_t)pg_addr + pages > USZRAM_PAGE_COUNT)
		return -1;

	const uint_least32_t pg_end = pg_addr + pages;
	for (; pg_addr != pg_end; ++pg_addr) {
		lock_as_writer(&pgtbl[pg_addr].lock);
		delete_pg(pgtbl + pg_addr);
		pgtbl[pg_addr].lock = 0;
	}

	return 0;
}

int uszram_delete_blk(uint_least32_t blk_addr, uint_least32_t blocks)
{
	if (blocks == 0)
		return 0;
	if ((uint_least64_t)blk_addr + blocks > USZRAM_BLOCK_COUNT)
		return -1;

	struct blkloop l = make_blkloop(blk_addr, blocks);
	size_type offset = blk_addr % BLK_PER_PG;
	blocks = BLK_PER_PG - offset;
	char compr_pg[MAX_NON_HUGE];

	if (l.pg_addr != l.pg_last) {
		delete_blk(l.pg_addr, offset, blocks, compr_pg);
		blk_addr += blocks;
		++l.pg_addr;
	}
	for (; l.pg_addr != l.pg_last; ++l.pg_addr) {
		delete_blk(l.pg_addr, 0, BLK_PER_PG, compr_pg);
		blk_addr += BLK_PER_PG;
	}
	delete_blk(l.pg_addr, blk_addr % BLK_PER_PG, l.blk_end - blk_addr,
		   compr_pg);

	return 0;
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

int uszram_pg_size(uint_least32_t pg_addr)
{
	if (pg_addr > (uint_least32_t)(USZRAM_PAGE_COUNT - 1))
		return -1;

	size_type size = sizeof (struct page);

	lock_as_reader(&pgtbl[pg_addr].lock);
	size += get_size(pgtbl + pg_addr);
	pgtbl[pg_addr].lock = 0;

	return size;
}

int uszram_pg_heap(uint_least32_t pg_addr)
{
	if (pg_addr > (uint_least32_t)(USZRAM_PAGE_COUNT - 1))
		return -1;

	size_type size = 0;

	lock_as_reader(&pgtbl[pg_addr].lock);
	size = get_size(pgtbl + pg_addr);
	pgtbl[pg_addr].lock = 0;

	return size;
}

uint_least64_t uszram_total_size(void)
{
	return sizeof pgtbl + stats.compr_data_size;
}

uint_least64_t uszram_total_heap(void)
{
	return stats.compr_data_size;
}

uint_least64_t uszram_pages_stored(void)
{
	return stats.pages_stored;
}

uint_least64_t uszram_huge_pages(void)
{
	return stats.huge_pages;
}

uint_least64_t uszram_num_compr(void)
{
	return stats.num_compr;
}

uint_least64_t uszram_failed_compr(void)
{
	return stats.failed_compr;
}
