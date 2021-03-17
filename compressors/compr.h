#ifndef COMPR_H
#define COMPR_H


#include "../uszram-private.h"


struct page;

inline static int   get_size(struct page *pg);
inline static char *get_alloc(struct page *pg);
inline static char *get_raw(struct page *pg);

// inline static void set_size

static int compress(char *src, char *dest);
static int decompress(struct page *pg, char *dest, int bytes);
static int update_blk(struct page *pg, char *new_blk, int offset);


#endif // COMPR_H
