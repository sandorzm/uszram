#ifndef ZAPI_ALLOC_H
#define ZAPI_ALLOC_H


#include "../compr-api.h"
// #include "zapi.h"


inline static int get_size(struct page *pg)
{
	return zapi_get_size(pg->data);
}

inline static char *get_raw(struct page *pg)
{
	return zapi_get_raw_data(pg->data);
}


#endif // ZAPI_ALLOC_H
