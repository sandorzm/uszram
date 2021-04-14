/* COMPILE:
 * clang main.c uszram.c -llz4 [-ljemalloc] -lpthread [-ldl -lm -static]
 */
#include <stdio.h>

#include "uszram.h"
#include "test/small-test.h"
#include "test/large-test.h"
#include "test/workload.h"

int main(void)
{
	// small-test.h
	run_small_tests();

	// large-test.h
	many_pgs_test(1, 1u << 15, 4096);
	many_blks_test(1, 1u << 18, 512);

	// workload.h
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
	for (unsigned i = 1; i <= 16; ++i) {
		w.thread_count = i;
		run_workload(&w, 0);
	}

	return 0;
}
