#ifndef USZRAM_DEF_H
#define USZRAM_DEF_H


#include "uszram.h"


#if USZRAM_PAGE_SHIFT <= 15
   typedef uint_least16_t size_type;
#else
   typedef uint_least32_t size_type;
#endif


#define BLK_PER_PG    USZRAM_BLK_PER_PG
#define MAX_NHUGE_MUL ((uint_least64_t)USZRAM_PAGE_SIZE	\
		       * USZRAM_MAX_NHUGE_PERCENT)
#define MAX_NON_HUGE  ((MAX_NHUGE_MUL > 100u ? MAX_NHUGE_MUL : 100u) / 100u)


#endif // USZRAM_DEF_H
