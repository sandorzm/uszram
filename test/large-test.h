#ifndef LARGE_TEST_H
#define LARGE_TEST_H


#include <stdint.h>

#include "workload.h"


void many_pgs_test(uint_least32_t pg_group, uint_least32_t num_pg_grp,
		   unsigned pg_fill);
void many_blks_test(uint_least32_t blk_group, uint_least32_t num_blk_grp,
		    unsigned blk_fill);


#endif // LARGE_TEST_H
