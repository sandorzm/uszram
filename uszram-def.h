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


#define BLK_PER_PG    (1 << (USZRAM_PAGE_SHIFT - USZRAM_BLOCK_SHIFT))
#define PG_ADDR_MAX   (USZRAM_BLK_ADDR_MAX / BLK_PER_PG)
#define LOCK_ADDR_MAX (PG_ADDR_MAX / USZRAM_PG_PER_LOCK)
#define MAX_NON_HUGE  ((size_type)(USZRAM_MAX_COMPR_FRAC * USZRAM_PAGE_SIZE))


#endif // USZRAM_DEF_H
