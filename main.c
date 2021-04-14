/* COMPILE:
 * clang main.c uszram.c -llz4 [-ljemalloc] -lpthread [-ldl -lm -static]
 */
#include <stdio.h>
#include <inttypes.h>

#include "uszram.h"
#include "test/small-test.h"
#include "test/large-test.h"
#include "test/workload.h"
#include "test/test-utils.h"

int main(void)
{
	// small-test.h
	run_small_tests();

	/*

	// large-test.h
	many_pgs_test(1, 1u << 15, 4096);
	many_blks_test(1, 1u << 18, 512);

	*/

	// workload.h
	struct workload pop = {
		.compr_min = 7,
		.compr_max = 8,
		.request_count = USZRAM_PAGE_COUNT,
		.thread_count = 8,
	};
	struct rw_workload rw = {
		.percent_blks = 50,
		.pgblk_group = {1, 4},
	};
	struct workload w = {
		.percent_writes = 50,
		.compr_min = 2,
		.compr_max = 8,
		.request_count = 1ul << 20,
		.thread_count = 1,
		.read = rw,
		.write = rw,
	};

	uszram_init();
	printf("Populating: ");
	populate_store(&pop);
	print_stats();

	for (unsigned i = 1; i <= 8; ++i) {
		w.thread_count = i;
		printf("%7"PRIuLEAST64" requests, %2u threads: ",
		       w.request_count, i);
		run_workload(&w);
	}
	uszram_exit();

	return 0;
}
