#ifndef USZRAM_RWLOCK_H
#define USZRAM_RWLOCK_H


#include <stdatomic.h>


typedef atomic_uchar lock_type;

static inline void initialize_lock(lock_type *lk)
{
	(void)lk;
}

static inline void lock_as_reader(lock_type *lk)
{
	unsigned char old = *lk;
	while (1) {
		if (old == 0xFF) {
			old = *lk;
			continue;
		}
		unsigned char new = old + 1;
		if (atomic_compare_exchange_weak(lk, &old, new))
			break;
	}
}

static inline void lock_as_writer(lock_type *lk)
{
	unsigned char zero = 0;
	while (!atomic_compare_exchange_weak(lk, &zero, 0xFF));
}

static inline void unlock_as_reader(lock_type *lk)
{
	--*lk;
}

static inline void unlock_as_writer(lock_type *lk)
{
	*lk = 0;
}


#endif // USZRAM_RWLOCK_H
