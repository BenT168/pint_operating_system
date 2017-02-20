#ifndef THREADS_THREAD_H
#define THREADS_THREAD_H

#include <debug.h>
#include <list.h>
#include <stdint.h>
#include "threads/synch.h"

/* States in a thread's life cycle. */
enum thread_status
  {
    THREAD_RUNNING,     /* Running thread. */
    THREAD_READY,       /* Not running but ready to run. */
    THREAD_BLOCKED,     /* Waiting for an event to trigger. */
    THREAD_DYING        /* About to be destroyed. */
  };

/* Thread identifier type.
   You can redefine this to whatever type you like. */
typedef int tid_t;

/* TASK 2: Process identifier type. */
typedef int pid_t;

#define TID_ERROR ((tid_t) -1)          /* Error value for tid_t. */

/* Thread priorities. */
#define PRI_MIN 0                       /* Lowest priority. */
#define PRI_DEFAULT 31                  /* Default priority. */
#define PRI_MAX 63                      /* Highest priority. */

/* TASK 1 : Nice Range */
#define NICE_DEFAULT 0                  /* Default niceness */
#define NICE_MIN -20                    /* Minumum niceness */
#define NICE_MAX 20                     /* Maximum niceness */

#define CPU_NUM_DEFAULT 0               /* Default cpu_num */
#define LOAD_AVG_DEFAULT 0              /* Default load_avg */
#define DEPTH_LIMIT 8                   /* Limit for nested donation */

/* A kernel thread or user process.

   Each thread structure is stored in its own 4 kB page.  The
   thread structure itself sits at the very bottom of the page
   (at offset 0).  The rest of the page is reserved for the
   thread's kernel stack, which grows downward from the top of
   the page (at offset 4 kB).  Here's an illustration:

        4 kB +---------------------------------+
             |          kernel stack           |
             |                |                |
             |                |                |
             |                V                |
             |         grows downward          |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             +---------------------------------+
             |              magic              |
             |                :                |
             |                :                |
             |               name              |
             |              status             |
        0 kB +---------------------------------+

   The upshot of this is twofold:

      1. First, `struct thread' must not be allowed to grow too
         big.  If it does, then there will not be enough room for
         the kernel stack.  Our base `struct thread' is only a
         few bytes in size.  It probably should stay well under 1
         kB.

      2. Second, kernel stacks must not be allowed to grow too
         large.  If a stack overflows, it will corrupt the thread
         state.  Thus, kernel functions should not allocate large
         structures or arrays as non-static local variables.  Use
         dynamic allocation with malloc() or palloc_get_page()
         instead.

   The first symptom of either of these problems will probably be
   an assertion failure in thread_current(), which checks that
   the `magic' member of the running thread's `struct thread' is
   set to THREAD_MAGIC.  Stack overflow will normally change this
   value, triggering the assertion. */
/* The `elem' member has a dual purpose.  It can be an element in
   the run queue (thread.c), or it can be an element in a
   semaphore wait list (synch.c).  It can be used these two ways
   only because they are mutually exclusive: only a thread in the
   ready state is on the run queue, whereas only a thread in the
   blocked state is on a semaphore wait list. */

struct thread
  {
    /* Owned by thread.c. */
    tid_t tid;                          /* Thread identifier. */
    enum thread_status status;          /* Thread state. */
    char name[16];                      /* Name (for debugging purposes). */
    uint8_t *stack;                     /* Saved stack pointer. */
    int priority;                       /* Priority. */
    int eff_priority;                   /* TASK 1 : Effective priority */
    struct list_elem allelem;           /* List element for all threads list. */

    /* Shared between thread.c and synch.c. */
    struct list_elem elem;              /* List element. */

#ifdef USERPROG
    /* Owned by userprog/process.c. */
    uint32_t *pagedir;                 /* Page directory. */

    /* TASK 2 */
    pid_t pid;                        /* Unique process identification */
    struct file *file;                /* Pointer to execuable file where
                                         process run */
    bool child_load_success;          /* True if the last process executed
                                         by this thread load successfully */
    struct semaphore load_sema;       /* Semaphore down when this thread is
                                         loading. */
    struct semaphore alive_sema;      /* Semaphore down when this thread is
                                         alive. */
    struct thread *parent;            /* Thread of parent process ('null' if no
                                         parent process exists.) */
    struct list child_procs;          /* List of threads representing child
                                         processes that have been spawned by
                                         this thread's embedding process. */
    struct list file_descriptors;     /* List of file descriptors that the
                                         process has currently opened. */
    struct list pid_to_exit_status;   /* Mappings list from process identification
                                         to the corresponding process' exit
                                         status. */
    struct list_elem child_elem;      /* List element for 'child_procs' list as
                                         a processes (single-threaded) can be
                                         both child and parent processes. */
    int exit_status;                  /* exit status of thread */
	  struct list file_list;            /* File list used for file system calls*/
	  int fd;
    int next_fd;                      /* Next file descriptor to use. */
#endif

    /* TASK 0 */
    int64_t wake_up_tick;               /* Keep track the tick when sleeping
                                          thread wakes up */

    /* TASK 1 : Priority Checking */
    int base_priority;                /* initial priority of thread */
    struct list_elem donation_thread; /* thread element that is donated */
    struct lock* lock_waiting;        /* lock that thread is waiting for */
    struct list threads_donated;      /* list of threads */

    /* TASK 1: Advanced scheduling */
    int cpu_num;                        /* Time spent in the CPU recently */
    int nice;                           /* Index of greediness for CPU */

	/* Owned by thread.c. */
    unsigned magic;                     /* Detects stack overflow. */
  };

  /* TASK 2: Struct for pid exit status. */
struct pid_exit_status
  {
    pid_t pid;
    int exit_status;
    struct list_elem elem;
  };

struct file_handle
  {
    int fd;                   /* File descriptor of file. */
    struct file *file;        /* Pointer to file struct. */
    struct list_elem elem;    /* List element. */
  };


/* If false (default), use round-robin scheduler.
   If true, use multi-level feedback queue scheduler.
   Controlled by kernel command-line option "-o mlfqs". */
extern bool thread_mlfqs;

void thread_init (void);
void thread_start (void);

void thread_tick (void);
void thread_print_stats (void);

typedef void thread_func (void *aux);
tid_t thread_create (const char *name, int priority, thread_func *, void *);

void thread_block (void);
void thread_unblock (struct thread *);

struct thread *thread_current (void);
tid_t thread_tid (void);
const char *thread_name (void);

void thread_exit (void) NO_RETURN;
void thread_yield (void);

/* Performs some operation on thread t, given auxiliary data AUX. */
typedef void thread_action_func (struct thread *t, void *aux);
void thread_foreach (thread_action_func *, void *);

int thread_get_priority (void);
void thread_set_priority (int);

int thread_get_nice (void);
void thread_set_nice (int);
int thread_get_recent_cpu (void);
int thread_get_load_avg (void);

struct thread *thread_for_sema_list_elem (const struct list_elem *);

/* TASK 1 : Priority Checking */
bool is_lower_priority (const struct list_elem *a, const struct list_elem *b, void *aux UNUSED);
void update_priority(void);
void donate_priority(void);
void check_max_priority(void);
void remove_with_lock(struct lock* l);


/* TASK 1: Advanced scheduling */

/* mlfqs functions */
void recalculate_mlfqs(void);
void priority_thread_mlfqs(struct thread* t, void *aux UNUSED);
void cpu_thread_mlfqs (struct thread *t, void *aux UNUSED);
void load_avg_thread_mlfqs (void);

/* TASK 2 */
struct thread *get_tid_thread(tid_t tid);
int thread_add_new_file (struct file *file);
struct file_handle* thread_get_file_handle (struct list *file_list, int fd);

#endif /* threads/thread.h */
