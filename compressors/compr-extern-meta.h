#ifndef COMPR_EXTERN_META_H
#define COMPR_EXTERN_META_H


#include <stddef.h>

#include "compr.h"


#if USZRAM_PAGE_SHIFT <= 15
   typedef uint_least16_t size_type;
#else
   typedef uint_least32_t size_type;
#endif


struct page {
	char *_Atomic data;
	size_type size;
};

inline static int get_size(struct page *pg)
{
	return pg->size;
}

inline static char *get_alloc(struct page *pg)
{
	return pg->data;
}

inline static char *get_raw(struct page *pg)
{
	if (pg->size == USZRAM_PAGE_SIZE)
		return pg->data;
	return NULL;
}


#endif // COMPR_EXTERN_META_H
