#ifndef WORKLOAD_H
#define WORKLOAD_H


#include <stdint.h>
#include <stdlib.h>


struct rw_workload {
	unsigned char   percent_blks;
	uint_least32_t  pgblk_group[2];
};

// Everything needed to specify how to run a test
struct workload {
	unsigned char       percent_writes,
			    compr_min,
			    compr_max;
	uint_least64_t      request_count;
	unsigned            thread_count;
	struct rw_workload  read, write;
};


#endif // WORKLOAD_H
