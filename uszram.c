#include <string.h>
#include <stdatomic.h>

#include "allocators/uszram-basic.h"

#ifdef USZRAM_LZ4
#  include "compressors/uszram-lz4.h"
#elif defined USZRAM_ZSTD
#  include "compressors/uszram-zstd.h"
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


static atomic_bool initialized;
static struct page pgtbl[USZRAM_PAGE_COUNT];
static struct lock lktbl[LOCK_COUNT];
static struct {
	atomic_uint_least64_t  compr_data_size,	// Total heap data except locks
			       pages_stored,	// # of pages currently stored
			       huge_pages,	// # of huge pages
			       num_compr,	// # of compression attempts
			       failed_compr;	// Attempts resulting in huge pages
} stats;

struct request {
	union {
		uint_least32_t  pg_addr,
				blk_addr;
	};
	union {
		uint_least32_t  count,
				lk_addr;
	};
	union {
		char           *rdata;
		const char     *wdata;
	};
	const char             *orig;
};

typedef int blkfn_type(const struct request, const size_t, const struct range);
typedef int  pgfn_type(const struct request, const size_t);

static int read_pg(const struct request req, const size_t data_offset)
{
	const struct page *pg = pgtbl + req.pg_addr;
	struct lock *lk = lktbl + req.lk_addr;
	char *const data = req.rdata + data_offset;
	int ret = 0;

	if (pg->data == NULL) {
		memset(data, 0, PAGE_SIZE);
		return ret;
	}

	lock_as_reader(lk);
	if (is_huge(pg))
		memcpy(data, pg->data, PAGE_SIZE);
	else
		ret = decompress(pg, PAGE_SIZE, data);
	unlock_as_reader(lk);

	return ret;
}

static int read_blk(const struct request req, const size_t data_offset,
		    const struct range blk)
{
	const struct range byte = {.offset = blk.offset * BLOCK_SIZE,
				   .count  = blk.count  * BLOCK_SIZE};
	const struct page *pg = pgtbl + req.pg_addr;
	struct lock *lk = lktbl + req.lk_addr;
	char *data = req.rdata + data_offset;
	int ret = 0;

	if (pg->data == NULL) {
		memset(data, 0, byte.count);
		return ret;
	}

	lock_as_reader(lk);
	if (is_huge(pg)) {
		memcpy(data, pg->data + byte.offset, byte.count);
	} else {
		char raw_pg[PAGE_SIZE];
		ret = decompress(pg, byte.offset + byte.count, raw_pg);
		if (ret >= 0)
			memcpy(data, raw_pg + byte.offset, byte.count);
	}
	unlock_as_reader(lk);

	return ret;
}

static size_type write_pg_common(struct page *pg, size_type compr_size,
				 const char compr_pg[static MAX_NON_HUGE],
				 const char raw_pg[static PAGE_SIZE])
{
	if (compr_size == 0) {
		++stats.failed_compr;
		compr_size = PAGE_SIZE;
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

static int write_pg(const struct request req, const size_t data_offset)
{
	struct page *pg = pgtbl + req.pg_addr;
	struct lock *lk = lktbl + req.lk_addr;
	const char *const data = req.wdata + data_offset;
	char compr_pg[MAX_NON_HUGE];

	const size_type new_size = compress(data, compr_pg);
	++stats.num_compr;

	lock_as_writer(lk);
	stats.pages_stored += pg->data == NULL;
	write_pg_common(pg, new_size, compr_pg, data);
	unlock_as_writer(lk);

	return new_size;
}

static inline size_type write_blk_helper(struct page *pg,
					 const char raw_pg[static PAGE_SIZE])
{
	char compr_pg[MAX_NON_HUGE];
	const size_type new_size = compress(raw_pg, compr_pg);
	++stats.num_compr;
	return write_pg_common(pg, new_size, compr_pg, raw_pg);
}

static int write_blk(const struct request req, const size_t data_offset,
		     const struct range blk)
{
	const struct range byte = {.offset = blk.offset * BLOCK_SIZE,
				   .count  = blk.count  * BLOCK_SIZE};
	struct page *pg = pgtbl + req.pg_addr;
	struct lock *lk = lktbl + req.lk_addr;
	const char *const data = req.wdata + data_offset;
	int ret = 0;

	lock_as_writer(lk);
	if (pg->data == NULL) {
		++stats.pages_stored;
		char raw_pg[PAGE_SIZE] = {0};
		memcpy(raw_pg + byte.offset, data, byte.count);
		ret = write_blk_helper(pg, raw_pg);
		unlock_as_writer(lk);
		return ret;
	}

	if (is_huge(pg)) {
		memcpy(pg->data + byte.offset, data, byte.count);
		if (needs_recompress(pg, blk.count))
			ret = write_blk_helper(pg, pg->data);
	} else {
		char raw_pg[PAGE_SIZE];
		const int old_size = get_size(pg);
		const char *const orig = req.orig
					 ? req.orig + data_offset
					 : NULL;
		if (read_modify(pg, blk, data, orig, raw_pg)) {
			stats.compr_data_size -= free_reachable(pg);
			ret = write_blk_helper(pg, raw_pg);
		} else {
			stats.compr_data_size += (int)get_size(pg) - old_size;
		}
	}
	unlock_as_writer(lk);
	return 0;
}

static void delete_pg_common(struct page *pg)
{
	--stats.pages_stored;
	if (is_huge(pg))
		--stats.huge_pages;
	else
		stats.compr_data_size -= free_reachable(pg);
	stats.compr_data_size += maybe_reallocate(pg, get_size_primary(pg), 0);
	write_compressed(pg, 0, NULL);
}

static int delete_pg(const struct request req, const size_t data_offset)
{
	(void)data_offset;
	struct page *const pg = pgtbl + req.pg_addr;

	if (pg->data == NULL)
		return 0;

	struct lock *const lk = lktbl + req.lk_addr;
	lock_as_writer(lk);
	delete_pg_common(pg);
	unlock_as_writer(lk);

	return 0;
}

static int delete_blk(const struct request req, const size_t data_offset,
		      const struct range blk)
{
	(void)data_offset;
	const struct range byte = {.offset = blk.offset * BLOCK_SIZE,
				   .count  = blk.count  * BLOCK_SIZE};
	struct page *pg = pgtbl + req.pg_addr;
	struct lock *lk = lktbl + req.lk_addr;
	int ret = 0;

	if (pg->data == NULL)
		return ret;

	lock_as_writer(lk);
	if (is_huge(pg)) {
		memset(pg->data + byte.offset, 0, byte.count);
		if (needs_recompress(pg, blk.count))
			ret = write_blk_helper(pg, pg->data);
	} else {
		char raw_pg[PAGE_SIZE];
		if (read_delete(pg, blk, raw_pg)) {
			stats.compr_data_size -= free_reachable(pg);
			ret = write_blk_helper(pg, raw_pg);
		} else {
			delete_pg_common(pg);
		}
	}
	unlock_as_writer(lk);
	return ret;
}

static int do_blk_request(blkfn_type blkfn, pgfn_type pgfn,
			  const struct request *req)
{
	if (req->count == 0)
		return 0;
	const uint_least64_t blk_end = req->blk_addr + req->count;
	if (blk_end > USZRAM_BLOCK_COUNT)
		return -1;

	const uint_least32_t pg_last
		= (uint_least32_t)(blk_end - 1u) / BLK_PER_PG;
	struct request pgreq = {
		.pg_addr = req->blk_addr / BLK_PER_PG,
		.lk_addr = req->blk_addr / BLK_PER_PG / PG_PER_LOCK,
		.rdata   = req->rdata,
		.orig    = req->orig,
	};
	const size_type first_blk = req->blk_addr % BLK_PER_PG;

	if (pgreq.pg_addr == pg_last) {
		if (first_blk == 0 && req->count == BLK_PER_PG)
			return pgfn(pgreq, 0);
		return blkfn(pgreq, 0, RANGE(first_blk, req->count));
	}

	const size_type first_count = BLK_PER_PG - first_blk;
	blkfn(pgreq, 0, RANGE(first_blk, first_count));
	++pgreq.pg_addr;

	size_type        data_offset = first_count * BLOCK_SIZE;
	const uint_least32_t lk_last = pg_last / PG_PER_LOCK;
	uint_least32_t       pg_next = pgreq.lk_addr * PG_PER_LOCK;
	for (; pgreq.lk_addr != lk_last; ++pgreq.lk_addr) {
		pg_next += PG_PER_LOCK;
		for (; pgreq.pg_addr != pg_next; ++pgreq.pg_addr) {
			pgfn(pgreq, data_offset);
			data_offset += PAGE_SIZE;
		}
	}
	for (; pgreq.pg_addr != pg_last; ++pgreq.pg_addr) {
		pgfn(pgreq, data_offset);
		data_offset += PAGE_SIZE;
	}
	const unsigned char last_count = blk_end % BLK_PER_PG;
	if (last_count)
		blkfn(pgreq, data_offset, RANGE(0, last_count));
	else
		pgfn(pgreq, data_offset);
	return 0;
}

static int do_pg_request(pgfn_type fn, const struct request *req)
{
	if (req->count == 0)
		return 0;
	const uint_least64_t pg_end = req->pg_addr + req->count;
	if (pg_end > USZRAM_PAGE_COUNT)
		return -1;

	struct request pgreq = {
		.pg_addr = req->pg_addr,
		.lk_addr = req->pg_addr / PG_PER_LOCK,
		.rdata   = req->rdata,
	};

	size_t data_offset = 0;
	const uint_least32_t lk_last
		= (uint_least32_t)(pg_end - 1u) / PG_PER_LOCK;
	uint_least32_t pg_next = pgreq.lk_addr * PG_PER_LOCK;
	for (; pgreq.lk_addr != lk_last; ++pgreq.lk_addr) {
		pg_next += PG_PER_LOCK;
		for (; pgreq.pg_addr != pg_next; ++pgreq.pg_addr) {
			fn(pgreq, data_offset);
			data_offset += PAGE_SIZE;
		}
	}
	for (; pgreq.pg_addr != pg_end; ++pgreq.pg_addr) {
		fn(pgreq, data_offset);
		data_offset += PAGE_SIZE;
	}
	return 0;
}


////////////////////////////////////////////////////////////////////////////////
//                              BEGIN PUBLIC API                              //
////////////////////////////////////////////////////////////////////////////////


int uszram_delete_all(void)
{
	uint_least64_t pg_addr = 0;
	uint_least32_t pg_next = 0, lk_addr = 0, lk_last = LOCK_COUNT - 1;
	for (; lk_addr != lk_last; ++lk_addr) {
		pg_next += PG_PER_LOCK;
		lock_as_writer(lktbl + lk_addr);
		for (; pg_addr != pg_next; ++pg_addr)
			if (pgtbl[pg_addr].data)
				delete_pg_common(pgtbl + pg_addr);
		unlock_as_writer(lktbl + lk_addr);
	}
	lock_as_writer(lktbl + lk_addr);
	for (; pg_addr != USZRAM_PAGE_COUNT; ++pg_addr)
		if (pgtbl[pg_addr].data)
			delete_pg_common(pgtbl + pg_addr);
	unlock_as_writer(lktbl + lk_addr);
	return 0;
}

int uszram_init(void)
{
	if (initialized)
		return -1;
	for (uint_least64_t i = 0; i != LOCK_COUNT; ++i)
		initialize_lock(lktbl + i);
	initialized = 1;
	return 0;
}

int uszram_exit(void)
{
	if (!initialized)
		return -1;
	initialized = 0;
	for (uint_least64_t i = 0; i != LOCK_COUNT; ++i)
		destroy_lock(lktbl + i);
	for (uint_least64_t i = 0; i != USZRAM_PAGE_COUNT; ++i)
		if (pgtbl[i].data)
			delete_pg_common(pgtbl + i);
	stats.num_compr = stats.failed_compr = 0;
	return 0;
}

int uszram_read_pg(uint_least32_t pg_addr, uint_least32_t pages, char *data)
{
	return do_pg_request(read_pg, &(struct request){
			.pg_addr = pg_addr,
			.count = pages,
			.rdata = data,
		});
}

int uszram_read_blk(uint_least32_t blk_addr, uint_least32_t blocks, char *data)
{
	return do_blk_request(read_blk, read_pg, &(struct request){
			.blk_addr = blk_addr,
			.count = blocks,
			.rdata = data,
		});
}

int uszram_write_pg(uint_least32_t pg_addr, uint_least32_t pages,
		    const char data[static pages * PAGE_SIZE])
{
	return do_pg_request(write_pg, &(struct request){
			.pg_addr = pg_addr,
			.count = pages,
			.wdata = data,
		});
}

int uszram_write_blk_hint(uint_least32_t blk_addr, uint_least32_t blocks,
			  const char *data, const char *orig)
{
	return do_blk_request(write_blk, write_pg, &(struct request){
			.blk_addr = blk_addr,
			.count = blocks,
			.wdata = data,
			.orig = orig,
		});
}

int uszram_write_blk(uint_least32_t blk_addr, uint_least32_t blocks,
		     const char *data)
{
	return uszram_write_blk_hint(blk_addr, blocks, data, NULL);
}

int uszram_delete_pg(uint_least32_t pg_addr, uint_least32_t pages)
{
	return do_pg_request(delete_pg, &(struct request){
			.pg_addr = pg_addr,
			.count = pages,
		});
}

int uszram_delete_blk(uint_least32_t blk_addr, uint_least32_t blocks)
{
	return do_blk_request(delete_blk, delete_pg, &(struct request){
			.blk_addr = blk_addr,
			.count = blocks,
		});
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
	struct lock *lk = lktbl + pg_addr / PG_PER_LOCK;

	lock_as_reader(lk);
	size += get_size(pgtbl + pg_addr);
	unlock_as_reader(lk);

	return size;
}

int uszram_pg_heap(uint_least32_t pg_addr)
{
	if (pg_addr > (uint_least32_t)(USZRAM_PAGE_COUNT - 1))
		return -1;

	size_type size;
	struct lock *lk = lktbl + pg_addr / PG_PER_LOCK;

	lock_as_reader(lk);
	size = get_size(pgtbl + pg_addr);
	unlock_as_reader(lk);

	return size;
}

uint_least64_t uszram_total_size(void)
{
	return sizeof pgtbl + sizeof lktbl + stats.compr_data_size;
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
