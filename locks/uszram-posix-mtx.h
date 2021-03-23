#ifndef USZRAM_POSIX_MTX_H
#define USZRAM_POSIX_MTX_H


#include <pthread.h>

#include "../locks-api.h"


struct lock {
	pthread_mutex_t mutex;
};

inline static int initialize_lock(struct lock *lock)
{
	return pthread_mutex_init((pthread_mutex_t *)lock, NULL);
}

inline static int destroy_lock(struct lock *lock)
{
	return pthread_mutex_destroy((pthread_mutex_t *)lock);
}

inline static int lock_as_reader(struct lock *lock)
{
	return pthread_mutex_lock((pthread_mutex_t *)lock);
}

inline static int lock_as_writer(struct lock *lock)
{
	return pthread_mutex_lock((pthread_mutex_t *)lock);
}

inline static int unlock_as_reader(struct lock *lock)
{
	return pthread_mutex_unlock((pthread_mutex_t *)lock);
}

inline static int unlock_as_writer(struct lock *lock)
{
	return pthread_mutex_unlock((pthread_mutex_t *)lock);
}


#endif // USZRAM_POSIX_MTX_H
