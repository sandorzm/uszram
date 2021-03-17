#ifndef COMPR_EXTERN_META_H
#define COMPR_EXTERN_META_H


#include "compr-api.h"


inline static int get_size(struct page *pg)
{
	return pg->compr_data.size;
}

inline static char *get_raw(struct page *pg)
{
	if (pg->compr_data.size >= USZRAM_PAGE_SIZE)
		return pg->data;
	return NULL;
}


#endif // COMPR_EXTERN_META_H
