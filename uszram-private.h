#ifndef USZRAM_PRIVATE_H
#define USZRAM_PRIVATE_H


#include "uszram.h"


#define BLK_PER_PG    (1 << (USZRAM_PAGE_SHIFT - USZRAM_BLOCK_SHIFT))
#define PG_ADDR_MAX   (USZRAM_BLK_ADDR_MAX / BLK_PER_PG)
#define LOCK_ADDR_MAX (PG_ADDR_MAX / USZRAM_PG_PER_LOCK)

/* The lower USZRAM_PAGE_SHIFT bits of uszram_page.flags are for compressed
 * size; the higher bits are for uszram_pgflags. When compressed size is its
 * maximum of USZRAM_PAGE_SIZE (incompressible page), the HUGE_PAGE flag is set
 * and the size field is zero. From zram source: "zram is mainly used for memory
 * efficiency so we want to keep memory footprint small so we can squeeze size
 * and flags into a field."
 */
enum uszram_pgflags {
	HUGE_PAGE = USZRAM_PAGE_SHIFT,	// Incompressible page, store raw data
};

#if USZRAM_PAGE_SHIFT <= 15		// Reduce when more flags are added
  typedef uint_least16_t flags_type;
#else
  typedef uint_least32_t flags_type;
#endif

#define GET_PG_SIZE(pg) ((int)(pg->flags		\
			       & (((uint_least32_t)1 << (HUGE_PAGE + 1)) - 1)))


struct uszram_page {
	char *_Atomic data;
	flags_type flags;
};


#endif // USZRAM_PRIVATE_H
