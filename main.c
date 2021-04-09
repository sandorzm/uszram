/* COMPILE:
 * clang main.c uszram.c -llz4 [-ljemalloc] -lpthread [-ldl -lm -static]
 */
#include <stdio.h>

#include "uszram.h"
#include "test/small-test.h"
#include "test/large-test.h"

int main(void)
{
	run_small_tests();
	many_pgs_test(1, 1u << 15, 4096);
	many_blks_test(1, 1u << 18, 512);
	return 0;
}
