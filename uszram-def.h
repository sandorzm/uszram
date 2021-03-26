#ifndef USZRAM_DEF_H
#define USZRAM_DEF_H


#include <string.h>
#include <stddef.h>

#include "uszram.h"


#if USZRAM_PAGE_SHIFT <= 15
   typedef uint_least16_t size_type;
#else
   typedef uint_least32_t size_type;
#endif


#define BLK_PER_PG    USZRAM_BLK_PER_PG
#define PG_PER_LOCK   USZRAM_PG_PER_LOCK
#define LOCK_COUNT    ((USZRAM_PAGE_COUNT - 1) / PG_PER_LOCK + 1)
#define MAX_NON_HUGE  ((size_type)(USZRAM_MAX_COMPR_FRAC * USZRAM_PAGE_SIZE))


#endif // USZRAM_DEF_H
