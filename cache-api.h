#ifndef CACHE_API_H
#define CACHE_API_H


#include "uszram-def.h"


#ifndef USZRAM_NO_CACHING
#  define RESTORE_BLK_ORDER(pg, d)      restore_blk_order ((pg)->cache_data, d)
#  define GET_ORDERED_BLKS(pg, b, s, d) get_ordered_blks  ((pg)->cache_data, \
							   b, s, d)
#  define DECOMPR_BYTE_COUNT(pg, b)     decompr_byte_count((pg)->cache_data, b)
#  define LOG_READ(pg, b)               log_read    (&(pg)->cache_data, b)
#  define UPDATE_CACHE(pg, d)           update_cache(&(pg)->cache_data, d)
#  define RESET_CACHE(pg)               reset_cache (&(pg)->cache_data)
#else
#  define RESTORE_BLK_ORDER(pg, d)
#  define GET_ORDERED_BLKS(pg, b, s, d)
#  define DECOMPR_BYTE_COUNT(pg, b)
#  define LOG_READ(pg, b)
#  define UPDATE_CACHE(pg, d)
#  define RESET_CACHE(pg)
#endif


struct cache_data;

/* restore_blk_order() arranges the blocks in 'data' in their original order
 * according to 'cache', putting any cached blocks back in their original
 * places.
 */
static inline void restore_blk_order(struct cache_data cache,
				     char data[static PAGE_SIZE]);

/* get_ordered_blks() gets the blocks specified by 'byte' from their (possibly
 * out-of-order) locations in src and reads them into dest in their original
 * order according to 'cache'.
 */
static inline void get_ordered_blks(struct cache_data cache,
				    struct range byte,
				    const char src[static PAGE_SIZE],
				    char dest[static BLOCK_SIZE]);

/* decompr_byte_count() returns the minimum number of bytes to decompress from
 * the beginning of the page in order to get all the blocks specified by blk.
 * (Without caching, this would simply be blk.offset + blk.count times the block
 * size.)
 */
static inline size_type decompr_byte_count(struct cache_data cache,
					   struct range blk);

/* log_read() updates metadata in 'cache' to reflect reads of blk.count blocks
 * starting at blk.offset blocks from the beginning of the corresponding page.
 */
static inline void log_read(struct cache_data *cache, struct range blk);

/* update_cache() rearranges the blocks in 'data' to cache currently popular
 * ones according to 'cache' and updates metadata in 'cache' to reflect this.
 */
static inline void update_cache(struct cache_data *cache,
				char data[static PAGE_SIZE]);

/* reset_cache() resets any metadata in 'cache' to reflect that nothing is
 * cached.
 */
static inline void reset_cache(struct cache_data *cache);


#endif // CACHE_API_H
