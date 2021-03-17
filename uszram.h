#ifndef USZRAM_H
#define USZRAM_H


#include <stdint.h>


/* Change the next 4 definitions to configure block size and locking.
 *
 * Logical block size, the smallest unit that can be read or written, is
 * (1 << USZRAM_BLOCK_SHIFT) bytes. USZRAM_BLOCK_SHIFT must be at least 0 and at
 * most 28 (or at most 14 if your implementation uses 16-bit int).
 *
 * USZRAM_BLK_ADDR_MAX is the maximum logical block address, or one fewer than
 * the number of logical blocks in the store. It must be at least 0 and at most
 * 2^32 - 1.
 *
 * Page size, the unit in which data is compressed, is (1 << USZRAM_PAGE_SHIFT)
 * bytes. USZRAM_PAGE_SHIFT must be at least USZRAM_BLOCK_SHIFT and at most 28
 * (or at most 14 if your implementation uses 16-bit int).
 *
 * USZRAM_PG_PER_LOCK adjusts lock granularity for multithreading. It is the
 * maximum number of pages that can be controlled by a single readers-writer
 * lock. It must be at least 1.
 */
#define USZRAM_BLOCK_SHIFT   9
#define USZRAM_BLK_ADDR_MAX 31
#define USZRAM_PAGE_SHIFT   12
#define USZRAM_PG_PER_LOCK   2

/* Change the next 2 defined symbols to configure the memory allocator and
 * compressor.
 *
 * The first definition sets the memory allocation engine:
 * - USZRAM_MALLOC selects standard malloc
 * - USZRAM_JEMALLOC selects jemalloc
 *
 * The second definition sets the compression library:
 * - USZRAM_LZ4 selects plain LZ4
 * - USZRAM_ZAPI selects Matthew Dennerlein's Z API, an LZ4 modified to reduce
 *   compression work as much as possible and thus increase speed
 */
#define USZRAM_JEMALLOC
#define USZRAM_ZAPI


/* Don't change any of the following lines.
 *
 * No casting needed here because block and page sizes are guaranteed by the
 * specifications above to fit into an int.
 */
#define USZRAM_BLOCK_SIZE (1 << USZRAM_BLOCK_SHIFT)
#define USZRAM_PAGE_SIZE  (1 << USZRAM_PAGE_SHIFT)


int uszram_init(void);
int uszram_read_blk (uint_least32_t blk_addr, char data[static USZRAM_BLOCK_SIZE]);
int uszram_read_pg  (uint_least32_t pg_addr,  char data[static USZRAM_PAGE_SIZE]);
int uszram_write_blk(uint_least32_t blk_addr, const char data[static USZRAM_BLOCK_SIZE]);
int uszram_write_pg (uint_least32_t pg_addr,  const char data[static USZRAM_PAGE_SIZE]);


#endif // USZRAM_H
