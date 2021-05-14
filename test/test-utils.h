#ifndef TEST_UTILS_H
#define TEST_UTILS_H


#include <time.h>

#include "../uszram.h"


#define PRINT_ERROR(...) do { fprintf(stderr, "%s:%i: ", __FILE__, __LINE__); \
			      fprintf(stderr, __VA_ARGS__); } while (0)


struct test_timer {
	struct timespec  start,     end;
	clock_t          cpu_start, cpu_end;
	double           real_sec,  cpu_sec;
};

void start_timer(struct test_timer *t);
void stop_timer(struct test_timer *t);

void print_stats(int indent);

void assert_safe(_Bool b);
void assert_equal(int expected, int actual);
void assert_empty(void);

char *memcpy_ret(char *restrict dest, const char *restrict src, size_t count);

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
