/* COMPILE:
 * clang main.c uszram.c -llz4 [-ljemalloc] -lpthread [-ldl -lm -static]
 */
#include <stdio.h>

#include "uszram.h"

int main(void)
{
	uszram_init();
	char pg1[4096] = {5, 4, 3, 4, 4, 9}, pg2[4096];
	uszram_write_pg(0, 1, pg1);
	uszram_read_pg(0, 1, pg2);
	for (unsigned char i = 0; i < 10; ++i)
		printf("%u\n", pg2[i]);
	uszram_exit();
	return 0;
}
