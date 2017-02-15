#include "threads/thread.h"
#include <debug.h>
#include <stddef.h>
#include <random.h>
#include <stdio.h>
#include <string.h>
#include "threads/flags.h"
#include "threads/interrupt.h"
#include "threads/intr-stubs.h"
#include "threads/palloc.h"
#include "threads/switch.h"
#include "threads/synch.h"
#include "threads/vaddr.h"
#include "threads/fixedpointrealarith.h"
#include "devices/timer.h"
#ifdef USERPROG
#include "userprog/process.h"
#endif

/* Random value for struct thread's `magic' member.
   Used to detect stack overflow.  See the big comment at the top
   of thread.h for details. */
#define THREAD_MAGIC 0xcd6abf4b

/* List of processes in THREAD_READY state, that is, processes
   that are ready to run but not actually running.
   Implemented as a priority-based sorted list. */
static struct list ready_list;

/* List of all processes.  Processes are added to this list
   when they are first scheduled and removed when they exit. */
static struct list all_list;

/* Idle thread. */
static struct thread *idle_thread;

/* Initial thread, the thread running init.c:main(). */
static struct thread *initial_thread;

/* Lock used by allocate_tid(). */
static struct lock tid_lock;

/* Stack frame for kernel_thread(). */
struct kernel_thread_frame
  {
    void *eip;                  /* Return address. */
    thread_func *function;      /* Function to call. */
    void *aux;                  /* Auxiliary data for function. */
  };

/* Statistics. */
static long long idle_ticks;    /* # of timer ticks spent idle. */
static long long kernel_ticks;  /* # of timer ticks in kernel threads. */
static long long user_ticks;    /* # of timer ticks in user programs. */

/* Scheduling. */
#define TIME_SLICE 4            /* # of timer ticks to give each thread. */
static unsigned thread_ticks;   /* # of timer ticks since last yield. */

/* If false (default), use round-robin scheduler.
   If true, use multi-level feedback queue scheduler.
   Controlled by kernel command-line option "-o mlfqs". */
bool thread_mlfqs;

int load_avg;

static void kernel_thread (thread_func *, void *aux);

static void idle (void *aux UNUSED);
static struct thread *running_thread (void);
static struct thread *next_thread_to_run (void);
static void init_thread (struct thread *, const char *name, int priority);
static bool is_thread (struct thread *) UNUSED;
static void *alloc_frame (struct thread *, size_t size);
static void schedule (void);
void thread_schedule_tail (struct thread *prev);
static tid_t allocate_tid (void);

/* Initializes the threading system by transforming the code
   that's currently running into a thread.  This can't work in
   general and it is possible in this case only because loader.S
   was careful to put the bottom of the stack at a page boundary.

   Also initializes the run queue and the tid lock.

   After calling this function, be sure to initialize the page
   allocator before trying to create any threads with
   thread_create().

   It is not safe to call thread_current() until this function
   finishes. */
void
thread_init (void)
{
  ASSERT (intr_get_level () == INTR_OFF);

  lock_init (&tid_lock);
  list_init (&ready_list);
  list_init (&all_list);

  /* Set up a thread structure for the running thread. */
  initial_thread = running_thread ();
  init_thread (initial_thread, "main", PRI_DEFAULT);
  initial_thread->status = THREAD_RUNNING;
  initial_thread->tid = allocate_tid ();
}

/* Starts preemptive thread scheduling by enabling interrupts.
   Also creates the idle thread. */
void
thread_start (void)
{
  /* Create the idle thread. */
  struct semaphore idle_started;
  sema_init (&idle_started, 0);
  thread_create ("idle", PRI_MIN, idle, &idle_started);

  /* TASK 1: load_avg is the average number of thread competing for the CPU */
  load_avg = LOAD_AVG_DEFAULT;

  /* Start preemptive thread scheduling. */
  intr_enable ();

  /* Wait for the idle thread to initialize idle_thread. */
  sema_down (&idle_started);
}

/* Called by the timer interrupt handler at each timer tick.
   Thus, this function runs in an external interrupt context. */
void
thread_tick (void)
{
  struct thread *t = thread_current ();

  /* Update statistics. */
  if (t == idle_thread)
    idle_ticks++;
#ifdef USERPROG
  else if (t->pagedir != NULL)
    user_ticks++;
#endif
  else
    kernel_ticks++;

  /* TASK 1: Handling advanced scheduler mode. */
  if (thread_mlfqs) {
    recalculate_mlfqs ();
  }
  thread_ticks++;

  /* Enforce preemption. */
  if (++thread_ticks >= TIME_SLICE)
    intr_yield_on_return ();
}

/* Prints thread statistics. */
void
thread_print_stats (void)
{
  printf ("Thread: %lld idle ticks, %lld kernel ticks, %lld user ticks\n",
          idle_ticks, kernel_ticks, user_ticks);
}

/* Creates a new kernel thread named NAME with the given initial
   PRIORITY, which executes FUNCTION passing AUX as the argument,
   and adds it to the ready queue.  Returns the thread identifier
   for the new thread, or TID_ERROR if creation fails.

   If thread_start() has been called, then the new thread may be
   scheduled before thread_create() returns.  It could even exit
   before thread_create() returns.  Contrariwise, the original
   thread may run for any amount of time before the new thread is
   scheduled.  Use a semaphore or some other form of
   synchronization if you need to ensure ordering.

   The code provided sets the new thread's `priority' member to
   PRIORITY, but no actual priority scheduling is implemented.
   Priority scheduling is the goal of Problem 1-3. */
tid_t
thread_create (const char *name, int priority,
               thread_func *function, void *aux)
{
  struct thread *t;
  struct kernel_thread_frame *kf;
  struct switch_entry_frame *ef;
  struct switch_threads_frame *sf;
  tid_t tid;
  enum intr_level old_level;

  ASSERT (function != NULL);

  /* Allocate thread. */
  t = palloc_get_page (PAL_ZERO);
  if (t == NULL)
    return TID_ERROR;

  /* Initialize thread. */
  init_thread (t, name, priority);
  tid = t->tid = allocate_tid ();

  /* Prepare thread for first run by initializing its stack.
     Do this atomically so intermediate values for the 'stack'
     member cannot be observed. */
  old_level = intr_disable ();

  /* Stack frame for kernel_thread(). */
  kf = alloc_frame (t, sizeof *kf);
  kf->eip = NULL;
  kf->function = function;
  kf->aux = aux;

  /* Stack frame for switch_entry(). */
  ef = alloc_frame (t, sizeof *ef);
  ef->eip = (void (*) (void)) kernel_thread;

  /* Stack frame for switch_threads(). */
  sf = alloc_frame (t, sizeof *sf);
  sf->eip = switch_entry;
  sf->ebp = 0;

  intr_set_level (old_level);

  /* TASK 1 */
  /* Add to run queue. */
  thread_unblock (t);

  /* Tests the maximum priority: if the new thread has higher priority than
     the currently running thread then let the former run first. */
  old_level = intr_disable ();
  check_max_priority();
  intr_set_level (old_level);
  
  /* TASK 2 */
  #ifdef USERPROG
  t->proc = (process*)malloc(sizeof(process));
  t->proc->parent = thread_current ();
  t->proc->wait = false;
  t->proc->exit = false; 
  list_init(&t->proc.child_procs);
  
   if (thread_current () != initial_thread) {
    list_push_back (&thread_current ()->proc.child_procs, &t->proc.elem);
  }
  
  #endif

  return tid;
}

/* Puts the current thread to sleep.  It will not be scheduled
   again until awoken by thread_unblock().

   This function must be called with interrupts turned off.  It
   is usually a better idea to use one of the synchronization
   primitives in synch.h. */
void
thread_block (void)
{
  ASSERT (!intr_context ());
  ASSERT (intr_get_level () == INTR_OFF);

  thread_current ()->status = THREAD_BLOCKED;
  schedule ();
}

/* Transitions a blocked thread T to the ready-to-run state.
   This is an error if T is not blocked.  (Use thread_yield() to
   make the running thread ready.)

   This function does not preempt the running thread.  This can
   be important: if the caller had disabled interrupts itself,
   it may expect that it can atomically unblock a thread and
   update other data. */
void
thread_unblock (struct thread *t)
{
  enum intr_level old_level;

  ASSERT (is_thread (t));

  old_level = intr_disable ();
  ASSERT (t->status == THREAD_BLOCKED);
  list_insert_ordered (&ready_list, &t->elem, is_lower_priority, NULL);
  t->status = THREAD_READY;
  intr_set_level (old_level);
}

/* Returns the name of the running thread. */
const char *
thread_name (void)
{
  return thread_current ()->name;
}

/* Returns the running thread.
   This is running_thread() plus a couple of sanity checks.
   See the big comment at the top of thread.h for details. */
struct thread *
thread_current (void)
{
  struct thread *t = running_thread ();

  /* Make sure T is really a thread.
     If either of these assertions fire, then your thread may
     have overflowed its stack.  Each thread has less than 4 kB
     of stack, so a few big automatic arrays or moderate
     recursion can cause stack overflow. */
  ASSERT (is_thread (t));
  ASSERT (t->status == THREAD_RUNNING);

  return t;
}

/* Returns the running thread's tid. */
tid_t
thread_tid (void)
{
  return thread_current ()->tid;
}

/* TASK 2: Returns thread with the tid number given */
struct thread* 
get_tid_thread(tid_t tid) {
struct list_elem *e;
  for (elem = list_begin (&all_list); elem != list_end (&all_list); 
    elem= list_next (elem)) {
    struct thread *t = list_entry (elem, struct thread, allelem);
    if (t->tid == tid)
      return t;
    }
  return NULL;
}    

/* Deschedules the current thread and destroys it.  Never
   returns to the caller. */
void
thread_exit (void)
{
  ASSERT (!intr_context ());
  
  #ifdef USERPROG
  
    struct list_elem *e;

    for (e = list_begin (&thread_current ()->proc.child_procs); e != list_end (&thread_current ()->proc.child_procs); 
      e = list_next (e)) {
      struct thread *t = list_entry (e, struct thread, &thread_current->proc.elem);
      if (!t->proc.exit) {
          t->proc->parent = NULL;
          list_remove (&t->proc.elem);
      }
	}
    process_exit ();
  
    if (thread_current ()->proc->parent != NULL && thread_current ()->proc->parent != initial_thread) {
      list_remove (&thread_current ()->proc.child);
	}
#endif

  /* Remove thread from all threads list, set our status to dying,
     and schedule another process.  That process will destroy us
     when it calls thread_schedule_tail(). */
  intr_disable ();
  list_remove (&thread_current()->allelem);
  thread_current ()->status = THREAD_DYING;
  schedule ();
  NOT_REACHED ();
}

/* Yields the CPU.  The current thread is not put to sleep and
   may be scheduled again immediately at the scheduler's whim. */
void
thread_yield (void)
{
  struct thread *cur = thread_current ();
  enum intr_level old_level;

  ASSERT (!intr_context ());

  old_level = intr_disable ();
  if (cur != idle_thread)
    list_insert_ordered (&ready_list, &cur->elem, is_lower_priority, NULL);
  cur->status = THREAD_READY;
  schedule ();
  intr_set_level (old_level);
}

/* Invoke function 'func' on all threads, passing along 'aux'.
   This function must be called with interrupts off. */
void
thread_foreach (thread_action_func *func, void *aux)
{
  struct list_elem *e;

  ASSERT (intr_get_level () == INTR_OFF);

  for (e = list_begin (&all_list); e != list_end (&all_list);
       e = list_next (e))
    {
      struct thread *t = list_entry (e, struct thread, allelem);
      func (t, aux);
    }
}

/* TASK 1: Sets the current thread's priority to NEW_PRIORITY.
   This function assumes ready_list is a priority-wise sorted list. */
void
thread_set_priority (int new_priority)
{
  /* Advanced scheduler mode makes no use of this function */
  if(thread_mlfqs) return;

  enum intr_level level = intr_disable();
  int priority_prev = thread_current()->priority;
  thread_current()->base_priority = new_priority;
  update_priority();

  if(priority_prev < thread_current()->priority) {
    donate_priority();
  }
  if (priority_prev > thread_current()->priority) {
    check_max_priority();
  }

  intr_set_level(level);
}

/* TASK 1: Returns the current thread's effective priority
           (the highest between base and any donations). */
int
thread_get_priority (void)
{
  return thread_current ()->priority;
}

/* TASK 1:  Sets the current thread's nice value to NICE. */
void
thread_set_nice (int nice)
{
  ASSERT (thread_mlfqs);
  ASSERT (nice >= NICE_MIN);
  ASSERT (nice <= NICE_MAX);

  thread_current()->nice = nice;

  priority_thread_mlfqs(thread_current (), NULL);

  if (!list_empty(&ready_list)) {
    struct thread *next = list_entry (list_max (&ready_list,
                                       &is_lower_priority , NULL),
                                       struct thread, elem);

    if(thread_current ()->priority < next->priority) {
      thread_yield ();
    }
  }
}

/* TASK 1 : Returns the current thread's nice value. */
int
thread_get_nice (void)
{
  return thread_current()->nice;
}

/* TASK 1: Returns 100 times the current system load average,
           rounded to the nearest integer. */
int
thread_get_load_avg (void)
{
  int mul = mul_x_n(load_avg, 100);
  int nearest_int = convert_to_int_nearest(mul);
  return nearest_int;
}

/* TASK 1: Returns 100 times the current thread’s recent cpu value,
           rounded to the nearest integer. */
int
thread_get_recent_cpu (void)
{
  int cpu = thread_current()->cpu_num;
  int mul = mul_x_n(cpu, 100);
  int nearest_int = convert_to_int_nearest(mul);
  return nearest_int;
}

/* Idle thread.  Executes when no other thread is ready to run.

   The idle thread is initially put on the ready list by
   thread_start().  It will be scheduled once initially, at which
   point it initializes idle_thread, "up"s the semaphore passed
   to it to enable thread_start() to continue, and immediately
   blocks.  After that, the idle thread never appears in the
   ready list.  It is returned by next_thread_to_run() as a
   special case when the ready list is empty. */
static void
idle (void *idle_started_ UNUSED)
{
  struct semaphore *idle_started = idle_started_;
  idle_thread = thread_current ();
  sema_up (idle_started);

  for (;;)
    {
      /* Let someone else run. */
      intr_disable ();
      thread_block ();

      /* Re-enable interrupts and wait for the next one.

         The `sti' instruction disables interrupts until the
         completion of the next instruction, so these two
         instructions are executed atomically.  This atomicity is
         important; otherwise, an interrupt could be handled
         between re-enabling interrupts and waiting for the next
         one to occur, wasting as much as one clock tick worth of
         time.

         See [IA32-v2a] "HLT", [IA32-v2b] "STI", and [IA32-v3a]
         7.11.1 "HLT Instruction". */
      asm volatile ("sti; hlt" : : : "memory");
    }
}

/* Function used as the basis for a kernel thread. */
static void
kernel_thread (thread_func *function, void *aux)
{
  ASSERT (function != NULL);

  intr_enable ();       /* The scheduler runs with interrupts off. */
  function (aux);       /* Execute the thread function. */
  thread_exit ();       /* If function() returns, kill the thread. */
}

/* Returns the running thread. */
struct thread *
running_thread (void)
{
  uint32_t *esp;

  /* Copy the CPU's stack pointer into `esp', and then round that
     down to the start of a page.  Because `struct thread' is
     always at the beginning of a page and the stack pointer is
     somewhere in the middle, this locates the curent thread. */
  asm ("mov %%esp, %0" : "=g" (esp));
  return pg_round_down (esp);
}

/* Returns true if T appears to point to a valid thread. */
static bool
is_thread (struct thread *t)
{
  return t != NULL && t->magic == THREAD_MAGIC;
}

/* Does basic initialization of T as a blocked thread named
   NAME. */
static void
init_thread (struct thread *t, const char *name, int priority)
{
  enum intr_level old_level;

  ASSERT (t != NULL);
  ASSERT (PRI_MIN <= priority && priority <= PRI_MAX);
  ASSERT (name != NULL);

  memset (t, 0, sizeof *t);
  t->status = THREAD_BLOCKED;
  strlcpy (t->name, name, sizeof t->name);
  t->stack = (uint8_t *) t + PGSIZE;
  t->priority = priority;
  t->magic = THREAD_MAGIC;

  /* TASK 1: Handling the advanced scheduler priority system */
  if (thread_mlfqs) {
      priority_thread_mlfqs (t, NULL);
  } else {
      t->base_priority = priority;
      t->lock_waiting = NULL;
      list_init(&t->threads_donated);
    }

  old_level = intr_disable ();
  list_push_back (&all_list, &t->allelem);
  intr_set_level (old_level);

  t->nice = NICE_DEFAULT;
  t->cpu_num = CPU_NUM_DEFAULT;
}

/* Allocates a SIZE-byte frame at the top of thread T's stack and
   returns a pointer to the frame's base. */
static void *
alloc_frame (struct thread *t, size_t size)
{
  /* Stack data is always allocated in word-size units. */
  ASSERT (is_thread (t));
  ASSERT (size % sizeof (uint32_t) == 0);

  t->stack -= size;
  return t->stack;
}

/* Chooses and returns the next thread to be scheduled.  Should
   return a thread from the run queue, unless the run queue is
   empty.  (If the running thread can continue running, then it
   will be in the run queue.)  If the run queue is empty, return
   idle_thread. */
static struct thread *
next_thread_to_run (void)
{
  if (list_empty (&ready_list))
    return idle_thread;
  else
    return list_entry (list_pop_front (&ready_list), struct thread, elem);
}

/* Completes a thread switch by activating the new thread's page
   tables, and, if the previous thread is dying, destroying it.

   At this function's invocation, we just switched from thread
   PREV, the new thread is already running, and interrupts are
   still disabled.  This function is normally invoked by
   thread_schedule() as its final action before returning, but
   the first time a thread is scheduled it is called by
   switch_entry() (see switch.S).

   It's not safe to call printf() until the thread switch is
   complete.  In practice that means that printf()s should be
   added at the end of the function.

   After this function and its caller returns, the thread switch
   is complete. */
void
thread_schedule_tail (struct thread *prev)
{
  struct thread *cur = running_thread ();

  ASSERT (intr_get_level () == INTR_OFF);

  /* Mark us as running. */
  cur->status = THREAD_RUNNING;

  /* Start new time slice. */
  thread_ticks = 0;

#ifdef USERPROG
  /* Activate the new address space. */
  process_activate ();
#endif

  /* If the thread we switched from is dying, destroy its struct
     thread.  This must happen late so that thread_exit() doesn't
     pull out the rug under itself.  (We don't free
     initial_thread because its memory was not obtained via
     palloc().) */
  if (prev != NULL && prev->status == THREAD_DYING && prev != initial_thread)
    {
      ASSERT (prev != cur);
      palloc_free_page (prev);
    }
}

/* Schedules a new process.  At entry, interrupts must be off and
   the running process's state must have been changed from
   running to some other state.  This function finds another
   thread to run and switches to it.

   It's not safe to call printf() until thread_schedule_tail()
   has completed. */
static void
schedule (void)
{
  struct thread *cur = running_thread ();
  struct thread *next = next_thread_to_run ();
  struct thread *prev = NULL;

  ASSERT (intr_get_level () == INTR_OFF);
  ASSERT (cur->status != THREAD_RUNNING);
  ASSERT (is_thread (next));

  if (cur != next)
    prev = switch_threads (cur, next);
  thread_schedule_tail (prev);
}

/* Returns a tid to use for a new thread. */
static tid_t
allocate_tid (void)
{
  static tid_t next_tid = 1;
  tid_t tid;

  lock_acquire (&tid_lock);
  tid = next_tid++;
  lock_release (&tid_lock);

  return tid;
}

/* Returns the thread associated with a sema_elem */
struct thread *thread_for_sema_list_elem (const struct list_elem *lelem) {
  struct semaphore sema = list_entry (lelem, struct semaphore_elem, elem)->semaphore;
  return list_entry (list_front (&sema.waiters), struct thread, elem);
}

/* Offset of `stack' member within `struct thread'.
   Used by switch.S, which can't figure it out on its own. */
uint32_t thread_stack_ofs = offsetof (struct thread, stack);



/* TASK 0: Function used as criterium to sort ready_list priority-wise */
bool is_lower_priority (const struct list_elem *a,
                          const struct list_elem *b, void *aux UNUSED) {
  ASSERT(a != NULL);
  ASSERT(b != NULL);

  struct thread *threadA = list_entry(a, struct thread, elem);
  struct thread *threadB = list_entry(b, struct thread, elem);

  return threadA->priority > threadB->priority;
}


/* TASK 1: Checks whether the currently running thread is still the thread
           with the highest priority, if not it changes running thread. */
void check_max_priority(void) {

  if(list_empty(&ready_list)) {
    return;
  }

  struct thread* t = list_entry(list_front(&ready_list), struct thread, elem);

  if(intr_context()) {
    thread_ticks++;
    if(thread_current ()->priority < t->priority || (thread_ticks >= TIME_SLICE
                        && thread_current()-> priority == t->priority)) {
      intr_yield_on_return();
    }
    return;
  }

    if(thread_current()->priority < t->priority) {
      thread_yield();
    }
}


/* TASK 1: Donates priority to another thread holding a lock */
void donate_priority(void) {
  int depth = 0;
  struct thread *t = thread_current();
  struct lock *l = t->lock_waiting;
  while(l != NULL && depth < DEPTH_LIMIT) {
    depth++;

    if(l->holder == NULL) return;
    if(l->holder->priority >= t->priority) return;

    l->holder->priority = t->priority;
    t = l->holder;
    l = t->lock_waiting;
  }
}


/* TASK 1: Updates priority member in thread struct with the effective
           priority (i.e. taking into account donations) */
void update_priority(void) {
  struct thread *t = thread_current();
  t->priority = t->base_priority;
  if(list_empty(&t->threads_donated)) {
    return;
  }
  struct thread *t2 = list_entry(list_front(&t->threads_donated),
                               struct thread, donation_thread);

  if(t2->priority > t->priority) {
     t->priority = t2->priority;
   }
  }


/* TASK 1: Removes from list threads_donated threads waiting for lock l */
void remove_with_lock(struct lock* l) {
  struct list_elem* e = list_begin(&thread_current()->threads_donated);
  struct list_elem* next;
  while(e != list_end(&thread_current()->threads_donated)) {
    struct thread* t = list_entry(e, struct thread, donation_thread);
    next = list_next(e);
    if(t->lock_waiting == l) {
      list_remove(e);
    }
    e = next;
  }
}


/* TASK 1: Function for advanced scheduling.
           Recalculates CPU time for the current thread (once per second). */
void recalculate_mlfqs(void)
{
  ASSERT (thread_mlfqs);

  struct thread *t = thread_current ();

  if (timer_ticks () % TIMER_FREQ == 0) {
    load_avg_thread_mlfqs ();
    thread_foreach (&cpu_thread_mlfqs, NULL);
  }
  if (thread_current () != idle_thread) {
    t->cpu_num = add_x_n(t->cpu_num, 1);
  }

  if (timer_ticks () % TIME_SLICE == 0) {
    thread_foreach (&priority_thread_mlfqs, NULL);
  }
}

/* TASK 1: Function for advanced scheduling.
           Sets thread's priority using the advanced scheduler system. */
void priority_thread_mlfqs(struct thread* t, void *aux UNUSED)
{
  ASSERT (thread_mlfqs);

  int32_t fp_priority = convert_to_fixed_point(PRI_MAX);
  int32_t cpu_recent = div_x_n(t->cpu_num, 4);
  fp_priority = sub_x_y(fp_priority, cpu_recent);
  fp_priority = sub_x_y(fp_priority, convert_to_fixed_point(t->nice * 2));
  int priority_num = convert_to_int_zero(fp_priority);

  /* Bound priority_num in valid range 0-64 */
  if (priority_num > PRI_MAX)
    priority_num = PRI_MAX;

  if (priority_num < PRI_MIN)
    priority_num = PRI_MIN;

  t->priority = priority_num;
}


/* TASK 1: Calculates recent_cpu (i.e. the time spent in the cpu recently). */
void cpu_thread_mlfqs (struct thread *t, void *aux UNUSED)
{
  ASSERT (thread_mlfqs);
  /* For cpu to be calculated, we need the following condition: */
  ASSERT(timer_ticks () % TIME_SLICE == 0);

  int cpu_recent = t->cpu_num;

  /*  (2*load_avg)  */
  int load_avg_curr = mul_x_n(load_avg, 2);

  /*  (2*load_avg)/(2*load_avg + 1)  */
  int coeff = div_x_y(load_avg_curr, add_x_n(load_avg_curr, 1));

  /*  (2*load_avg)/(2*load_avg + 1) * recent_cpu  */
  cpu_recent = mul_x_y(coeff, cpu_recent);

  /*  (2*load_avg)/(2*load_avg + 1) * recent_cpu + nice  */
  cpu_recent = add_x_n(cpu_recent, t->nice);

  t->cpu_num = cpu_recent;
}

/* TASK 1: Function to calculate load_avg (i.e. the average number
           of threads competing for the CPU) */
void load_avg_thread_mlfqs (void)
{

  ASSERT (thread_mlfqs);

  int fp_59 = convert_to_fixed_point(59);
  int fp_60 = convert_to_fixed_point(60);

  int load_avg_curr = load_avg;

  /*  59 / 60  */
  int fp_59_div_60 = div_x_y(fp_59, fp_60);

  /*  (59/60)*load_avg  */
  int load_avg_mul = mul_x_y(load_avg_curr, fp_59_div_60);

  /* Size of ready threads */
  int ready_threads = list_size (&ready_list);
  if (thread_current() != idle_thread) {
    ready_threads++;
  }

  /*  (1/60) * ready_threads  */

  int fp_READY_THREADS = convert_to_fixed_point(ready_threads);
  int ready_threads_60 = div_x_y(fp_READY_THREADS, fp_60);

  /*  (59/60)load_avg + (1/60)* ready_threads  */
  load_avg = add_x_y(load_avg_mul, ready_threads_60);

}
