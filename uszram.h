#ifndef USZRAM_H
#define USZRAM_H


#include <stdint.h>


/* Change the next 4 definitions to configure the data store.
 *
 * Logical block size, the smallest unit that can be read or written, is
 * (1 << USZRAM_BLOCK_SHIFT) bytes. USZRAM_BLOCK_SHIFT must be at least 0 and at
 * most 28, or at most 15 if your implementation uses 16-bit int.
 *
 * USZRAM_BLOCK_ADDR_MAX is the maximum logical block address, or one fewer than
 * the number of logical blocks in the store. It must be at least 0 and at most
 * 2^32 - 1.
 *
 * Page size, the unit in which data is compressed, is (1 << USZRAM_PAGE_SHIFT)
 * bytes. USZRAM_PAGE_SHIFT must be at least USZRAM_BLOCK_SHIFT and at most 28,
 * or at most 15 if your implementation uses 16-bit int.
 *
 * USZRAM_PG_PER_LOCK adjusts lock granularity for multithreading. It is the
 * maximum number of pages that can be controlled by a single readers-writer
 * lock. It must be at least 1.
 */
#define USZRAM_BLOCK_SHIFT   9
#define USZRAM_BLK_ADDR_MAX 31
#define USZRAM_PAGE_SHIFT   12
#define USZRAM_PG_PER_LOCK   2

#define USZRAM_BLOCK_SIZE (1 << USZRAM_BLOCK_SHIFT)
#define USZRAM_PAGE_SIZE  (1 << USZRAM_PAGE_SHIFT)


int uszram_init(void);
int uszram_read_blk (int_least32_t blk_addr, char data[static USZRAM_BLOCK_SIZE]);
int uszram_read_pg  (int_least32_t pg_addr,  char data[static USZRAM_PAGE_SIZE]);
int uszram_write_blk(int_least32_t blk_addr, const char data[static USZRAM_BLOCK_SIZE]);
int uszram_write_pg (int_least32_t pg_addr,  const char data[static USZRAM_PAGE_SIZE]);


#endif // USZRAM_H
