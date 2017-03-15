#ifndef _VM_FRAME_H
#define _VM_FRAME_H


#include "threads/palloc.h"
#include <hash.h>
#include <list.h>
#include <stdbool.h>

/* Page frame */
struct frame
{
  unsigned frame_no:20;   /* Unique identifier of page frame. This should be
                             equal to the 20 high order bits of the kernel
                             address at which the page frame begins. */
  struct list pids;       /* Ids of processes which have a virtual page mapping
                             to this page. */
  unsigned shared:1;      /* 1 if the page is shared and 0 if it is not. */
  struct lock lock;       /* Lock used to implement mutually exclusive access to
                             a page frame. This is particularly necessary for
                             shared pages: for example, if Process A attempts
                             to write to a shared frame - frame 1 - at the same
                             time that Process B attempts to read from it, we
                             must perform a Copy-On-Write, ensuring that
                             Process A writes to the copied page and Process B
                             reads from the original page.

                             Another example is when Process A attempts to
                             evict a page at the same time that Process B
                             "requests" it as a result of a page fault.

                             Mutual exclusion is necessary to handle such
                             situations. */
  struct list_elem elem;
};

/* Creation and destruction of frame table. */
struct list* ft_create (void);
void ft_destroy (void);

/* Loading and retrieving */
struct frame* ft_load (struct page_table_entry *pte, enum palloc_flags flags);
struct frame* ft_get (void *kpage);

/* Eviction */
void ft_evict (void);

#endif /* vm/frame.h */
