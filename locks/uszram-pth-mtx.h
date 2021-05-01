#ifndef USZRAM_PTH_MTX_H
#define USZRAM_PTH_MTX_H


#include <pthread.h>

#include "../locks-api.h"


struct lock {
	pthread_mutex_t mutex;
};

static inline int initialize_lock(struct lock *lock)
{
	return pthread_mutex_init((pthread_mutex_t *)lock, NULL);
}

static inline int destroy_lock(struct lock *lock)
{
	return pthread_mutex_destroy((pthread_mutex_t *)lock);
}

static inline int lock_as_reader(struct lock *lock)
{
	return pthread_mutex_lock((pthread_mutex_t *)lock);
}

static inline int lock_as_writer(struct lock *lock)
{
	return pthread_mutex_lock((pthread_mutex_t *)lock);
}

static inline int unlock_as_reader(struct lock *lock)
{
	return pthread_mutex_unlock((pthread_mutex_t *)lock);
}

static inline int unlock_as_writer(struct lock *lock)
{
	return pthread_mutex_unlock((pthread_mutex_t *)lock);
}


#endif // USZRAM_PTH_MTX_H
