#ifndef USZRAM_LZ4_H
#define USZRAM_LZ4_H


#include "compr-extern-meta.h"


static int maybe_recompress(struct page *pg, const char *new_data, int offset,
			    int blocks, char *dest)
{
	unsigned char mask = (1 << 6) - 1;
	unsigned char updates = (pg->compr_data.size & mask) + blocks;
	if (updates >= 64)
		return compress(get_raw(pg), dest);
	pg->compr_data.size &= ~mask | updates;
	return 0;
}

static int read_mod_write(struct page *pg, const char *new_data, int offset,
			  int blocks, char *dest)
{
	char *raw_pg = get_raw(pg);
	if (raw_pg != NULL) {

	}
	return 0;
}


#endif // USZRAM_LZ4_H
