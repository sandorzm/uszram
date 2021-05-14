#ifndef CACHE_API_H
#define CACHE_API_H


#include "uszram-def.h"


#ifndef USZRAM_NO_CACHING
#  define UNCACHE_PG(pg, d)       uncache_pg    ( (pg)->cache_data, d)
#  define CACHE_READ(pg, b, s, d) cache_read    ( (pg)->cache_data, b, s, d)
#  define BYTES_NEEDED(pg, b)     bytes_needed  ( (pg)->cache_data, b)
#  define CACHE_LOG_READ(pg, b)   cache_log_read(&(pg)->cache_data, b)
#  define CACHE_PG(pg, d)         cache_pg      (&(pg)->cache_data, d)
#  define CACHE_RESET(pg)         cache_reset   (&(pg)->cache_data)
#  define CACHE_INIT(pg)          cache_init    (&(pg)->cache_data)
#else
#  define UNCACHE_PG(pg, d)
#  define CACHE_READ(pg, b, s, d)
#  define BYTES_NEEDED(pg, b)
#  define CACHE_LOG_READ(pg, b)
#  define CACHE_PG(pg, d)
#  define CACHE_RESET(pg)
#  define CACHE_INIT(pg)
#endif


struct cache_data;

/* uncache_pg() puts the blocks in 'data' in their original order according to
 * 'cache', uncaching any cached blocks.
 */
static inline void uncache_pg(const struct cache_data cache,
			      char data[static PAGE_SIZE]);

/* cache_read() reads the blocks specified by 'byte' into dest from their
 * (possibly out-of-order) locations in src.
 */
static inline void cache_read(const struct cache_data cache, struct range byte,
			      const char src[static PAGE_SIZE],
			      char dest[static BLOCK_SIZE]);

/* bytes_needed() returns the minimum number of bytes at the beginning of the
 * page that contain the (possibly out-of-order) locations of all the blocks
 * specified by blk.
 */
static inline size_type bytes_needed(const struct cache_data cache,
				     const struct range blk);

/* cache_log_read() updates 'cache' to reflect reads of blk.count blocks
 * starting at blk.offset blocks from the beginning of the corresponding page.
 */
static inline void cache_log_read(struct cache_data *cache, struct range blk);

/* cache_pg() rearranges the blocks in 'data' to cache currently popular ones
 * according to 'cache' and updates 'cache' to reflect this.
 */
static inline void cache_pg(struct cache_data *cache,
			    char data[static PAGE_SIZE]);

/* cache_reset() resets any metadata in 'cache' to reflect that nothing is
 * cached.
 */
static inline void cache_reset(struct cache_data *cache);

/* cache_init() is like reset_cache() but assumes that *cache is all zeros, as
 * when it's statically initialized at uszram startup.
 */
static inline void cache_init(struct cache_data *cache);


#endif // CACHE_API_H
