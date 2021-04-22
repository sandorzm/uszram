#include <stdio.h>
#include <string.h>
#include <time.h>
#include <inttypes.h>

#include "workload.h"
#include "../uszram.h"

#ifdef USZRAM_STD_MTX
#  include <threads.h>
#  define THREAD_CREATE(thr, func, arg) thrd_create(thr, func, arg)
#  define THREAD_JOIN(thr) thrd_join(thr, NULL)
   typedef thrd_t  thread_type;
   typedef int     thread_ret;
#else
#  include <pthread.h>
#  define THREAD_CREATE(thr, func, arg) pthread_create(thr, NULL, func, arg)
#  define THREAD_JOIN(thr) pthread_join(thr, NULL)
   typedef pthread_t  thread_type;
   typedef void      *thread_ret;
#endif

#define DATA_FILE_SIZE 4096


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
				unsigned long op_rand,
				unsigned long addr_rand)
{
	const _Bool blk = op_rand % 100 < td->w->read.percent_blks;
	const uint_least32_t count = td->w->read.pgblk_group[blk];
	const uint_least64_t max_count = uszram_counts[blk] - count + 1;
	const uint_least32_t addr = addr_rand % max_count;
	read_fns[blk](addr, count, td->read_buf);
}

static inline void call_write_fn(struct thread_data *td,
				 unsigned long op_rand,
				 unsigned long addr_rand)
{
	long compr_rand;
	mrand48_r(&td->rand_data, &compr_rand);
	const unsigned char compr = (unsigned long)compr_rand
				    % td->compr_count;

	const _Bool blk = op_rand % 100 < td->w->write.percent_blks;
	const uint_least32_t count = td->w->write.pgblk_group[blk];
	const uint_least64_t max_count = uszram_counts[blk] - count + 1;
	const uint_least32_t addr = addr_rand % max_count;
	write_fns[blk](addr, count, td->write_data[compr]);
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

static inline int get_write_data(size_t buf_size, char buf[static buf_size],
				 unsigned char compr)
{
	// Max length 16, plus null terminator: "data/cr11-12.raw"
	char filename[17] = "data/cr";
	snprintf(filename + 7, 10, "%hhu-%u.raw", compr, compr + 1u);
	FILE *data = fopen(filename, "r");
	if (data == NULL) {
		PRINT_ERROR("Can't open %s\n", filename);
		return -1;
	}
	size_t ret = fread(buf, 1, buf_size, data);
	fclose(data);
	if (ret != buf_size) {
		PRINT_ERROR("Error reading %s\n", filename);
		return -1;
	}
	return 0;
}

static thread_ret pop_thread(void *tdata)
{
	struct thread_data *const td = tdata;
	uint_least32_t pg_group = DATA_FILE_SIZE / USZRAM_PAGE_SIZE;
	uint_least64_t pg_count = (td->w->request_count < USZRAM_PAGE_COUNT ?
				   td->w->request_count : USZRAM_PAGE_COUNT),
		       req_pgs = pg_count / td->w->thread_count;
	uint_least32_t addr_first = req_pgs * td->id;

	// First thread gets the leftover requests
	if (td->id == 0)
		req_pgs += pg_count % td->w->thread_count;
	else
		addr_first += pg_count % td->w->thread_count;

	uint_least32_t addr_end = addr_first + req_pgs / pg_group * pg_group,
		       last_group = req_pgs % pg_group;

	long compr_rand;
	unsigned char compr;
	uint_least32_t addr = addr_first;
	for (; addr != addr_end; addr += pg_group) {
		mrand48_r(&td->rand_data, &compr_rand);
		compr = (unsigned long)compr_rand % td->compr_count;
		uszram_write_pg(addr, pg_group, td->write_data[compr]);
	}
	if (last_group) {
		mrand48_r(&td->rand_data, &compr_rand);
		compr = (unsigned long)compr_rand % td->compr_count;
		uszram_write_pg(addr, last_group, td->write_data[compr]);
	}
	return 0;
}

// Contains code common to both reading and writing
static thread_ret run_thread(void *tdata)
{
	struct thread_data *const td = tdata;
	uint_least64_t req = td->w->request_count / td->w->thread_count;

	// First thread gets the leftover requests
	if (td->id == 0)
		req += td->w->request_count % td->w->thread_count;

	long op_rand, addr_rand;
	_Bool write;
	for (uint_least64_t i = 0; i < req; ++i) {
		mrand48_r(&td->rand_data, &op_rand);
		write = (unsigned long)op_rand % 100 < td->w->percent_writes;
		mrand48_r(&td->rand_data, &op_rand);
		mrand48_r(&td->rand_data, &addr_rand);
		if (write)
			call_write_fn(td, op_rand, addr_rand);
		else
			call_read_fn(td, op_rand, addr_rand);
	}
	return 0;
}

void populate_store(const struct workload *w, struct test_timer *t)
{
	if (!valid_compr(w->compr_min, w->compr_max) || w->thread_count == 0) {
		PRINT_ERROR("Invalid workload\n");
		return;
	} else if (USZRAM_PAGE_SIZE > DATA_FILE_SIZE) {
		PRINT_ERROR("Page size > %i bytes: %u\n",
			    DATA_FILE_SIZE, USZRAM_PAGE_SIZE);
		return; // Data files are only DATA_FILE_SIZE bytes
	}

	unsigned char compr_count = w->compr_max - w->compr_min;
	char **const write_data = malloc((sizeof *write_data) * compr_count);
	if (write_data == NULL) {
		PRINT_ERROR("malloc ran out of memory\n");
		return;
	}
	for (unsigned char i = 0; i < compr_count; ++i) {
		write_data[i] = malloc(DATA_FILE_SIZE);
		if (write_data[i] == NULL)
			PRINT_ERROR("malloc ran out of memory\n");
		else if (!get_write_data(DATA_FILE_SIZE, write_data[i],
					 w->compr_min + i))
			continue;
		compr_count = i + 1;
		goto out_write_data;
	}

	unsigned last_thread = w->thread_count - 1;
	thread_type *threads = NULL;
	if (last_thread > 0) {
		threads = malloc((sizeof *threads) * last_thread);
		if (threads == NULL) {
			PRINT_ERROR("malloc ran out of memory\n");
			goto out_threads;
		}
	}
	struct thread_data *const td = malloc((sizeof *td) * w->thread_count);
	if (td == NULL) {
		PRINT_ERROR("malloc ran out of memory\n");
		goto out_threads;
	}

	td[last_thread].seed = -2091457876; // Arbitrary initial seed
	srand48_r(td[last_thread].seed, &td[last_thread].rand_data);

	start_timer(t);
	for (unsigned i = 0; i < w->thread_count; ++i) {
		td[i].id = i;
		td[i].w = w;
		td[i].write_data = write_data;
		td[i].compr_count = compr_count;
		if (i != last_thread) {
			// Give each thread a different random seed
			mrand48_r(&td[last_thread].rand_data, &td[i].seed);
			srand48_r(td[i].seed, &td[i].rand_data);
			THREAD_CREATE(threads + i, pop_thread, td + i);
		}
	}
	pop_thread(td + last_thread);
	for (unsigned i = 0; i < last_thread; ++i)
		THREAD_JOIN(threads[i]);
	stop_timer(t);

	free(td);
out_threads:
	free(threads);
out_write_data:
	for (unsigned char i = 0; i < compr_count; ++i)
		free(write_data[i]);
	free(write_data);
}

void run_workload(const struct workload *w, struct test_timer *t)
{
	if (!valid_workload(w)) {
		PRINT_ERROR("Invalid workload\n");
		return;
	}

	// Make read_buf and write_data large enough for either a page group or
	// a block group
	const size_t read_buf_size  = buf_size(w->read),
		     write_buf_size = buf_size(w->write);
	if (write_buf_size > DATA_FILE_SIZE) {
		PRINT_ERROR("Write group > %i bytes: %zu\n",
			    DATA_FILE_SIZE, write_buf_size);
		return; // Data files are only DATA_FILE_SIZE bytes
	}

	unsigned char compr_count = w->compr_max - w->compr_min;
	char **const write_data = malloc((sizeof *write_data) * compr_count);
	if (write_data == NULL) {
		PRINT_ERROR("malloc ran out of memory\n");
		return;
	}
	for (unsigned char i = 0; i < compr_count; ++i) {
		write_data[i] = malloc(write_buf_size);
		if (write_data[i] == NULL)
			PRINT_ERROR("malloc ran out of memory\n");
		else if (!get_write_data(write_buf_size, write_data[i],
					 w->compr_min + i))
			continue;
		compr_count = i + 1;
		goto out_write_data;
	}

	unsigned last_thread = w->thread_count - 1;
	thread_type *threads = NULL;
	if (last_thread > 0) {
		threads = malloc((sizeof *threads) * last_thread);
		if (threads == NULL) {
			PRINT_ERROR("malloc ran out of memory\n");
			goto out_threads;
		}
	}
	struct thread_data *const td = malloc((sizeof *td) * w->thread_count);
	if (td == NULL) {
		PRINT_ERROR("malloc ran out of memory\n");
		goto out_threads;
	}

	td[last_thread].seed = -613048132; // Arbitrary initial seed
	srand48_r(td[last_thread].seed, &td[last_thread].rand_data);

	start_timer(t);
	for (unsigned i = 0; i < w->thread_count; ++i) {
		td[i].id = i;
		td[i].w = w;
		td[i].read_buf = malloc(read_buf_size);
		if (td[i].read_buf == NULL) {
			PRINT_ERROR("malloc ran out of memory\n");
			last_thread = i;
			goto out_join;
		}
		td[i].write_data = write_data;
		td[i].compr_count = compr_count;
		if (i != last_thread) {
			// Give each thread a different random seed
			mrand48_r(&td[last_thread].rand_data, &td[i].seed);
			srand48_r(td[i].seed, &td[i].rand_data);
			THREAD_CREATE(threads + i, run_thread, td + i);
		}
	}
	run_thread(td + last_thread);
out_join:
	for (unsigned i = 0; i < last_thread; ++i)
		THREAD_JOIN(threads[i]);
	stop_timer(t);

	for (unsigned i = 0; i <= last_thread; ++i)
		free(td[i].read_buf);
	free(td);
out_threads:
	free(threads);
out_write_data:
	for (unsigned char i = 0; i < compr_count; ++i)
		free(write_data[i]);
	free(write_data);
}
