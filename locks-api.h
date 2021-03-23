#ifndef LOCKS_API_H
#define LOCKS_API_H


struct lock;

inline static int initialize_lock(struct lock *lock);
inline static int destroy_lock(struct lock *lock);
inline static int lock_as_reader(struct lock *lock);
inline static int lock_as_writer(struct lock *lock);
inline static int unlock_as_reader(struct lock *lock);
inline static int unlock_as_writer(struct lock *lock);


#endif // LOCKS_API_H
