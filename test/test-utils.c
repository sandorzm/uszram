#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <time.h>

#include "test-utils.h"


struct test_timer start_timer(void)
{
	struct test_timer t;
	t.cpu_start = clock();
	timespec_get(&t.start, TIME_UTC);
	return t;
}

void stop_timer(struct test_timer *t)
{
	timespec_get(&t->end, TIME_UTC);
	t->cpu_end = clock();

	double cpu_sec = (double)(t->cpu_end - t->cpu_start) / CLOCKS_PER_SEC;
	double real_sec = t->end.tv_sec - t->start.tv_sec
			  + (t->end.tv_nsec - t->start.tv_nsec) / 1e9;
	printf("%.4f s real time, %.4f s CPU time\n", real_sec, cpu_sec);
}

void print_stats(void)
{
	printf("Total size:   %"PRIuLEAST64"\n", uszram_total_size());
	printf("Pages stored: %"PRIuLEAST64"\n", uszram_pages_stored());
	printf("Huge pages:   %"PRIuLEAST64"\n", uszram_huge_pages());
	printf("Compressions: %"PRIuLEAST64"\n", uszram_num_compr());
	printf("Failed compr: %"PRIuLEAST64"\n", uszram_failed_compr());
}

void assert_safe(_Bool b)
{
	if (b)
		return;
	uszram_exit();
	exit(EXIT_FAILURE);
}

void assert_equal(int expected, int actual)
{
	if (expected == actual)
		return;
	uszram_exit();
	fprintf(stderr, "Expected <%i> but was <%i>\n", expected, actual);
	exit(EXIT_FAILURE);
}

void assert_empty(void)
{
	assert_equal(0, uszram_total_heap());
	assert_equal(0, uszram_pages_stored());
	assert_equal(0, uszram_huge_pages());
	for (uint_least64_t i = 0; i != USZRAM_PAGE_COUNT; ++i) {
		assert_equal(0, uszram_pg_exists(i));
		assert_equal(0, uszram_pg_is_huge(i));
		assert_equal(0, uszram_pg_heap(i));
	}
}

void blk_read_fast(uint_least32_t blk_addr,
		   char expected[static USZRAM_BLOCK_SIZE])
{
	char actual[USZRAM_BLOCK_SIZE];
	int ret = uszram_read_blk(blk_addr, 1, actual);
	assert_equal(0, ret);
	for (unsigned i = 0; i < USZRAM_BLOCK_SIZE; ++i)
		assert_equal(expected[i], actual[i]);
}

void pg_read_fast(uint_least32_t pg_addr,
		  char expected[static USZRAM_PAGE_SIZE])
{
	char actual[USZRAM_PAGE_SIZE];
	int ret = uszram_read_pg(pg_addr, 1, actual);
	assert_equal(0, ret);
	for (unsigned i = 0; i < USZRAM_PAGE_SIZE; ++i)
		assert_equal(expected[i], actual[i]);
}

void one_blk_read(uint_least32_t blk_addr,
		  char expected[static USZRAM_BLOCK_SIZE],
		  char actual[static USZRAM_BLOCK_SIZE])
{
	memset(actual, 0, USZRAM_BLOCK_SIZE);
	int ret = uszram_read_blk(blk_addr, 1, actual);
	assert_equal(0, ret);
	for (unsigned i = 0; i < USZRAM_BLOCK_SIZE; ++i)
		assert_equal(expected[i], actual[i]);
}

void blks_read(uint_least32_t blk_addr, uint_least32_t blocks,
	       char expected[static blocks * USZRAM_BLOCK_SIZE],
	       char actual[static blocks * USZRAM_BLOCK_SIZE])
{
	uint_least32_t blk_end = blk_addr + blocks;
	char *exp_blk = expected;

	memset(actual, 0, blocks * USZRAM_BLOCK_SIZE);
	int ret = uszram_read_blk(blk_addr, blocks, actual);
	assert_equal(0, ret);
	for (size_t i = 0; i < blocks * USZRAM_BLOCK_SIZE; ++i)
		assert_equal(expected[i], actual[i]);

	for (uint_least32_t i = blk_addr; i != blk_end; ++i) {
		one_blk_read(i, exp_blk, actual);
		exp_blk += USZRAM_BLOCK_SIZE;
	}
}

void one_pg_read(uint_least32_t pg_addr,
		 char expected[static USZRAM_PAGE_SIZE],
		 char actual[static USZRAM_PAGE_SIZE])
{
	memset(actual, 0, USZRAM_PAGE_SIZE);
	int ret = uszram_read_pg(pg_addr, 1, actual);
	assert_equal(0, ret);
	for (unsigned i = 0; i < USZRAM_PAGE_SIZE; ++i)
		assert_equal(expected[i], actual[i]);

	blks_read(pg_addr * USZRAM_BLK_PER_PG, USZRAM_BLK_PER_PG, expected,
		  actual);
}

void pgs_read(uint_least32_t pg_addr, uint_least32_t pages,
	      char expected[static pages * USZRAM_PAGE_SIZE],
	      char actual[static pages * USZRAM_PAGE_SIZE])
{
	uint_least32_t pg_end = pg_addr + pages;
	char *exp_pg = expected;

	memset(actual, 0, pages * USZRAM_PAGE_SIZE);
	int ret = uszram_read_pg(pg_addr, pages, actual);
	assert_equal(0, ret);
	for (size_t i = 0; i < pages * USZRAM_PAGE_SIZE; ++i)
		assert_equal(expected[i], actual[i]);

	for (uint_least32_t i = pg_addr; i != pg_end; ++i) {
		one_pg_read(i, exp_pg, actual);
		exp_pg += USZRAM_PAGE_SIZE;
	}
}

void rand_populate(size_t bytes, char data[static bytes])
{
	for (size_t i = 0; i < bytes; ++i)
		((unsigned char *)data)[i] = rand();
}
