#include <stdio.h>
#include <string.h>
#include <time.h>
#include <pthread.h>

#include "workload.h"
#include "test-utils.h"
#include "../uszram.h"


struct thread_data {
	// Thread-specific data
	unsigned                id;
	long                    seed;
	struct drand48_data     rand_data;
	char                   *read_buf;

	// Global constants (pointers or small variables)
	const struct workload  *w;
	char *const            *write_data;
	unsigned char           compr_count;
};

// Branch tables for convenience
static int (*const read_fns[2])(uint_least32_t, uint_least32_t, char *) = {
	uszram_read_pg,
	uszram_read_blk,
};

static int
(*const write_fns[2])(uint_least32_t, uint_least32_t, const char *) = {
	uszram_write_pg,
	uszram_write_blk,
};

static const uint_least64_t uszram_counts[2] = {
	USZRAM_PAGE_COUNT,
	USZRAM_BLOCK_COUNT,
};

static inline void call_read_fn(const struct thread_data *td,
				uint_least32_t op_rand,
				uint_least32_t addr_rand)
{
	const _Bool blk = (op_rand % 100 < td->w->read.percent_blks);
	const uint_least32_t count = td->w->read.pgblk_group[blk];
	const uint_least64_t max_count = uszram_counts[blk] - count + 1;
	const uint_least32_t addr = addr_rand % max_count;
	read_fns[blk](addr, count, td->read_buf);
}

static inline void call_write_fn(struct thread_data *td,
				 uint_least32_t op_rand,
				 uint_least32_t addr_rand)
{
	long compr_rand;
	mrand48_r(&td->rand_data, &compr_rand);
	const _Bool blk = (op_rand % 100 < td->w->write.percent_blks);

	const uint_least32_t count = td->w->write.pgblk_group[blk];
	const uint_least64_t max_count = uszram_counts[blk] - count + 1;
	const uint_least32_t addr = addr_rand % max_count;
	const unsigned char compr = (uint_least32_t)compr_rand
				    % td->compr_count;

	write_fns[blk](addr, count, td->write_data[compr]);
}

static void *populate_thread(void *tdata)
{
	struct thread_data *const td = tdata;

	uint_least64_t pg_count = td->w->request_count;
	if (pg_count > USZRAM_PAGE_SIZE)
		pg_count = USZRAM_PAGE_SIZE;
	uint_least64_t req = pg_count / td->w->thread_count;
	const uint_least32_t addr = req * td->id;

	if (td->id)
		srand48_r(td->seed, &td->rand_data);
	else // Thread 0 gets the leftover requests since it always exists
		req += td->w->request_count % td->w->thread_count;

	long compr_rand;
	unsigned char compr;

	for (uint_least64_t i = 0; i < req; ++i) {
		mrand48_r(&td->rand_data, &compr_rand);
		compr = (uint_least32_t)compr_rand % td->compr_count;
		uszram_write_pg(addr + i, 1, td->write_data[compr]);
	}
	return NULL;
}

// Contains code common to both reading and writing
static void *run_thread(void *tdata)
{
	struct thread_data *td = tdata;

	uint_least64_t req = td->w->request_count / td->w->thread_count;
	if (td->id)
		srand48_r(td->seed, &td->rand_data);
	else // Thread 0 gets the leftover requests since it always exists
		req += td->w->request_count % td->w->thread_count;

	long op_rand, addr_rand;
	_Bool write;

	for (uint_least64_t i = 0; i < req; ++i) {
		mrand48_r(&td->rand_data, &op_rand);
		write = (op_rand % 100 < td->w->percent_writes);
		mrand48_r(&td->rand_data, &op_rand);
		mrand48_r(&td->rand_data, &addr_rand);
		if (write)
			call_write_fn(td, op_rand, addr_rand);
		else
			call_read_fn(td, op_rand, addr_rand);
	}
	return NULL;
}

static inline size_t buf_size(struct rw_workload rw)
{
	const size_t size_pg  = rw.pgblk_group[0] * USZRAM_PAGE_SIZE,
		     size_blk = rw.pgblk_group[1] * USZRAM_BLOCK_SIZE;
	if (rw.percent_blks >= 100 || size_pg < size_blk)
		return size_blk;
	return size_pg;
}

static inline _Bool valid_compr(unsigned char min, unsigned char max)
{
	if (max > 12 || max <= min)
		return 0;
	return 1;
}

static inline _Bool valid_workload(const struct workload *w)
{
	if (!valid_compr(w->compr_min, w->compr_max))
		return 0;
	if ((w->read.percent_blks < 100 && w->read.pgblk_group[0] == 0)
	    || (w->read.percent_blks && w->read.pgblk_group[1] == 0))
		return 0;
	if ((w->write.percent_blks < 100 && w->write.pgblk_group[0] == 0)
	    || (w->write.percent_blks && w->write.pgblk_group[1] == 0))
		return 0;
	if (w->thread_count == 0)
		return 0;
	return 1;
}

static inline void get_write_data(size_t buf_size, char buf[static buf_size],
				  unsigned char compr)
{
	// Max length 16, plus null terminator: "data/cr11-12.raw"
	char filename[17] = "data/cr";
	snprintf(filename + 7, 10, "%i-%i.raw", compr, compr + 1);
	FILE *data = fopen(filename, "r");
	fread(buf, 1, buf_size, data);
	fclose(data);
}

void populate_store(const struct workload *w)
{
	if (!valid_compr(w->compr_min, w->compr_max) || w->thread_count == 0)
		return;
	if (USZRAM_PAGE_SIZE > 4096)
		return; // Data files are only 4096 bytes

	const unsigned char compr_count = w->compr_max - w->compr_min;
	char **const write_data = malloc((sizeof *write_data) * compr_count);
	for (unsigned char i = 0; i < compr_count; ++i) {
		write_data[i] = malloc(USZRAM_PAGE_SIZE);
		get_write_data(USZRAM_PAGE_SIZE, write_data[i],
			       w->compr_min + i);
	}

	struct thread_data *const td = malloc((sizeof *td) * w->thread_count);
	pthread_t *threads = NULL;
	if (w->thread_count > 1)
		threads = malloc((sizeof *threads) * (w->thread_count - 1));

	td->seed = -2091457876;
	srand48_r(td->seed, &td->rand_data);
	struct test_timer t = start_timer();

	for (unsigned i = 0; i < w->thread_count; ++i) {
		td[i].id = i;
		td[i].w = w;
		td[i].write_data = write_data;
		td[i].compr_count = compr_count;
		if (i) {
			// Give each thread a different random seed
			mrand48_r(&td->rand_data, &td[i].seed);
			pthread_create(threads + i - 1, NULL, populate_thread,
				       td + i);
		}
	}
	populate_thread(td);
	for (unsigned i = 1; i < w->thread_count; ++i)
		pthread_join(threads[i - 1], NULL);

	stop_timer(&t);
	free(threads);
	free(td);
	for (unsigned char i = 0; i < compr_count; ++i)
		free(write_data[i]);
	free(write_data);
}

void run_workload(const struct workload *w)
{
	if (!valid_workload(w))
		return;

	// Make read_buf and write_data large enough for either a page group or
	// a block group
	const size_t read_buf_size  = buf_size(w->read),
		     write_buf_size = buf_size(w->write);
	if (write_buf_size > 4096)
		return; // Data files are only 4096 bytes

	const unsigned char compr_count = w->compr_max - w->compr_min;
	char **const write_data = malloc((sizeof *write_data) * compr_count);
	for (unsigned char i = 0; i < compr_count; ++i) {
		write_data[i] = malloc(write_buf_size);
		get_write_data(write_buf_size, write_data[i],
			       w->compr_min + i);
	}

	struct thread_data *const td = malloc((sizeof *td) * w->thread_count);
	pthread_t *threads = NULL;
	if (w->thread_count > 1)
		threads = malloc((sizeof *threads) * (w->thread_count - 1));

	td->seed = -613048132; // Arbitrary initial seed
	srand48_r(td->seed, &td->rand_data);
	struct test_timer t = start_timer();

	for (unsigned i = 0; i < w->thread_count; ++i) {
		td[i].id = i;
		td[i].w = w;
		td[i].read_buf = malloc(read_buf_size);
		td[i].write_data = write_data;
		td[i].compr_count = compr_count;
		if (i) {
			// Give each thread a different random seed
			mrand48_r(&td->rand_data, &td[i].seed);
			pthread_create(threads + i - 1, NULL, run_thread,
				       td + i);
		}
	}
	run_thread(td);
	for (unsigned i = 1; i < w->thread_count; ++i)
		pthread_join(threads[i - 1], NULL);

	stop_timer(&t);
	free(threads);
	for (unsigned i = 0; i < w->thread_count; ++i)
		free(td[i].read_buf);
	free(td);
	for (unsigned char i = 0; i < compr_count; ++i)
		free(write_data[i]);
	free(write_data);
}
