#ifndef USZRAM_BASIC_H
#define USZRAM_BASIC_H


#include "alloc.h"


static char *maybe_realloc(char *data, int data_size, int new_size)
{
	if (data == NULL || data_size != new_size) {
		free(data);
		return calloc(new_size, 1);
	}
	return data;
}


#endif // USZRAM_BASIC_H
