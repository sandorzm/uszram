#ifndef ALLOCATORS_H
#define ALLOCATORS_H


#include "../uszram-private.h"


int get_class_size(int size);
void maybe_realloc(struct uszram_page *pg, int new_size);


#endif // ALLOCATORS_H
