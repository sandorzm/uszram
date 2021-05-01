#ifndef USZRAM_DEF_H
#define USZRAM_DEF_H


#include "uszram.h"


#if USZRAM_PAGE_SHIFT <= 15
#  define SIZE_SHIFT 15
   typedef uint_least16_t size_type;
#else
#  define SIZE_SHIFT 31
   typedef uint_least32_t size_type;
#endif

struct range {
	size_type offset, count;
};

#define RNG(o, c) (struct range){.offset = (o), .count = (c)}

#define BLOCK_SIZE    USZRAM_BLOCK_SIZE
#define PAGE_SIZE     USZRAM_PAGE_SIZE
#define BLK_PER_PG    USZRAM_BLK_PER_PG
#define PG_PER_LOCK   USZRAM_PG_PER_LOCK
#define LOCK_COUNT    ((USZRAM_PAGE_COUNT - 1) / PG_PER_LOCK + 1)
#define MAX_NHUGE_MUL ((uint_least64_t)PAGE_SIZE * USZRAM_MAX_NHUGE_PERCENT)
#define MAX_NON_HUGE  ((MAX_NHUGE_MUL > 100u ? MAX_NHUGE_MUL : 100u) / 100u)


#endif // USZRAM_DEF_H
