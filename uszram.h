#ifndef USZRAM_H
#define USZRAM_H


#include <stdint.h>


/* Change the next 3 definitions to configure data sizes.
 *
 * Logical block size, the smallest unit that can be read or written, is
 * (1u << USZRAM_BLOCK_SHIFT) bytes. USZRAM_BLOCK_SHIFT must be at least 0 and
 * at most 28 (or at most 14 if your implementation uses 16-bit int).
 *
 * Page size, the unit in which data is compressed, is (1u << USZRAM_PAGE_SHIFT)
 * bytes. USZRAM_PAGE_SHIFT must be at least USZRAM_BLOCK_SHIFT, at most
 * USZRAM_BLOCK_SHIFT + 8, and at most 28 (or at most 14 if your implementation
 * uses 16-bit int).
 *
 * USZRAM_BLOCK_COUNT is the number of logical blocks in the store. It must be
 * at least 1 and at most (1ull << 32).
 */
#define USZRAM_BLOCK_SHIFT  8u
#define USZRAM_PAGE_SHIFT  12u
#define USZRAM_BLOCK_COUNT (1ul << 24)

/* Change the next 3 definitions to select the memory allocator, compressor, and
 * caching strategy. The allocator uses standard malloc unless you link the
 * program with jemalloc (like cc *.c -ljemalloc).
 *
 * The first definition sets the memory allocation strategy:
 * - USZRAM_BASIC selects a basic strategy
 *
 * The second definition sets the compression library:
 * - USZRAM_ZAPI selects Matthew Dennerlein's Z API, an LZ4 modified to reduce
 *   compression work as much as possible and thus increase speed
 * - USZRAM_LZ4 selects plain LZ4
 * - USZRAM_ZSTD selects Zstandard
 *
 * The third definition sets the caching strategy. uszram can move frequently
 * read blocks to the beginning of the page to speed up future reads of those
 * blocks when Z API or LZ4 is used.
 * - USZRAM_LIST2_CACHE uses a recently-read list to cache 2 blocks in each page
 * - USZRAM_NO_CACHING disables caching
 */
#define USZRAM_BASIC
#define USZRAM_LZ4
#define USZRAM_LIST2_CACHE

/* Change the next 2 definitions to configure the handling of large pages.
 *
 * Compressed pages are limited to USZRAM_MAX_NHUGE_PERCENT of the page size.
 * Those that would be bigger ("huge" pages) are instead stored uncompressed,
 * occupying the full page size (what little space compression would save is not
 * deemed worth the overhead). USZRAM_MAX_NHUGE_PERCENT must be an integer at
 * least 100.0 / USZRAM_PAGE_SIZE and at most 100.
 *
 * uszram allows huge pages to accumulate USZRAM_HUGE_WAIT block updates before
 * compressing them again (so it doesn't waste too much time compressing
 * incompressible data). USZRAM_HUGE_WAIT must be at least 1 and at most 64.
 */
#define USZRAM_MAX_NHUGE_PERCENT 75u
#define USZRAM_HUGE_WAIT         64u

/* Change the next 2 definitions to configure locking.
 *
 * USZRAM_PG_PER_LOCK adjusts lock granularity for multithreading. It is the
 * maximum number of pages that can be controlled by a single lock. It must be
 * at least 1 and at most (1ull << 32).
 *
 * The second definition sets the lock type:
 * - USZRAM_STD_MTX selects a plain mutex from the C standard library
 * - USZRAM_PTH_MTX selects a plain mutex from the pthread library
 * - USZRAM_PTH_RW selects a readers-writer lock from the pthread library
 */
#define USZRAM_PG_PER_LOCK 4u
#define USZRAM_PTH_MTX


/* Don't change any of the following lines.
 */
#define USZRAM_BLOCK_SIZE (1u << USZRAM_BLOCK_SHIFT)
#define USZRAM_PAGE_SIZE  (1u << USZRAM_PAGE_SHIFT)
#define USZRAM_BLK_PER_PG (1u << (USZRAM_PAGE_SHIFT - USZRAM_BLOCK_SHIFT))
#define USZRAM_PAGE_COUNT (USZRAM_BLOCK_COUNT / USZRAM_BLK_PER_PG	\
			   + (USZRAM_BLOCK_COUNT % USZRAM_BLK_PER_PG != 0))


/* uszram_init() initializes locks and metadata to a valid state. It can safely
 * be called multiple times; however, it is not thread-safe. Must be called
 * before
 * - uszram_{read, write, delete}_*()
 * - uszram_pg_is_huge()
 * - uszram_pg_size()
 * - uszram_pg_heap()
 */
int uszram_init(void);

/* uszram_exit() deallocates memory and resets metadata to a valid state. It can
 * safely be called multiple times; however, it is not thread-safe. The
 * functions that cannot be called before uszram_init() also cannot be called
 * after uszram_exit() until uszram_init() is called again.
 */
int uszram_exit(void);

/* uszram_read_blk() reads 'blocks' blocks starting at blk_addr into 'data'.
 * 'data' must be at least 'blocks' blocks in size. Any nonexistent blocks are
 * read as all zeros. Thread-safe.
 */
int uszram_read_blk(uint_least32_t blk_addr, uint_least32_t blocks, char *data);

/* uszram_read_pg() reads 'pages' pages starting at pg_addr into 'data'. 'data'
 * must be at least 'pages' pages in size. Any nonexistent pages are read as all
 * zeros. Thread-safe.
 */
int uszram_read_pg(uint_least32_t pg_addr, uint_least32_t pages, char *data);

/* uszram_write_blk() writes 'blocks' blocks starting at blk_addr from 'data'.
 * 'data' must be at least 'blocks' blocks in size. Thread-safe.
 */
int uszram_write_blk(uint_least32_t blk_addr, uint_least32_t blocks,
		     const char *data);

/* uszram_write_blk_hint() is like uszram_write_blk() except that orig must be
 * at least 'blocks' blocks in size, and its first 'blocks' blocks must equal
 * the original data to be overwritten with nonexistent blocks replaced by all
 * zeros. That is, orig must be as if uszram_read_blk(blk_addr, blocks, orig)
 * were called.
 *
 * This hint speeds up the operation when using USZRAM_ZAPI.
 */
int uszram_write_blk_hint(uint_least32_t blk_addr, uint_least32_t blocks,
			  const char *data, const char *orig);

/* uszram_write_blk() writes 'pages' pages starting at pg_addr from 'data'.
 * 'data' must be at least 'pages' pages in size. Thread-safe.
 */
int uszram_write_pg(uint_least32_t pg_addr, uint_least32_t pages,
		    const char *data);

/* uszram_delete_blk() writes zeros over 'blocks' blocks starting at blk_addr,
 * increasing compressibility and saving space. If this makes a page empty and
 * USZRAM_ZAPI is defined, the page may be deallocated, further saving space.
 * Thread-safe.
 */
int uszram_delete_blk(uint_least32_t blk_addr, uint_least32_t blocks);

/* uszram_delete_pg() deallocates 'pages' pages starting at pg_addr.
 * Thread-safe.
 */
int uszram_delete_pg(uint_least32_t pg_addr, uint_least32_t pages);

/* uszram_delete_all() deallocates all USZRAM_PAGE_COUNT pages. Thread-safe.
 */
int uszram_delete_all(void);

/* uszram_pg_exists() returns whether the page at pg_addr has a heap allocation.
 * This is always true if it contains any nonzero data. Thread-safe.
 */
_Bool uszram_pg_exists(uint_least32_t pg_addr);

/* uszram_pg_is_huge() returns whether the page at pg_addr is incompressible and
 * stored as raw data. Thread-safe.
 */
_Bool uszram_pg_is_huge(uint_least32_t pg_addr);

/* uszram_pg_size() returns the number of stack + heap bytes representing the
 * page at pg_addr. Thread-safe.
 */
int uszram_pg_size(uint_least32_t pg_addr);

/* uszram_pg_heap() returns the number of bytes on the heap representing the
 * page at pg_addr. Thread-safe.
 */
int uszram_pg_heap(uint_least32_t pg_addr);

/* uszram_total_size() returns the number of stack + heap bytes representing the
 * entire data store, except for any heap data allocated by locks, which is
 * unknowable. Thread-safe.
 */
uint_least64_t uszram_total_size(void);

/* uszram_total_heap() returns the number of bytes on the heap representing the
 * entire data store, except locks, whose heap data is inscrutable.
 * Thread-safe.
 */
uint_least64_t uszram_total_heap(void);

/* uszram_pages_stored() returns the current number of pages that exist (see
 * uszram_pg_exists()). Thread-safe.
 */
uint_least64_t uszram_pages_stored(void);

/* uszram_huge_pages() returns the current number of incompressible, or huge,
 * pages (see uszram_pg_is_huge()). Thread-safe.
 */
uint_least64_t uszram_huge_pages(void);

/* uszram_num_compr() returns the number of calls to the compressor's
 * compression function(s) since the last uszram_exit() (or the start of the
 * program if there were none). Thread-safe.
 */
uint_least64_t uszram_num_compr(void);

/* uszram_failed_compr() returns the number of calls to the compressor's
 * compression function(s) since the last uszram_exit() (or the start of the
 * program if there were none) that compressed to more than
 * USZRAM_MAX_NHUGE_PERCENT of the page size. Thread-safe.
 */
uint_least64_t uszram_failed_compr(void);


#endif // USZRAM_H
