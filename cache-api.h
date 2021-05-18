#ifndef CACHE_API_H
#define CACHE_API_H


#include "uszram-def.h"


#ifndef USZRAM_NO_CACHING
#  define UNCACHE_PG(pg, d)       uncache_pg    ( (pg)->cache_data, d)
#  define CACHE_READ(pg, b, s, d) cache_read    ( (pg)->cache_data, b, s, d)
#  define GET_PG_RANGES(pg, b, r) get_pg_ranges ( (pg)->cache_data, b, r)
#  define BYTES_NEEDED(pg, b, y)  bytes_needed  ( (pg)->cache_data, b)
#  define CACHE_LOG_READ(pg, b)   cache_log_read(&(pg)->cache_data, b)
#  define CACHE_PG_COPY(pg, s, d) cache_pg_copy (&(pg)->cache_data, s, d)
#  define CACHE_PG(pg, d)         cache_pg      (&(pg)->cache_data, d)
#  define CACHE_RESET(pg)         cache_reset   (&(pg)->cache_data)
#  define CACHE_INIT(pg)          cache_init    (&(pg)->cache_data)
#else
#  define UNCACHE_PG(pg, d)
#  define CACHE_READ(pg, b, s, d)
#  define GET_PG_RANGES(pg, b, r) 1
#  define BYTES_NEEDED(pg, b, y)  (y.offset + y.count)
#  define CACHE_LOG_READ(pg, b)
#  define CACHE_PG_COPY(pg, s, d)
#  define CACHE_PG(pg, d)
#  define CACHE_RESET(pg)
#  define CACHE_INIT(pg)
#endif

/* MAX_PG_RANGES expands to the maximum number of disjoint ranges that a single
 * range within a page may have to be split into to make the real positions of
 * the ranges contiguous in the (possibly out-of-order) page.
 */

struct cache_data;

/* uncache_pg() puts the blocks in 'data' in their original order according to
 * 'cache', uncaching any cached blocks.
 */
static inline void uncache_pg(const struct cache_data cache,
			      char data[static PAGE_SIZE]);

/* cache_read() reads the blocks specified by 'byte' into dest from their
 * (possibly out-of-order) locations in src.
 */
static inline void cache_read(const struct cache_data cache, ByteRange byte,
			      const char src[static PAGE_SIZE],
			      char dest[static BLOCK_SIZE]);

/* get_pg_ranges() splits blk into a list of ranges written to ret whose real
 * positions in the (possibly out-of-order) page are contiguous. ret must have
 * at least MAX_PG_RANGES elements. It returns the number of such ranges, which
 * can't be more than MAX_PG_RANGES.
 */
static inline unsigned char get_pg_ranges(const struct cache_data cache,
					  BlkRange blk, BlkRange *ret);

/* bytes_needed() returns the minimum number of bytes at the beginning of the
 * page that contain the (possibly out-of-order) locations of all the blocks
 * specified by blk.
 */
static inline size_type bytes_needed(const struct cache_data cache,
				     const BlkRange blk);

/* cache_log_read() updates 'cache' to reflect reads of blk.count blocks
 * starting at blk.offset blocks from the beginning of the corresponding page.
 */
static inline void cache_log_read(struct cache_data *cache, BlkRange blk);

/* cache_pg() rearranges the blocks in 'data' to cache currently popular ones
 * according to 'cache' and updates 'cache' to reflect this.
 */
static inline void cache_pg(struct cache_data *cache,
			    char data[static PAGE_SIZE]);

/* cache_pg_copy() is like cache_pg() but doesn't modify src, instead writing
 * the rearranged blocks to dest.
 */
static inline void cache_pg_copy(struct cache_data *cache,
				 const char src[static PAGE_SIZE],
				 char dest[static PAGE_SIZE]);

/* cache_reset() resets any metadata in 'cache' to reflect that nothing is
 * cached.
 */
static inline void cache_reset(struct cache_data *cache);

/* cache_init() is like reset_cache() but assumes that *cache is all zeros, as
 * when it's statically initialized at uszram startup.
 */
static inline void cache_init(struct cache_data *cache);


#endif // CACHE_API_H
