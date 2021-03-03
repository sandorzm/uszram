#ifndef USZRAM_H
#define USZRAM_H


/* Change the next 4 definitions to configure the data store.
 *
 * Logical block size, the smallest unit that can be read or written, is
 * (1 << USZRAM_BLOCK_SHIFT) bytes. USZRAM_BLOCK_COUNT is the number of logical
 * blocks in the store.
 *
 * Page size, the unit in which data is compressed, is (1 << USZRAM_PAGE_SHIFT)
 * bytes. USZRAM_PAGE_SHIFT must not be less than USZRAM_BLOCK_SHIFT.
 *
 * USZRAM_PG_PER_LOCK adjusts lock granularity for multithreading. It is the
 * maximum number of pages that can be controlled by a single mutex lock.
 */
#define USZRAM_BLOCK_SHIFT  9
#define USZRAM_BLOCK_COUNT 32
#define USZRAM_PAGE_SHIFT  12
#define USZRAM_PG_PER_LOCK  2

#define USZRAM_BLOCK_SIZE (1 << USZRAM_BLOCK_SHIFT)
#define USZRAM_PAGE_SIZE  (1 << USZRAM_PAGE_SHIFT)


int uszram_init(void);
int uszram_read_blk (unsigned long blk_addr, char data[static USZRAM_BLOCK_SIZE]);
int uszram_read_pg  (unsigned long pg_addr,  char data[static USZRAM_PAGE_SIZE]);
int uszram_write_blk(unsigned long blk_addr, const char data[static USZRAM_BLOCK_SIZE]);
int uszram_write_pg (unsigned long pg_addr,  const char data[static USZRAM_PAGE_SIZE]);


#endif // USZRAM_H
