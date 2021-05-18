#ifndef USZRAM_PAGE_H
#define USZRAM_PAGE_H


#include "uszram-def.h"

#ifdef USZRAM_BASIC
#  include "allocators/uszram-basic-def.h"
#endif

#ifdef USZRAM_LZ4
#  include "compressors/uszram-lz4-def.h"
#else
#  include "compressors/uszram-zapi-def.h"
#endif

#include "cache-api.h"
#ifdef USZRAM_LIST2_CACHE
#  include "caches/list2-cache.h"
#endif


struct page {
	char *_Atomic data;
#ifndef NO_ALLOC_METADATA
	struct alloc_data alloc_data;
#endif
#ifndef NO_COMPR_METADATA
	struct compr_data compr_data;
#endif
#ifndef USZRAM_NO_CACHING
	struct cache_data cache_data;
#endif
};


#endif // USZRAM_PAGE_H
