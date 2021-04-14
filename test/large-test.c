#include <stdio.h>
#include <inttypes.h>
#include <time.h>

#include "large-test.h"
#include "test-utils.h"


void assert_non_null(void *p)
{
	if (p != NULL)
		return;
	fprintf(stderr, "Unexpected null pointer\n");
	uszram_exit();
	exit(EXIT_FAILURE);
}

void many_pgs_test(uint_least32_t pg_group, uint_least32_t num_pg_grp,
		   unsigned pg_fill)
{
	size_t bytes_pg_grp = (size_t)pg_group * USZRAM_PAGE_SIZE;

	char **write = malloc((sizeof *write) * num_pg_grp),
	     **read  = malloc((sizeof *read)  * num_pg_grp);
	assert_non_null(write);
	assert_non_null(read);

	for (uint_least32_t i = 0; i < num_pg_grp; ++i) {
		write[i] = malloc(bytes_pg_grp);
		read [i] = malloc(bytes_pg_grp);
		assert_non_null(write[i]);
		assert_non_null(read [i]);

		for (size_t j = 0; j < pg_group; ++j)
			rand_populate(pg_fill, write[i] + j * USZRAM_PAGE_SIZE);
	}

	uszram_init();
	double duration = 0;
	struct timespec start, end;
	timespec_get(&start, TIME_UTC);
	for (uint_least32_t i = 0; i < num_pg_grp; ++i)
		uszram_write_pg(i * pg_group, pg_group, write[i]);
	timespec_get(&end, TIME_UTC);
	duration = end.tv_sec - start.tv_sec
		   + (end.tv_nsec - start.tv_nsec) / 1000000000.0;

	printf("Stored %"PRIuLEAST32" groups of %"PRIuLEAST32
	       " pages in %.4f s\n", num_pg_grp, pg_group, duration);
	print_stats();
	assert_equal(pg_group * num_pg_grp, uszram_pages_stored());

	timespec_get(&start, TIME_UTC);
	for (uint_least32_t i = 0; i < num_pg_grp; ++i)
		uszram_read_pg(i * pg_group, pg_group, read[i]);
	timespec_get(&end, TIME_UTC);
	duration = end.tv_sec - start.tv_sec
		   + (end.tv_nsec - start.tv_nsec) / 1000000000.0;
	uszram_exit();

	printf("Loaded %"PRIuLEAST32" groups of %"PRIuLEAST32
	       " pages in %.4f s\n\n", num_pg_grp, pg_group, duration);

	for (uint_least32_t i = 0; i < num_pg_grp; ++i)
		for (size_t j = 0; j < bytes_pg_grp; ++j)
			assert_equal(write[i][j], read[i][j]);
}

void many_blks_test(uint_least32_t blk_group, uint_least32_t num_blk_grp,
		    unsigned blk_fill)
{
	size_t bytes_blk_grp = (size_t)blk_group * USZRAM_BLOCK_SIZE;

	char **write = malloc((sizeof *write) * num_blk_grp),
	     **read  = malloc((sizeof *read)  * num_blk_grp);
	assert_non_null(write);
	assert_non_null(read);

	for (uint_least32_t i = 0; i < num_blk_grp; ++i) {
		write[i] = malloc(bytes_blk_grp);
		read [i] = malloc(bytes_blk_grp);
		assert_non_null(write[i]);
		assert_non_null(read [i]);

		for (size_t j = 0; j < blk_group; ++j)
			rand_populate(blk_fill,
				      write[i] + j * USZRAM_BLOCK_SIZE);
	}

	uszram_init();
	double duration = 0;
	struct timespec start, end;
	timespec_get(&start, TIME_UTC);
	for (uint_least32_t i = 0; i < num_blk_grp; ++i)
		uszram_write_blk(i * blk_group, blk_group, write[i]);
	timespec_get(&end, TIME_UTC);
	duration = end.tv_sec - start.tv_sec
		   + (end.tv_nsec - start.tv_nsec) / 1000000000.0;

	printf("Stored %"PRIuLEAST32" groups of %"PRIuLEAST32
	       " blocks in %.4f s\n", num_blk_grp, blk_group, duration);
	print_stats();
	uint_least64_t blocks = blk_group * num_blk_grp;
	uint_least64_t pages
		= blocks / USZRAM_BLK_PER_PG
		  + (blocks % USZRAM_BLK_PER_PG != 0);
	assert_equal(pages, uszram_pages_stored());

	timespec_get(&start, TIME_UTC);
	for (uint_least32_t i = 0; i < num_blk_grp; ++i)
		uszram_read_blk(i * blk_group, blk_group, read[i]);
	timespec_get(&end, TIME_UTC);
	duration = end.tv_sec - start.tv_sec
		   + (end.tv_nsec - start.tv_nsec) / 1000000000.0;
	uszram_exit();

	printf("Loaded %"PRIuLEAST32" groups of %"PRIuLEAST32
	       " blocks in %.4f s\n\n", num_blk_grp, blk_group, duration);

	for (uint_least32_t i = 0; i < num_blk_grp; ++i)
		for (size_t j = 0; j < bytes_blk_grp; ++j)
			assert_equal(write[i][j], read[i][j]);
}
