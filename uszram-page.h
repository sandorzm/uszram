#ifndef USZRAM_PAGE_H
#define USZRAM_PAGE_H


#include <stdatomic.h>

#include "uszram-def.h"

#ifdef USZRAM_BASIC
#  include "allocators/uszram-basic-def.h"
#endif

#ifdef USZRAM_LZ4
#  include "compressors/uszram-lz4-def.h"
#else
#  include "compressors/uszram-zapi-def.h"
#endif

#ifdef USZRAM_RWLOCK
#  include "locks/uszram-rwlock.h"
#else
#  include "locks/uszram-mutex.h"
#endif


struct page {
	char *_Atomic data;
	lock_type lock;
#ifndef NO_ALLOC_METADATA
	struct alloc_data alloc_data;
#endif
#ifndef NO_COMPR_METADATA
	struct compr_data compr_data;
#endif
};


#include "locks-api.h"


#endif // USZRAM_PAGE_H
