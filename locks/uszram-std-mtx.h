#ifndef USZRAM_STD_MTX_H
#define USZRAM_STD_MTX_H


#include <threads.h>

#include "../locks-api.h"


struct lock {
	mtx_t mutex;
};

static inline int initialize_lock(struct lock *lock)
{
	return mtx_init((mtx_t *)lock, mtx_plain);
}

static inline int destroy_lock(struct lock *lock)
{
	mtx_destroy((mtx_t *)lock);
	return 0;
}

static inline int lock_as_reader(struct lock *lock)
{
	return mtx_lock((mtx_t *)lock);
}

static inline int lock_as_writer(struct lock *lock)
{
	return mtx_lock((mtx_t *)lock);
}

static inline int unlock_as_reader(struct lock *lock)
{
	return mtx_unlock((mtx_t *)lock);
}

static inline int unlock_as_writer(struct lock *lock)
{
	return mtx_unlock((mtx_t *)lock);
}


#endif // USZRAM_STD_MTX_H
