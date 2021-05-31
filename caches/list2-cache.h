/* list2-cache.h uses a heuristic to keep two of the most frequently read blocks
 * at the beginning of each page ("cache" them) so that reading them requires
 * less decompression. struct cache_data is a list of six block offsets that
 * stores all relevant information. The blocks are rearranged to put the cached
 * ones at the beginning whenever the page is compressed.
 *
 * The six block offsets are the two that are currently cached (cur0, cur1), the
 * two that will be cached on the next recompression (next0, next1), and two
 * candidates that may replace the next blocks if read frequently enough (cand0,
 * cand1). The next and cand pairs are ordered by recency: next0 is more recent
 * than next1, and cand0 is more recent than cand1. Thus, whenever a new block
 * enters the next or cand lists, it goes into next0 or cand0, respectively,
 * displacing what was there previously. And when next1 or cand1 is read, it is
 * swapped with next0 or cand0, respectively.
 *
 * When uszram starts up, the struct is initialized to {.cur0 = 0, .cur1 = 1,
 * .next0 = 0, .next1 = 1}, with everything else zero. This represents that the
 * page is in its natural order and will stay that way on the next
 * recompression. When a block is read, it goes into cand0, and if the block
 * equals cur1 (1 in this case), next0 and next1 are swapped to maintain recency
 * order. When another, different block is read, it goes into cand0 again,
 * moving the value of cand0 to cand1. (Same as before if the block is cur1.)
 * When the next new block is read, cand1 is kicked out, and so on. As long as
 * new blocks not already in cand keep being read, they just cycle through, with
 * cand containing the two most recent ones.
 *
 * However, if a block already in cand is read, and then again before both next
 * blocks are read, it is promoted to next. When it enters cand as cand0, it
 * gets a two-bit counter cand0count that starts at 00. If it's still in cand
 * the next time it's read, the two cands swap and the two counts swap if the
 * block got demoted to cand1, and cand0count becomes 11 (binary). Now, we see
 * which comes first: the candidate is read again, or both next blocks are read.
 * So if next0 is read, bit 1 of the counter is cleared (&= 1), and if next1 is
 * read, bit 0 is cleared, and then the two nexts swap and the two bits of the
 * counter swap (counter >>= 1 does all of this). When the candidate is read
 * again, check its counter: if it's zero, both next blocks were read;
 * otherwise, promote the candidate. Which next block does it replace? The one
 * that wasn't read and didn't clear its bit of the counter. (If neither
 * next block was read, one is chosen arbitrarily.) If it replaces next1, the
 * two nexts are swapped, because the promoted candidate was just read.
 *
 * With this scheme, if just one next block falls out of use, it is quickly
 * replaced, provided there is a better candidate. Likewise if both next blocks
 * fall out of use.
 */

#ifndef LIST2_CACHE_H
#define LIST2_CACHE_H


#include <string.h>

#include "../cache-api.h"


#define BLKPPG_SHIFT  (USZRAM_PAGE_SHIFT - USZRAM_BLOCK_SHIFT)
#define MAX_PG_RANGES 5


struct cache_data {
	unsigned  cur0      :BLKPPG_SHIFT,
		  cur1      :BLKPPG_SHIFT,
		  next0     :BLKPPG_SHIFT,
		  next1     :BLKPPG_SHIFT,
		  cand0     :BLKPPG_SHIFT,
		  cand1     :BLKPPG_SHIFT,
		  cand0count:2,
		  cand1count:2;
};

static inline void uncache_pg(const struct cache_data cache,
			      char data[static PAGE_SIZE])
{
	const _Bool min_loc = cache.cur1 < cache.cur0;
	const size_type cached[] = {cache.cur0 * BLOCK_SIZE,
				    cache.cur1 * BLOCK_SIZE};
	char max_data[BLOCK_SIZE];
	if (cache.cur0 == 0) {
		if (cache.cur1 == 1)
			return;
		data += BLOCK_SIZE;
		memcpy(max_data, data, BLOCK_SIZE);
	} else if (cache.cur1 > 1) {
		char min_data[BLOCK_SIZE];
		memcpy(min_data, data + BLOCK_SIZE *  min_loc, BLOCK_SIZE);
		memcpy(max_data, data + BLOCK_SIZE * !min_loc, BLOCK_SIZE);
		memmove(data, data + 2 * BLOCK_SIZE, cached[min_loc]);
		data += cached[min_loc];
		memcpy(data, min_data, BLOCK_SIZE);
		data += BLOCK_SIZE;
	} else {
		memcpy(max_data, data, BLOCK_SIZE);
		char *temp = data;
		data += (1 + cache.cur1) * BLOCK_SIZE;
		memcpy(temp, data, BLOCK_SIZE);
	}
	const size_type count = cached[!min_loc] - cached[min_loc] - BLOCK_SIZE;
	memmove(data, data + BLOCK_SIZE, count);
	memcpy(data + count, max_data, BLOCK_SIZE);
}

static inline void cache_read(const struct cache_data cache, ByteRange byte,
			      const char src[static PAGE_SIZE],
			      char dest[static BLOCK_SIZE])
{
	_Bool min_loc = cache.cur1 < cache.cur0;
	const size_type cached[] = {cache.cur0 * BLOCK_SIZE,
				    cache.cur1 * BLOCK_SIZE};
	const size_type offsets[] = {
		cached[ min_loc],
		cached[!min_loc],
		byte.offset + byte.count,
	};
	for (unsigned char i = 0; i < sizeof offsets / sizeof *offsets; ++i) {
		if (byte.offset <= offsets[i]) {
			size_type to_next = offsets[i] - byte.offset;
			const size_type src_offset
				= (2 - i) * BLOCK_SIZE + byte.offset;
			if (to_next >= byte.count) {
				memcpy(dest, src + src_offset, byte.count);
				return;
			}
			memcpy(dest, src + src_offset, to_next);
			dest += to_next;
			memcpy(dest, src + BLOCK_SIZE * min_loc, BLOCK_SIZE);
			dest += BLOCK_SIZE;
			to_next     += BLOCK_SIZE;
			byte.offset += to_next;
			byte.count  -= to_next;
		}
		min_loc = !min_loc;
	}
}

static inline unsigned char get_pg_ranges(const struct cache_data cache,
					  BlkRange blk,
					  BlkRange ret[static MAX_PG_RANGES])
{
	_Bool min_loc = cache.cur1 < cache.cur0;
	const uint_least16_t cached[] = {cache.cur0, cache.cur1};
	const uint_least16_t offsets[] = {
		cached[ min_loc],
		cached[!min_loc],
		blk.offset + blk.count,
	};
	unsigned char ret_pos = 0;
	for (unsigned char i = 0; i < sizeof offsets / sizeof *offsets; ++i) {
		if (blk.count == 0)
			return ret_pos;
		if (blk.offset <= offsets[i]) {
			size_type to_next = offsets[i] - blk.offset;
			const uint_least16_t start = 2 - i + blk.offset;
			if (to_next >= blk.count) {
				ret[ret_pos] = BLRNG(start, blk.count);
				if (ret_pos && ret[ret_pos - 1].offset
					       + ret[ret_pos - 1].count
					       == start)
					ret[ret_pos - 1].count += blk.count;
				else
					++ret_pos;
				return ret_pos;
			}
			if (to_next) {
				ret[ret_pos] = BLRNG(start, to_next);
				if (ret_pos && ret[ret_pos - 1].offset
					       + ret[ret_pos - 1].count
					       == start)
					ret[ret_pos - 1].count += to_next;
				else
					++ret_pos;
			}
			ret[ret_pos] = BLRNG(min_loc, 1);
			if (ret_pos && ret[ret_pos - 1].offset
				       + ret[ret_pos - 1].count == min_loc)
				++ret[ret_pos - 1].count;
			else
				++ret_pos;
			++to_next;
			blk.offset += to_next;
			blk.count  -= to_next;
		}
		min_loc = !min_loc;
	}
	return ret_pos;
}

static inline size_type bytes_needed(const struct cache_data cache,
				     const BlkRange blk)
{
	if (blk.count <= 2) {
		unsigned char found    = 0,
			      cached[] = {cache.cur0, cache.cur1};
		for (unsigned char i = 0;
		     i < sizeof cached / sizeof *cached; ++i) {
			for (unsigned short j = 0; j < blk.count; ++j)
				if (blk.offset + j == cached[i])
					++found;
			if (found == blk.count)
				return (i + 1) * BLOCK_SIZE;
		}
	}
	const size_type ret = blk.offset + blk.count;
	return (ret + (cache.cur0 >= ret) + (cache.cur1 >= ret)) * BLOCK_SIZE;
}

#define LOG_READ2
static inline void cache_log_read(struct cache_data *cache, BlkRange blk)
{
#ifdef LOG_READ1
	unsigned char counts[] = {cache->cand0count, cache->cand1count};
	size_type blk_end = blk.offset + blk.count;
	for (; blk.offset != blk_end; ++blk.offset) {
		if (blk.offset == cache->next0) {
			cache->cand0count &= 1;
			cache->cand1count &= 1;
		} else if (blk.offset == cache->next1) {
			unsigned char temp = cache->next1;
			cache->next1 = cache->next0;
			cache->next0 = temp;
			cache->cand0count >>= 1;
			cache->cand1count >>= 1;
		} else if (blk.offset == cache->cand0) {
			if (cache->cand0count) {
				unsigned char temp = cache->cand0;
				cache->cand0 = cache->next1;
				cache->next1 = cache->next0;
				cache->next0 = temp;
				cache->cand0count = 0;
			} else {
				cache->cand0count = 3;
			}
		} else if (blk.offset == cache->cand1) {
			unsigned char temp = cache->cand1;
			cache->cand1 = cache->cand0;
			if (cache->cand1count) {
				cache->cand0 = cache->next1;
				cache->next1 = cache->next0;
				cache->next0 = temp;
				cache->cand1count = cache->cand0count;
				cache->cand0count = 0;
			} else {
				cache->cand0 = temp;
				cache->cand1count = cache->cand0count;
				cache->cand0count = 3;
			}
		}
	}
#elif defined LOG_READ2
	unsigned char cached[] = {
		0,
		cache->next0,
		cache->next1,
		cache->cand0,
		cache->cand1,
	}, counts[] = {
		cache->cand0count,
		cache->cand1count
	}, cache_loc[256];
	memset(cache_loc + blk.offset, 0, blk.count);
	for (unsigned char i = 1; i < sizeof cached / sizeof *cached; ++i)
		if (cached[i] >= blk.offset)
			cache_loc[cached[i]] = i;
	for (unsigned char i = blk.offset; i < blk.offset + blk.count; ++i) {
		unsigned char temp = cached[cache_loc[i]];
		switch (cache_loc[i]) {
		case 1:
			counts[0] &= 1;
			counts[1] &= 1;
			break;
		case 2:
			cached[2] = cached[1];
			cache_loc[cached[2]] = 2;
			cached[1] = temp;
			counts[0] >>= 1;
			counts[1] >>= 1;
			break;
		case 3:
			if (counts[0]) {
				if (counts[0] & 2) {
					cached[3] = cached[1];
					counts[1] &= 1;
				} else {
					cached[3] = cached[2];
					cached[2] = cached[1];
					cache_loc[cached[2]] = 2;
					counts[1] >>= 1;
				}
				cache_loc[cached[3]] = 3;
				cached[1] = temp;
				counts[0] = 0;
			} else {
				counts[0] = 3;
			}
			break;
		case 4:
			cached[4] = cached[3];
			cache_loc[cached[4]] = 4;
			if (counts[1]) {
				if (counts[1] & 2) {
					cached[3] = cached[1];
					counts[1] = counts[0] & 1;
				} else {
					cached[3] = cached[2];
					cached[2] = cached[1];
					cache_loc[cached[2]] = 2;
					counts[1] = counts[0] >> 1;
				}
				cache_loc[cached[3]] = 3;
				cached[1] = temp;
				counts[0] = 0;
			} else {
				cached[3] = temp;
				counts[1] = counts[0];
				counts[0] = 3;
			}
			break;
		default:
			cache_loc[cached[4]] = 0;
			cached[4] = cached[3];
			cache_loc[cached[4]] = 4;
			cached[3] = i;
			counts[1] = counts[0];
			counts[0] = 0;
		}
	}
#endif
	cache->next0 = cached[1];
	cache->next1 = cached[2];
	cache->cand0 = cached[3];
	cache->cand1 = cached[4];
	cache->cand0count = counts[0];
	cache->cand1count = counts[1];
}

static inline void cache_pg_copy(struct cache_data *cache,
				 const char src[static PAGE_SIZE],
				 char dest[static PAGE_SIZE])
{
	const _Bool min_loc = cache->next1 < cache->next0;
	const size_type cached[] = {cache->next0 * BLOCK_SIZE,
				    cache->next1 * BLOCK_SIZE};
	cache_read(*cache, BYRNG(0, cached[min_loc]), src,
		   dest + 2 * BLOCK_SIZE);
	cache_read(*cache, BYRNG(cached[min_loc], BLOCK_SIZE), src,
		   dest + BLOCK_SIZE * min_loc);
	size_type next = cached[min_loc] + BLOCK_SIZE;
	cache_read(*cache, BYRNG(next, cached[!min_loc] - next),
		   src, dest + 2 * BLOCK_SIZE + cached[min_loc]);
	cache_read(*cache, BYRNG(cached[!min_loc], BLOCK_SIZE), src,
		   dest + BLOCK_SIZE * !min_loc);
	next = cached[!min_loc] + BLOCK_SIZE;
	cache_read(*cache, BYRNG(next, PAGE_SIZE - next),
		   src, dest + BLOCK_SIZE + cached[!min_loc]);
	cache->cur0 = cache->next0;
	cache->cur1 = cache->next1;
}

static inline void cache_pg(struct cache_data *cache,
			    char data[static PAGE_SIZE])
{
	char copy[PAGE_SIZE];
	memcpy(copy, data, PAGE_SIZE);
	cache_pg_copy(cache, copy, data);
}

static inline void cache_init(struct cache_data *cache)
{
	cache->cur1  = 1;
	cache->next1 = 1;
}

static inline void cache_reset(struct cache_data *cache)
{
	memset(cache, 0, sizeof *cache);
	cache_init(cache);
}


#endif // LIST2_CACHE_H
