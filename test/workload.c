#include <stdio.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include <inttypes.h>

#include "workload.h"
#include "test-utils.h"
#include "../uszram.h"


struct thread_data {
	// Thread-specific data
	unsigned                id;
	int                     seed;
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

void call_write_fn(struct thread_data *td, long op_rand, long addr_rand)
{
	long compr_rand;
	mrand48_r(&td->rand_data, &compr_rand);
	_Bool blk = (op_rand % 100 < td->w->write.percent_blks);

	uint_least32_t addr = (uint_least32_t)addr_rand % uszram_counts[blk],
		       count = td->w->write.pgblk_group[blk];
	unsigned char compr_num = (uint_least32_t)compr_rand % td->compr_count;

	write_fns[blk](addr, count, td->write_data[compr_num]);
}

void call_read_fn(struct thread_data *td, long op_rand, long addr_rand)
{
	_Bool blk = (op_rand % 100 < td->w->read.percent_blks);
	uint_least32_t addr = (uint_least32_t)addr_rand % uszram_counts[blk],
		       count = td->w->read.pgblk_group[blk];
	read_fns[blk](addr, count, td->read_buf);
}

// Contains code common to both reading and writing
void *run_thread(void *tdata)
{
	struct thread_data *td = tdata;

	uint_least64_t req = td->w->request_count / td->w->thread_count;
	long op_rand, addr_rand;
	_Bool write;
	// Thread 0 gets the leftover requests
	if (td->id == 0)
		req += td->w->request_count % td->w->thread_count;

	srand48_r(td->seed, &td->rand_data);

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

void run_workload(struct workload *w)
{
	if (w->compr_max > 12 || w->compr_max <= w->compr_min)
		return;
	if (w->thread_count == 0)
		return;

	// Make read_buf large enough for either a page group or a block group
	size_t read_buf_size = w->read.pgblk_group[0] * USZRAM_PAGE_SIZE,
	       buf_blksize = w->read.pgblk_group[1] * USZRAM_BLOCK_SIZE;
	if (w->read.percent_blks >= 100 || read_buf_size < buf_blksize)
		read_buf_size = buf_blksize;

	size_t write_buf_size = w->write.pgblk_group[0] * USZRAM_PAGE_SIZE;
	buf_blksize = w->write.pgblk_group[1] * USZRAM_BLOCK_SIZE;
	if (w->write.percent_blks >= 100 || write_buf_size < buf_blksize)
		write_buf_size = buf_blksize;

	unsigned char compr_count = w->compr_max - w->compr_min;
	char **write_data = malloc((sizeof *write_data) * compr_count);
	char data_name[20] = "../data/cr";
	int compr_chars;
	for (unsigned char i = 0; i < compr_count; ++i) {
		write_data[i] = malloc(write_buf_size);
		compr_chars = snprintf(data_name + 10, 6, "%i-%i",
				       w->compr_min + i, w->compr_min + i + 1);
		strncpy(data_name + 10 + compr_chars, ".raw", 5);
		FILE *data = fopen(data_name, "r");
		fread(write_data[i], 1, write_buf_size, data);
		fclose(data);
	}

	uszram_init();
	struct thread_data *td = malloc((sizeof *td) * w->thread_count);
	pthread_t *threads;
	if (w->thread_count > 1)
		threads = malloc((sizeof *threads) * (w->thread_count - 1));
	double duration = 0;
	struct timespec start, end;
	timespec_get(&start, TIME_UTC);
	for (unsigned i = 0; i < w->thread_count; ++i) {
		td[i].id = i;
		// Give each thread a different random seed
		td[i].seed = rand();
		td[i].w = w;
		td[i].read_buf = malloc(read_buf_size);
		td[i].write_data = write_data;
		td[i].compr_count = compr_count;
		if (i)
			pthread_create(threads + i - 1, NULL, run_thread,
				       td + i);
	}
	run_thread(td);
	for (unsigned i = 1; i < w->thread_count; ++i)
		pthread_join(threads[i], NULL);
	timespec_get(&end, TIME_UTC);
	duration = end.tv_sec - start.tv_sec
		   + (end.tv_nsec - start.tv_nsec) / 1e9;

	printf("%"PRIuLEAST64" requests in %.4f s\n", w->request_count,
	       duration);
	free(threads);
	free(td);
	print_stats();
	uszram_exit();
	for (unsigned char i = 0; i < compr_count; ++i)
		free(write_data[i]);
	free(write_data);
}
