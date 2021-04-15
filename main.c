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

void run_varying_threads(struct workload *w, unsigned max_threads, int indent)
{
	if (max_threads == 0)
		return;
	unsigned copy = max_threads;
	unsigned char max_digits = 1;
	while (copy /= 10)
		++max_digits;
	for (unsigned i = 1; i <= max_threads; ++i) {
		w->thread_count = i;
		if (i == 1)
			printf("%*u thread : ", indent + max_digits, i);
		else
			printf("%*u threads: ", indent + max_digits, i);
		run_workload(w);
	}
}

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
		.request_count = USZRAM_PAGE_COUNT,
		.thread_count = 8,
	};
	struct workload work = {
		.request_count = 2 * USZRAM_PAGE_COUNT,
		.read = {.pgblk_group = {1, 2}},
		.write = {.pgblk_group = {1, 2}},
	};

	const unsigned char blks  [] = {0, 100},
			    writes[] = {0, 50, 100},
			    comprs[] = {1, 2, 4};
	printf("USZRAM_PAGE_SIZE:   %4u\n"
	       "USZRAM_PG_PER_LOCK: %4u\n\n",
	       USZRAM_PAGE_SIZE, USZRAM_PG_PER_LOCK);

	for (unsigned char b = 0; b < sizeof blks; ++b) {
		printf("%u%% block operations:\n", blks[b]);
		work.read.percent_blks = work.write.percent_blks = blks[b];

		for (unsigned char w = 0; w < sizeof writes; ++w) {
			printf("  %u%% writes:\n", writes[w]);
			work.percent_writes = writes[w];

			for (unsigned char c = 0; c < sizeof comprs; ++c) {
				printf("    Compressibility %u to %u:\n",
				       comprs[c], comprs[c] + 1);
				pop.compr_min = work.compr_min = comprs[c];
				pop.compr_max = work.compr_max = comprs[c] + 1;
				printf("      Populating: ");

				uszram_init();
				populate_store(&pop);
				printf("      Stats:\n");
				print_stats(8);
				printf("      Running:\n");
				run_varying_threads(&work, 8, 8);
				printf("\n");
				uszram_exit();
			}
		}
	}

	return 0;
}
