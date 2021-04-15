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


static inline void print_timer(struct test_timer *t, _Bool raw)
{
	if (raw)
		printf("%.4f,%.4f\n", t->real_sec, t->cpu_sec);
	else
		printf("%.4f s real time, %.4f s CPU time\n",
		       t->real_sec, t->cpu_sec);
}

static inline void run_varying_threads(struct workload *w,
				       struct test_timer *t,
				       unsigned max_threads, int indent,
				       _Bool raw)
{
	if (max_threads == 0)
		return;
	unsigned copy = max_threads;
	++indent;
	while (copy /= 10)
		++indent;
	for (unsigned i = 1; i <= max_threads; ++i) {
		w->thread_count = i;
		run_workload(w, t);
		if (!raw)
			printf("%*u thread%s: ", indent, i,
			       i == 1 ? " " : "s");
		else
			printf("%u,", i);
		print_timer(t, raw);
	}
}

int main(int argc, char **argv)
{
	_Bool raw = argc > 1;
	(void)argv;

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

	struct test_timer t;
	const unsigned char blks  [] = {0, 100},
			    writes[] = {0, 50, 100},
			    comprs[] = {1, 2, 4};
	printf("USZRAM_PAGE_SIZE:   %4u\n"
	       "USZRAM_PG_PER_LOCK: %4u\n\n",
	       USZRAM_PAGE_SIZE, USZRAM_PG_PER_LOCK);

	for (unsigned char c = 0; c < sizeof comprs; ++c) {
		printf("Compressibility %u to %u:\n",
		       comprs[c],
		       comprs[c] + 1);
		pop.compr_min = work.compr_min = comprs[c];
		pop.compr_max = work.compr_max = comprs[c] + 1;
		if (!raw)
			printf("Populating: ");

		uszram_init();
		populate_store(&pop, &t);

		print_timer(&t, raw);
		if (!raw)
			print_stats(0);
		printf("\n");

		for (unsigned char b = 0; b < sizeof blks; ++b) {
			printf("  %u%% block operations:\n", blks[b]);
			work.read.percent_blks = work.write.percent_blks
				= blks[b];

			for (unsigned char w = 0; w < sizeof writes; ++w) {
				printf("    %u%% writes:\n", writes[w]);
				work.percent_writes = writes[w];
				if (raw)
					printf("Thread count,Real time (s)"
					       ",CPU time (s)\n");
				run_varying_threads(&work, &t, 4, 6, raw);
				printf("\n");
			}
		}

		uszram_exit();
	}

	return 0;
}
