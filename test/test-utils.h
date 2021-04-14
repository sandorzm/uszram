#ifndef TEST_UTILS_H
#define TEST_UTILS_H


#include <stdlib.h>

#include "../uszram.h"


struct test_timer {
	struct timespec  start,     end;
	clock_t          cpu_start, cpu_end;
};

struct test_timer start_timer(void);
void stop_timer(struct test_timer *t);

void print_stats(void);

void assert_safe(_Bool b);
void assert_equal(int expected, int actual);
void assert_empty(void);

void blk_read_fast(uint_least32_t blk_addr,
		   char expected[static USZRAM_BLOCK_SIZE]);
void pg_read_fast(uint_least32_t pg_addr,
		  char expected[static USZRAM_PAGE_SIZE]);

void one_blk_read(uint_least32_t blk_addr,
		  char expected[static USZRAM_BLOCK_SIZE],
		  char actual[static USZRAM_BLOCK_SIZE]);
void blks_read(uint_least32_t blk_addr, uint_least32_t blocks,
	       char expected[static blocks * USZRAM_BLOCK_SIZE],
	       char actual[static blocks * USZRAM_BLOCK_SIZE]);
void one_pg_read(uint_least32_t pg_addr,
		 char expected[static USZRAM_PAGE_SIZE],
		 char actual[static USZRAM_PAGE_SIZE]);
void pgs_read(uint_least32_t pg_addr, uint_least32_t pages,
	      char expected[static pages * USZRAM_PAGE_SIZE],
	      char actual[static pages * USZRAM_PAGE_SIZE]);

void rand_populate(size_t bytes, char data[static bytes]);


#endif // TEST_UTILS_H
