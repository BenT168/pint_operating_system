#ifndef DEVICES_TIMER_H
#define DEVICES_TIMER_H

#include <round.h>
#include <stdint.h>
#include <list.h>

/* Number of timer interrupts per second. */
#define TIMER_FREQ 100

void timer_init (void);
void timer_calibrate (void);

int64_t timer_ticks (void);
int64_t timer_elapsed (int64_t);

/* Sleep and yield the CPU to other threads. */
void timer_sleep (int64_t ticks);
void timer_msleep (int64_t milliseconds);
void timer_usleep (int64_t microseconds);
void timer_nsleep (int64_t nanoseconds);

/* Busy waits. */
void timer_mdelay (int64_t milliseconds);
void timer_udelay (int64_t microseconds);
void timer_ndelay (int64_t nanoseconds);

void timer_print_stats (void);

/* 't1' and 't2' are pointers to the threads in which the list elements, 'elem1'
   and 'elem2' are embedded.
   The following function implements a binary relation 'R' on the set of all
   threads 'T' defined as follows:
     forall t1,t2 belonging to T, R(t1, t2) iff
       ( t1->wakeup_time <= t2->wakeup_time ) OR
       ( t1->wakeup_time == t2->wakeup_time AND t1->priority >= t2->priority )
   We return true iff (t1,t2) belongs to R and false otherwise.
   The auxiliary element 'aux' is unused and needed only to satisfy the
   interface of the list 'less' function. */
bool comparator_wakeup_time (const struct list_elem *elem1,
                             const struct list_elem *elem2,
                             void *aux);

#endif /* devices/timer.h */
