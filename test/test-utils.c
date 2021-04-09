#include <stdio.h>
#include <string.h>

#include "test-utils.h"


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
	assert_equal(0, uszram_heap_size());
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
