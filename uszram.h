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

/* Change the next 3 definitions to select the memory allocator and compressor.
 * The allocator uses standard malloc unless you link the program with jemalloc
 * (like cc *.c -ljemalloc).
 *
 * The second definition sets the memory allocation strategy:
 * - USZRAM_BASIC selects a basic strategy
 *
 * The third definition sets the compression library:
 * - USZRAM_ZAPI selects Matthew Dennerlein's Z API, an LZ4 modified to reduce
 *   compression work as much as possible and thus increase speed
 * - USZRAM_LZ4 selects plain LZ4
 * - USZRAM_ZSTD selects Zstandard
 */
#define USZRAM_BASIC
#define USZRAM_LZ4

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

/* Each page is controlled by a lock for thread safety. With many pages, threads
 * are unlikely to conflict, so only unfair spinlocks are supported. Change the
 * next definition to set the lock type:
 * - USZRAM_MUTEX selects a mutex
 * - USZRAM_RWLOCK selects a readers-writer lock
 */
#define USZRAM_MUTEX


/* Don't change any of the following lines.
 */
#define USZRAM_BLOCK_SIZE (1u << USZRAM_BLOCK_SHIFT)
#define USZRAM_PAGE_SIZE  (1u << USZRAM_PAGE_SHIFT)
#define USZRAM_BLK_PER_PG (1u << (USZRAM_PAGE_SHIFT - USZRAM_BLOCK_SHIFT))
#define USZRAM_PAGE_COUNT (USZRAM_BLOCK_COUNT / USZRAM_BLK_PER_PG	\
			   + (USZRAM_BLOCK_COUNT % USZRAM_BLK_PER_PG != 0))


int uszram_init(void);
int uszram_exit(void);

int uszram_read_pg(uint_least32_t pg_addr, uint_least32_t pages,
		   char data[static pages * USZRAM_PAGE_SIZE]);
int uszram_read_blk(uint_least32_t blk_addr, uint_least32_t blocks,
		    char data[static blocks * USZRAM_BLOCK_SIZE]);
int uszram_write_pg(uint_least32_t pg_addr, uint_least32_t pages,
		    const char data[static pages * USZRAM_PAGE_SIZE]);
int uszram_write_blk(uint_least32_t blk_addr, uint_least32_t blocks,
		     const char data[static blocks * USZRAM_BLOCK_SIZE]);
int uszram_delete_pg(uint_least32_t pg_addr, uint_least32_t pages);
int uszram_delete_blk(uint_least32_t blk_addr, uint_least32_t blocks);
int uszram_delete_all(void);

_Bool uszram_pg_exists(uint_least32_t pg_addr);
_Bool uszram_pg_is_huge(uint_least32_t pg_addr);
int uszram_pg_size(uint_least32_t pg_addr);
int uszram_pg_heap(uint_least32_t pg_addr);

uint_least64_t uszram_total_size(void);
uint_least64_t uszram_total_heap(void);
uint_least64_t uszram_pages_stored(void);
uint_least64_t uszram_huge_pages(void);
uint_least64_t uszram_num_compr(void);
uint_least64_t uszram_failed_compr(void);


#endif // USZRAM_H
