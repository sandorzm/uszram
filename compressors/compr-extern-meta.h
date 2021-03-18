#ifndef COMPR_EXTERN_META_H
#define COMPR_EXTERN_META_H


#include "../compr-api.h"


inline static _Bool is_huge(struct page *pg)
{
	return pg->compr_data.size & (1 << USZRAM_PAGE_SHIFT);
}

inline static int get_size(struct page *pg)
{
	return is_huge(pg) ? USZRAM_PAGE_SIZE : pg->compr_data.size;
}

inline static char *get_raw(struct page *pg)
{
	return pg->data;
}


#endif // COMPR_EXTERN_META_H
