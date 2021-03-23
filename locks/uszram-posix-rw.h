#ifndef USZRAM_POSIX_RW_H
#define USZRAM_POSIX_RW_H


#include <pthread.h>

#include "../locks-api.h"


struct lock {
	pthread_rwlock_t rwlock;
};

inline static int initialize_lock(struct lock *lock)
{
	return pthread_rwlock_init((pthread_rwlock_t *)lock, NULL);
}

inline static int destroy_lock(struct lock *lock)
{
	return pthread_rwlock_destroy((pthread_rwlock_t *)lock);
}

inline static int lock_as_reader(struct lock *lock)
{
	return pthread_rwlock_rdlock((pthread_rwlock_t *)lock);
}

inline static int lock_as_writer(struct lock *lock)
{
	return pthread_rwlock_wrlock((pthread_rwlock_t *)lock);
}

inline static int unlock_as_reader(struct lock *lock)
{
	return pthread_rwlock_unlock((pthread_rwlock_t *)lock);
}

inline static int unlock_as_writer(struct lock *lock)
{
	return pthread_rwlock_unlock((pthread_rwlock_t *)lock);
}


#endif // USZRAM_POSIX_RW_H
