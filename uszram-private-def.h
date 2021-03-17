#ifndef USZRAM_PRIVATE_DEF_H
#define USZRAM_PRIVATE_DEF_H


#include <stddef.h>

#include "uszram.h"


#define BLK_PER_PG    (1 << (USZRAM_PAGE_SHIFT - USZRAM_BLOCK_SHIFT))
#define PG_ADDR_MAX   (USZRAM_BLK_ADDR_MAX / BLK_PER_PG)
#define LOCK_ADDR_MAX (PG_ADDR_MAX / USZRAM_PG_PER_LOCK)


#if USZRAM_PAGE_SHIFT <= 15
   typedef uint_least16_t size_type;
#else
   typedef uint_least32_t size_type;
#endif


#endif // USZRAM_PRIVATE_DEF_H
