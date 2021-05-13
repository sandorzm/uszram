#ifndef LIST2_CACHE_H
#define LIST2_CACHE_H


#include <string.h>

#include "../cache-api.h"


#define BLKPPG_SHIFT (USZRAM_PAGE_SHIFT - USZRAM_BLOCK_SHIFT)


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

static inline void restore_blk_order(struct cache_data cache,
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
		data += (1 + cache.cur1) * BLOCK_SIZE;
		memcpy(data, data, BLOCK_SIZE);
	}
	const size_type count = cached[!min_loc] - cached[min_loc] - BLOCK_SIZE;
	memmove(data, data + BLOCK_SIZE, count);
	memcpy(data + count, max_data, BLOCK_SIZE);
}

static inline void get_ordered_blks(struct cache_data cache,
				    struct range byte,
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
		if (byte.offset > offsets[i])
			continue;
		size_type to_next = offsets[i] - byte.offset;
		if (to_next >= byte.count) {
			memcpy(dest, src + (2 - i) * BLOCK_SIZE + byte.offset,
			       byte.count);
			return;
		}
		memcpy(dest, src + (2 - i) * BLOCK_SIZE + byte.offset, to_next);
		dest += to_next;
		memcpy(dest, src + BLOCK_SIZE * min_loc, BLOCK_SIZE);
		dest += BLOCK_SIZE;

		min_loc      = !min_loc;
		to_next     += BLOCK_SIZE;
		byte.offset += to_next;
		byte.count  -= to_next;
	}
}

static inline size_type decompr_byte_count(struct cache_data cache,
					   struct range blk)
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
	size_type ret = blk.offset + blk.count;
	if (cache.cur0 >= ret)
		++ret;
	if (cache.cur1 >= ret)
		++ret;
	return ret * BLOCK_SIZE;
}

#define LOG_READ2
static inline void log_read(struct cache_data *cache, struct range blk)
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

static inline void update_cache(struct cache_data *cache,
				char data[static PAGE_SIZE])
{
	char copy[PAGE_SIZE];
	memcpy(copy, data, PAGE_SIZE);
	const _Bool min_loc = cache->next1 < cache->next0;
	const size_type cached[] = {cache->next0 * BLOCK_SIZE,
				    cache->next1 * BLOCK_SIZE};
	get_ordered_blks(*cache, RNG(0, cached[min_loc]), copy,
			 data + 2 * BLOCK_SIZE);
	get_ordered_blks(*cache, RNG(cached[min_loc], 1), copy, data);
	get_ordered_blks(*cache, RNG(cached[min_loc] + 1,
				     cached[!min_loc] - cached[min_loc] - 1),
			 copy, data + (2 + cached[min_loc]) * BLOCK_SIZE);
	get_ordered_blks(*cache, RNG(cached[!min_loc], 1), copy,
			 data + BLOCK_SIZE);
	get_ordered_blks(*cache, RNG(cached[!min_loc] + 1,
				     BLK_PER_PG - cached[!min_loc] - 1),
			 copy, data + (1 + cached[!min_loc]) * BLOCK_SIZE);
	cache->cur0 = cache->next0;
	cache->cur1 = cache->next1;
}

static inline void reset_cache(struct cache_data *cache)
{
	memset(cache, 0, sizeof *cache);
	cache->cur1 = 1;
}


#endif // LIST2_CACHE_H
