#include "small-test.h"
#include "test-utils.h"


#if USZRAM_BLK_PER_PG < 4 || USZRAM_PG_PER_LOCK < 4
#  error small-test.c requires USZRAM_BLK_PER_PG and USZRAM_PG_PER_LOCK be >= 4
#endif
#if USZRAM_PAGE_COUNT < 16
#  error small-test.c requires USZRAM_PAGE_COUNT >= 16
#endif

#define PGSIZE  USZRAM_PAGE_SIZE
#define BLKSIZE USZRAM_BLOCK_SIZE
#define BLKPPG  USZRAM_BLK_PER_PG
#define PGPLK   USZRAM_PG_PER_LOCK
#define BLKPLK  (BLKPPG * PGPLK)


void empty_test(void)
{
	uszram_init();
	assert_empty();

	char zero[PGSIZE] = {0}, scratch[PGSIZE];
	for (uint_least64_t i = 0; i != USZRAM_PAGE_COUNT; ++i)
		one_pg_read(i, zero, scratch);

	uszram_write_pg(0, 1, zero);
	uszram_write_pg(USZRAM_PAGE_COUNT - 1, 1, zero);

	uszram_exit();
	assert_empty();
}

void one_pg_test(void)
{
	uszram_init();

	char pg[PGSIZE], zero[PGSIZE] = {0}, scratch[PGSIZE];
	rand_populate(PGSIZE, pg);
	uszram_write_pg(0, 1, pg);
	uszram_write_pg(1, 1, pg);
	uszram_write_pg(PGPLK - 1, 1, pg);
	uszram_write_pg(USZRAM_PAGE_COUNT - 1, 1, pg);

	assert_equal(4, uszram_pages_stored());
	assert_equal(0, uszram_pg_exists(2));

	one_pg_read(2, zero, scratch); // Expect all zero
	one_pg_read(0, pg, scratch);
	one_pg_read(1, pg, scratch);
	one_pg_read(PGPLK - 1, pg, scratch);
	one_pg_read(USZRAM_PAGE_COUNT - 1, pg, scratch);

	uszram_delete_pg(0, 2);
	uszram_delete_pg(PGPLK - 1, 1);
	uszram_delete_pg(USZRAM_PAGE_COUNT - 1, 1);

	assert_empty();
	uszram_exit();
}

void pgs_1lk_test(void)
{
	uszram_init();

	char pg[2 * PGSIZE], scratch[2 * PGSIZE];
	rand_populate(2 * PGSIZE, pg);
	uszram_write_pg(0, 2, pg);
	uszram_write_pg(PGPLK + 1, 2, pg);
	uszram_write_pg(USZRAM_PAGE_COUNT - 2, 2, pg);

	assert_equal(6, uszram_pages_stored());
	assert_equal(0, uszram_pg_exists(2));

	pgs_read(0, 2, pg, scratch);
	pgs_read(PGPLK + 1, 2, pg, scratch);
	pgs_read(USZRAM_PAGE_COUNT - 2, 2, pg, scratch);

	uszram_delete_pg(0, 2);
	uszram_delete_pg(PGPLK + 1, 2);
	uszram_delete_pg(USZRAM_PAGE_COUNT - 2, 2);

	assert_empty();
	uszram_exit();
}

void pgs_lks_test(void)
{
	uszram_init();

	char pg1[3 * PGSIZE],
	     pg2[(PGPLK + 1) * PGSIZE],
	     pg3[(PGPLK + 2) * PGSIZE],
	     scratch[6 * PGSIZE];
	rand_populate(3 * PGSIZE, pg1);
	rand_populate((PGPLK + 1) * PGSIZE, pg2);
	rand_populate((PGPLK + 2) * PGSIZE, pg3);
	uszram_write_pg(0, PGPLK + 1, pg2);
	uszram_write_pg(2 * PGPLK - 1, 3, pg1);
	uszram_write_pg(USZRAM_PAGE_COUNT - PGPLK - 1, PGPLK + 1, pg2);

	assert_equal(2 * PGPLK + 5, uszram_pages_stored());
	assert_equal(0, uszram_pg_exists(PGPLK + 1));

	pgs_read(0, PGPLK + 1, pg2, scratch);
	pgs_read(2 * PGPLK - 1, 3, pg1, scratch);
	pgs_read(USZRAM_PAGE_COUNT - PGPLK - 1, PGPLK + 1, pg2, scratch);

	uszram_delete_pg(0, PGPLK + 1);
	uszram_delete_pg(2 * PGPLK - 1, 3);
	uszram_write_pg(PGPLK - 1, PGPLK + 2, pg3);

	assert_equal(2 * PGPLK + 3, uszram_pages_stored());

	pgs_read(PGPLK - 1, PGPLK + 2, pg3, scratch);

	uszram_delete_pg(PGPLK - 1, PGPLK + 2);
	uszram_delete_pg(USZRAM_PAGE_COUNT - PGPLK - 1, PGPLK + 1);

	assert_empty();
	uszram_exit();
}

void one_blk_test(void)
{
	uszram_init();

	char blk[BLKSIZE], zero[BLKSIZE] = {0}, scratch[BLKSIZE];
	rand_populate(BLKSIZE, blk);
	uszram_write_blk(0, 1, blk);
	uszram_write_blk(1, 1, blk);
	uszram_write_blk(BLKPPG - 1, 1, blk);
	uszram_write_blk(USZRAM_BLOCK_COUNT - 1, 1, blk);

	assert_equal(2, uszram_pages_stored());
	assert_equal(0, uszram_pg_exists(1));

	one_blk_read(2, zero, scratch); // Expect all zero
	one_blk_read(2 * BLKPPG, zero, scratch);
	one_blk_read(0, blk, scratch);
	one_blk_read(1, blk, scratch);
	one_blk_read(BLKPPG - 1, blk, scratch);
	one_blk_read(USZRAM_BLOCK_COUNT - 1, blk, scratch);

	uszram_delete_blk(0, 2);
	one_blk_read(0, zero, scratch);
	one_blk_read(1, zero, scratch);
	one_blk_read(BLKPPG - 1, blk, scratch);
	uszram_delete_pg(0, 1);
	one_blk_read(BLKPPG - 1, zero, scratch);
	uszram_delete_pg(USZRAM_PAGE_COUNT - 1, 1);
	one_blk_read(USZRAM_BLOCK_COUNT - 1, zero, scratch);

	assert_empty();
	uszram_exit();
}

void blks_1pg_test(void)
{
	uszram_init();

	char blk[2 * BLKSIZE], zero[2 * BLKSIZE] = {0}, scratch[2 * BLKSIZE];
	rand_populate(2 * BLKSIZE, blk);
	uszram_write_blk(0, 2, blk);
	uszram_write_blk(BLKPPG + 1, 2, blk);
	uszram_write_blk(USZRAM_BLOCK_COUNT - 2, 2, blk);

	assert_equal(3, uszram_pages_stored());
	assert_equal(0, uszram_pg_exists(2));

	blks_read(0, 2, blk, scratch);
	blks_read(BLKPPG + 1, 2, blk, scratch);
	blks_read(USZRAM_BLOCK_COUNT - 2, 2, blk, scratch);

	uszram_delete_blk(0, 2);
	uszram_delete_blk(BLKPPG + 1, 2);
	uszram_delete_blk(USZRAM_BLOCK_COUNT - 2, 2);
	blks_read(0, 2, zero, scratch);
	blks_read(BLKPPG + 1, 2, zero, scratch);
	blks_read(USZRAM_BLOCK_COUNT - 2, 2, zero, scratch);

	uszram_delete_pg(0, 2);
	uszram_delete_pg(USZRAM_PAGE_COUNT - 1, 1);

	assert_empty();
	uszram_exit();
}

void blks_pgs_1lk_test(void)
{
	uszram_init();

	char blk1[3 * BLKSIZE],
	     blk2[(BLKPPG + 1) * BLKSIZE],
	     blk3[(BLKPPG + 2) * BLKSIZE],
	     scratch[(BLKPPG + 2) * BLKSIZE];
	rand_populate(3 * BLKSIZE, blk1);
	rand_populate((BLKPPG + 1) * BLKSIZE, blk2);
	rand_populate((BLKPPG + 2) * BLKSIZE, blk3);
	uszram_write_blk(0, BLKPPG + 1, blk2);
	uszram_write_blk(2 * BLKPPG - 1, 3, blk1);
	uszram_write_blk(5 * BLKPPG - 1, BLKPPG + 2, blk3);
	uszram_write_blk(USZRAM_BLOCK_COUNT - BLKPPG - 1, BLKPPG + 1, blk2);

	assert_equal(8, uszram_pages_stored());
	assert_equal(0, uszram_pg_exists(3));

	blks_read(0, BLKPPG + 1, blk2, scratch);
	blks_read(2 * BLKPPG - 1, 3, blk1, scratch);
	blks_read(5 * BLKPPG - 1, BLKPPG + 2, blk3, scratch);
	blks_read(USZRAM_BLOCK_COUNT - BLKPPG - 1, BLKPPG + 1, blk2, scratch);

	uszram_delete_pg(0, 3);
	uszram_delete_pg(4, 3);
	uszram_delete_pg(USZRAM_PAGE_COUNT - 2, 2);

	assert_empty();
	uszram_exit();
}

void blks_pgs_lks_test(void)
{
	uszram_init();

	char blk1[3 * BLKSIZE],
	     blk2[(BLKPLK + 3) * BLKSIZE],
	     blk3[(BLKPLK + 6) * BLKSIZE],
	     scratch[(BLKPLK + 6) * BLKSIZE];
	rand_populate(3 * BLKSIZE, blk1);
	rand_populate((BLKPLK + 3) * BLKSIZE, blk2);
	rand_populate((BLKPLK + 6) * BLKSIZE, blk3);
	uszram_write_blk(0, BLKPLK + 3, blk2);
	uszram_write_blk(2 * BLKPLK - 2, 3, blk1);
	uszram_write_blk(USZRAM_BLOCK_COUNT - BLKPLK - 3, BLKPLK + 3, blk2);

	assert_equal(2 * PGPLK + 4, uszram_pages_stored());
	assert_equal(0, uszram_pg_exists(PGPLK + 1));

	blks_read(0, BLKPLK + 3, blk2, scratch);
	blks_read(2 * BLKPLK - 2, 3, blk1, scratch);
	blks_read(USZRAM_BLOCK_COUNT - BLKPLK - 3, BLKPLK + 3, blk2, scratch);

	uszram_delete_blk(0, BLKPLK + 3);
	uszram_delete_blk(2 * BLKPLK - 2, 3);
	uszram_write_blk(BLKPLK - 3, BLKPLK + 6, blk3);

	blks_read(BLKPLK - 3, BLKPLK + 6, blk3, scratch);

	uszram_delete_all();

	assert_empty();
	uszram_exit();
}

void run_small_tests(void)
{
	empty_test();
	one_pg_test();
	pgs_1lk_test();
	pgs_lks_test();
	one_blk_test();
	blks_1pg_test();
	blks_pgs_1lk_test();
	blks_pgs_lks_test();
}
