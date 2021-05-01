#ifndef USZRAM_PTH_RW_H
#define USZRAM_PTH_RW_H


#include <pthread.h>

#include "../locks-api.h"


struct lock {
	pthread_rwlock_t rwlock;
};

static inline int initialize_lock(struct lock *lock)
{
	return pthread_rwlock_init((pthread_rwlock_t *)lock, NULL);
}

static inline int destroy_lock(struct lock *lock)
{
	return pthread_rwlock_destroy((pthread_rwlock_t *)lock);
}

static inline int lock_as_reader(struct lock *lock)
{
	return pthread_rwlock_rdlock((pthread_rwlock_t *)lock);
}

static inline int lock_as_writer(struct lock *lock)
{
	return pthread_rwlock_wrlock((pthread_rwlock_t *)lock);
}

static inline int unlock_as_reader(struct lock *lock)
{
	return pthread_rwlock_unlock((pthread_rwlock_t *)lock);
}

static inline int unlock_as_writer(struct lock *lock)
{
	return pthread_rwlock_unlock((pthread_rwlock_t *)lock);
}


#endif // USZRAM_PTH_RW_H
