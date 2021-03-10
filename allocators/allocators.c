#include "allocators.h"


int get_class_size(int size)
{
	if (size <= USZRAM_SLAB_MIN)
		return USZRAM_SLAB_MIN;
	size -= USZRAM_SLAB_MIN;
	size = size / USZRAM_SLAB_INCR + (size % USZRAM_SLAB_INCR != 0);
	size = USZRAM_SLAB_MIN + size * USZRAM_SLAB_INCR;
	return size < USZRAM_PAGE_SIZE ? size : USZRAM_PAGE_SIZE;
}
