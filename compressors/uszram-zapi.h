#ifndef ZAPI_ALLOC_H
#define ZAPI_ALLOC_H


#include "compr.h"
// #include "zapi.h"


struct page {
	char *zapi_data;
};

inline static int get_size(struct page *pg)
{
	return zapi_get_size(pg->zapi_data);
}

inline static char *get_alloc(struct page *pg)
{
	return pg->zapi_data;
}

inline static char *get_raw(struct page *pg)
{
	return zapi_get_raw_data(pg->zapi_data);
}


#endif // ZAPI_ALLOC_H
