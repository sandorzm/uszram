#ifndef USZRAM_MUTEX_H
#define USZRAM_MUTEX_H


#include <stdatomic.h>


typedef struct atomic_flag lock_type;

static inline void initialize_lock(lock_type *lk)
{
	atomic_flag_clear(lk);
}

static inline void lock_as_reader(lock_type *lk)
{
	while (atomic_flag_test_and_set(lk));
}

static inline void lock_as_writer(lock_type *lk)
{
	lock_as_reader(lk);
}

static inline void unlock_as_reader(lock_type *lk)
{
	atomic_flag_clear(lk);
}

static inline void unlock_as_writer(lock_type *lk)
{
	unlock_as_reader(lk);
}


#endif // USZRAM_MUTEX_H
