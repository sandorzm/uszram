#ifndef COMPR_API_H
#define COMPR_API_H


#include "uszram-page.h"


inline static _Bool is_huge(struct page *pg);
inline static int   get_size(struct page *pg);
inline static char *get_raw(struct page *pg);

static int compress(char *src, char *dest);
static int decompress(struct page *pg, char *dest, int bytes);
static int maybe_recompress(struct page *pg, const char *new_data, int offset,
			    int blocks, char *dest);
static int read_mod_write(struct page *pg, const char *new_data, int offset,
			  int blocks, char *dest);


#endif // COMPR_API_H
