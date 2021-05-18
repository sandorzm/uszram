#include <stdio.h>
#include <stdlib.h>

#include "test-utils.h"
#include "../caches/list2-cache.h"


#if USZRAM_BLK_PER_PG < 8
#  error list2-cache-test.c requires at least 8 blocks per page
#endif


struct cache_test {
	struct cache_data cache;
	char pg[PAGE_SIZE];
};

static void make_tests(const char *pg, unsigned char test_count,
		       struct cache_test *tests)
{
	for (unsigned char i = 0; i < test_count; ++i) {
		char *dest = tests[i].pg;
		const unsigned char next[] = {
			tests[i].cache.next0,
			tests[i].cache.next1,
		};
		const _Bool min_loc = next[1] < next[0];
		const unsigned char mid = next[!min_loc] - next[ min_loc] - 1,
				    end = BLK_PER_PG     - next[!min_loc] - 1;

		tests[i].cache.cur0 = next[0];
		tests[i].cache.cur1 = next[1];

		dest = memcpy_ret(dest, pg + next[0] * BLOCK_SIZE, BLOCK_SIZE);
		dest = memcpy_ret(dest, pg + next[1] * BLOCK_SIZE, BLOCK_SIZE);

		dest = memcpy_ret(dest, pg, next[min_loc] * BLOCK_SIZE);
		dest = memcpy_ret(dest, pg + (next[ min_loc] + 1) * BLOCK_SIZE,
				  mid * BLOCK_SIZE);
		dest = memcpy_ret(dest, pg + (next[!min_loc] + 1) * BLOCK_SIZE,
				  end * BLOCK_SIZE);
	}
}

static void assert_blkeq(unsigned short blk_count,
			 const char expected[static BLOCK_SIZE],
			 const char actual[static BLOCK_SIZE])
{
	for (size_type i = 0; i < blk_count * BLOCK_SIZE; ++i)
		if (expected[i] != actual[i]) {
			PRINT_ERROR("Blocks differed at byte %u\n", i);
			exit(EXIT_FAILURE);
		}
}

static void assert_pgeq(const char expected[static PAGE_SIZE],
			const char actual[static PAGE_SIZE])
{
	assert_blkeq(BLK_PER_PG, expected, actual);
}

static void assert_cacheq(const struct cache_data expected,
			  const struct cache_data actual)
{
	assert_equal(expected.cur0,       actual.cur0);
	assert_equal(expected.cur1,       actual.cur1);
	assert_equal(expected.next0,      actual.next0);
	assert_equal(expected.next1,      actual.next1);
	assert_equal(expected.cand0,      actual.cand0);
	assert_equal(expected.cand1,      actual.cand1);
	assert_equal(expected.cand0count, actual.cand0count);
	assert_equal(expected.cand1count, actual.cand1count);
}

static void uncache_pg_test(const char *expected, unsigned char test_count,
			    const struct cache_test *tests)
{
	for (unsigned char i = 0; i < test_count; ++i) {
		char copy[PAGE_SIZE];
		memcpy(copy, tests[i].pg, sizeof copy);
		uncache_pg(tests[i].cache, copy);
		assert_pgeq(expected, copy);
	}
}

static void cache_read_test(const char *expected, unsigned char test_count,
			    const struct cache_test *tests,
			    unsigned char range_count, const BlkRange *ranges)
{
	for (unsigned char i = 0; i < test_count; ++i)
		for (unsigned char j = 0; j < range_count; ++j) {
			char copy[PAGE_SIZE];
			const ByteRange byte = {
				.offset = ranges[j].offset * BLOCK_SIZE,
				.count  = ranges[j].count  * BLOCK_SIZE,
			};
			cache_read(tests[i].cache, byte, tests[i].pg, copy);
			assert_blkeq(ranges[j].count, expected + byte.offset,
				     copy);
		}
}

static void get_pg_ranges_test(const char *expected, unsigned char test_count,
			       const struct cache_test *tests,
			       unsigned char range_count,
			       const BlkRange *ranges)
{
	for (unsigned char i = 0; i < test_count; ++i)
		for (unsigned char j = 0; j < range_count; ++j) {
			BlkRange ret[MAX_PG_RANGES];
			const unsigned char sub_count
				= get_pg_ranges(tests[i].cache, ranges[j], ret);
			const struct ByteRange byte = {
				ranges[j].offset * BLOCK_SIZE,
				ranges[j].count  * BLOCK_SIZE,
			};
			size_type offset = byte.offset;
			for (unsigned char r = 0; r < sub_count; ++r) {
				char copy[PAGE_SIZE];
				const struct ByteRange sub = {
					ret[r].offset * BLOCK_SIZE,
					ret[r].count  * BLOCK_SIZE,
				};
				memcpy(copy, tests[i].pg + sub.offset,
				       sub.count);
				assert_blkeq(ret[r].count, expected + offset,
					     copy);
				offset += sub.count;
			}
			assert_equal(byte.offset + byte.count, offset);
		}
}

static void bytes_needed_test(unsigned char test_count,
			      const struct cache_test *tests,
			      unsigned char range_count, const BlkRange *ranges)
{
	for (unsigned char i = 0; i < test_count; ++i)
		for (unsigned char j = 0; j < range_count; ++j) {
			unsigned short count = ranges[j].count;
			const size_type blks = ranges[j].offset + count;
			unsigned char inc = 0;
			if (tests[i].cache.cur0 >= blks)
				++inc;
			else if (tests[i].cache.cur0 >= ranges[j].offset)
				--count;
			if (!count) {
				assert_equal(BLOCK_SIZE,
					     bytes_needed(tests[i].cache,
							  ranges[j]));
				continue;
			}
			if (tests[i].cache.cur1 >= blks)
				++inc;
			else if (tests[i].cache.cur1 >= ranges[j].offset)
				--count;
			assert_equal((count ? (blks + inc) : 2) * BLOCK_SIZE,
				     bytes_needed(tests[i].cache, ranges[j]));
		}
}

static void cache_log_read_test(void)
{
	static const struct cache_data tests[] = {
		{.next0 = 0, 0, .cand0 = 0, 0, .cand0count = 0, 0},
		{.next0 = 0, 0, .cand0 = 0, 0, .cand0count = 3, 0},
		{.next0 = 0, 0, .cand0 = 1, 0, .cand0count = 3, 0},
		{.next0 = 2, 5, .cand0 = 1, 0, .cand0count = 1, 0},
	}, expected[] = {
		{.next0 = 0, 0, .cand0 = 2, 1, .cand0count = 0, 0},
		{.next0 = 0, 0, .cand0 = 2, 1, .cand0count = 0, 0},
		{.next0 = 1, 0, .cand0 = 2, 0, .cand0count = 0, 0},
		{.next0 = 2, 1, .cand0 = 5, 0, .cand0count = 0, 0},
	};
	_Static_assert(sizeof tests / sizeof *tests
		       == sizeof expected / sizeof *expected,
		       "Different numbers of tests and expected results\n");
	static const unsigned char test_count = sizeof tests / sizeof *tests;

	for (unsigned char i = 0; i < test_count; ++i) {
		struct cache_data cache = tests[i];
		cache_log_read(&cache, BLRNG(1, 2));
		assert_cacheq(expected[i], cache);
	}
}

static void cache_pg_copy_test(unsigned char test_count,
			       const struct cache_test *tests)
{
	static const unsigned char compare[] = {1, 2, 3};
	static const unsigned char compare_count =
		sizeof compare / sizeof *compare;
	for (unsigned char i = 0; i < test_count; ++i) {
		for (unsigned char j = 0; j < compare_count; ++j) {
			char copy[PAGE_SIZE];
			struct cache_data cache_copy = tests[i].cache;
			const unsigned char cmp = (i + compare[j]) % test_count;
			cache_copy.next0 = tests[cmp].cache.next0;
			cache_copy.next1 = tests[cmp].cache.next1;
			cache_pg_copy(&cache_copy, tests[i].pg, copy);
			assert_pgeq(tests[cmp].pg, copy);
		}
	}
}

static void cache_pg_test(unsigned char test_count,
			  const struct cache_test *tests)
{
	static const unsigned char compare[] = {1, 2, 3};
	static const unsigned char compare_count =
		sizeof compare / sizeof *compare;
	for (unsigned char i = 0; i < test_count; ++i) {
		for (unsigned char j = 0; j < compare_count; ++j) {
			char copy[PAGE_SIZE];
			memcpy(copy, tests[i].pg, sizeof copy);
			struct cache_data cache_copy = tests[i].cache;
			const unsigned char cmp = (i + compare[j]) % test_count;
			cache_copy.next0 = tests[cmp].cache.next0;
			cache_copy.next1 = tests[cmp].cache.next1;
			cache_pg(&cache_copy, copy);
			assert_pgeq(tests[cmp].pg, copy);
		}
	}
}

static void cache_reset_test(unsigned char test_count, struct cache_test *tests)
{
	static const struct cache_data reset = {.cur1 = 1, .next1 = 1};
	for (unsigned char i = 0; i < test_count; ++i) {
		cache_reset(&tests[i].cache);
		assert_cacheq(reset, tests[i].cache);
	}
}

int main(void)
{
	static struct cache_test tests[] = {
		{.cache.next0 = 0, 1}, {.cache.next0 = 0, 2},
		{.cache.next0 = 0, 5}, {.cache.next0 = 1, 0},
		{.cache.next0 = 2, 0}, {.cache.next0 = 3, 0},
		{.cache.next0 = 7, 0}, {.cache.next0 = 1, 2},
		{.cache.next0 = 2, 3}, {.cache.next0 = 3, 4},
		{.cache.next0 = 2, 1}, {.cache.next0 = 3, 1},
		{.cache.next0 = 6, 1}, {.cache.next0 = 2, 4},
		{.cache.next0 = 4, 2}, {.cache.next0 = 3, 5},
		{.cache.next0 = 5, 3}, {.cache.next0 = 6, 7},
		{.cache.next0 = 7, 6},
	};
	static const unsigned char test_count = sizeof tests / sizeof *tests;
	static const BlkRange ranges[] = {
		{.offset = 0, .count = 1}, {.offset = 0, .count = 2},
		{.offset = 0, .count = 3}, {.offset = 0, .count = 8},
		{.offset = 1, .count = 1}, {.offset = 1, .count = 2},
		{.offset = 1, .count = 3}, {.offset = 1, .count = 7},
		{.offset = 2, .count = 1}, {.offset = 2, .count = 2},
		{.offset = 2, .count = 3}, {.offset = 2, .count = 6},
		{.offset = 3, .count = 1}, {.offset = 3, .count = 2},
		{.offset = 3, .count = 3}, {.offset = 3, .count = 4},
		{.offset = 3, .count = 5},
	};
	static const unsigned char range_count = sizeof ranges / sizeof *ranges;

	char pg[PAGE_SIZE];
	rand_populate(PAGE_SIZE, pg);
	make_tests(pg, test_count, tests);

	uncache_pg_test(pg, test_count, tests);
	cache_read_test(pg, test_count, tests, range_count, ranges);
	get_pg_ranges_test(pg, test_count, tests, range_count, ranges);
	bytes_needed_test(test_count, tests, range_count, ranges);
	cache_log_read_test();
	cache_pg_copy_test(test_count, tests);
	cache_pg_test(test_count, tests);
	cache_reset_test(test_count, tests);
}
