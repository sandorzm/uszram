#ifndef LOCKS_API_H
#define LOCKS_API_H


#include "uszram-page.h"


inline static void initialize_lock(lock_type *lock);
inline static void lock_as_reader(lock_type *lock);
inline static void lock_as_writer(lock_type *lock);
inline static void unlock_as_reader(lock_type *lock);
inline static void unlock_as_writer(lock_type *lock);


#endif // LOCKS_API_H
