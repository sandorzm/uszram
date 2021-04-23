/* COMPILE:
 * cc -pthread main.c uszram.c ... -llz4 [-ljemalloc] [-ldl -lm -static]
 */
#include <stdio.h>
#include <inttypes.h>

#include "uszram.h"
//#include "test/small-test.h"
//#include "test/large-test.h"
#include "test/workload.h"
#include "test/test-utils.h"


static inline void run_varying_threads(struct workload *w, struct test_timer *t,
				       unsigned max_threads, int indent,
				       _Bool raw)
{
	if (max_threads == 0)
		return;
	if (!raw) {
		unsigned copy = max_threads;
		++indent;
		while (copy /= 10)
			++indent;
	}
	for (unsigned i = 1; i <= max_threads; ++i) {
		w->thread_count = i;
		run_workload(w, t);
		if (!raw)
			printf("%*u thread%s: %.4f s real time, "
			       "%.4f s CPU time\n", indent, i,
			       i == 1 ? " " : "s", t->real_sec, t->cpu_sec);
		else
			printf("%u,%.4f,%.4f\n", i, t->real_sec, t->cpu_sec);
	}
}

int main(int argc, char **argv)
{
	_Bool raw = argc > 1;
	(void)argv;

	/*

	// small-test.h
	run_small_tests();

	// large-test.h
	many_pgs_test(1, 1u << 15, 4096);
	many_blks_test(1, 1u << 18, 512);

	*/

	// workload.h
	struct workload pop = {
		.request_count = USZRAM_PAGE_COUNT,
		.thread_count = 16,
	};
	struct workload work = {
		.request_count = 1ul << 23,
		.read = {.pgblk_group = {1, 1}},
		.write = {.pgblk_group = {1, 1}},
	};

	struct test_timer t;
	const unsigned char blks  [] = {100},
			    writes[] = {0, 50, 100},
			    comprs[] = {1, 2, 4};
	printf("USZRAM_PAGE_SIZE:   %4u\n"
	       "USZRAM_PG_PER_LOCK: %4u\n\n",
	       USZRAM_PAGE_SIZE, USZRAM_PG_PER_LOCK);

	for (unsigned char c = 0; c < sizeof comprs; ++c) {
		printf("Compressibility %u to %u:\n",
		       comprs[c],
		       comprs[c] + 1u);
		pop.compr_min = work.compr_min = comprs[c];
		pop.compr_max = work.compr_max = comprs[c] + 1;
		if (!raw)
			printf("Populating: ");

		uszram_init();
		populate_store(&pop, &t);

		if (raw) {
			printf("%.4f,%.4f\n\n", t.real_sec, t.cpu_sec);
		} else {
			printf("%.4f s real time, %.4f s CPU time\n",
			       t.real_sec, t.cpu_sec);
			print_stats(0);
			printf("\n");
		}

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
				run_varying_threads(&work, &t, 16, 6, raw);
				printf("\n");
			}
		}

		uszram_exit();
	}

	return 0;
}
