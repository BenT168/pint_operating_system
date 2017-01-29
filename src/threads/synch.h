#ifndef THREADS_SYNCH_H
#define THREADS_SYNCH_H

#include <list.h>
#include <stdbool.h>

/* Definition: The priority ceiling of a lock (binary semaphore / mutex)
               is the priority of the highest priority thread that may acquire
               the lock.
*/

/* A counting semaphore. */
struct semaphore
  {
    unsigned value;             /* Current value. */
    struct list waiters;        /* List of waiting threads. */
  };

void sema_init (struct semaphore *, unsigned value);
void sema_down (struct semaphore *);
bool sema_try_down (struct semaphore *);
void sema_up (struct semaphore *);
void sema_self_test (void);

/* Lock. */
struct lock
  {
    struct thread *holder;       /* Thread holding lock (for debugging). */
    struct semaphore semaphore;  /* Binary semaphore controlling access. */
    int priority_ceiling;   /* Priority ceiling of semaphore. */
    struct list_elem acq_elem;   /* List element for 'acquired_locks' list */
    struct list_elem allelem;    /* List element. */
  };

void lock_init (struct lock *);
void lock_acquire (struct lock *);
bool lock_try_acquire (struct lock *);
void lock_release (struct lock *);
bool lock_held_by_current_thread (const struct lock *);

/* Condition variable. */
struct condition
  {
    struct list waiters;        /* List of waiting semaphore_elems. */
  };

void cond_init (struct condition *);
void cond_wait (struct condition *, struct lock *);
void cond_signal (struct condition *, struct lock *);
void cond_broadcast (struct condition *, struct lock *);

/* Optimization barrier.

   The compiler will not reorder operations across an
   optimization barrier.  See "Optimization Barriers" in the
   reference guide for more information.*/
#define barrier() asm volatile ("" : : : "memory")

#endif /* threads/synch.h */
