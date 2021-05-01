#ifndef LOCKS_API_H
#define LOCKS_API_H


struct lock;

static inline int initialize_lock(struct lock *lock);
static inline int destroy_lock(struct lock *lock);
static inline int lock_as_reader(struct lock *lock);
static inline int lock_as_writer(struct lock *lock);
static inline int unlock_as_reader(struct lock *lock);
static inline int unlock_as_writer(struct lock *lock);


#endif // LOCKS_API_H
