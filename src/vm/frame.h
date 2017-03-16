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
  struct hash_elem elem;
};

/* Funtions for frame table. */

/* Basic life cycle. */
struct hash* ft_create (void);
bool         ft_init (void);
void         ft_clear (void);
void         ft_destroy (void);

/* Frame table retrieval. */
struct hash* ft_get_frame_table (void);

/* Functions for page frames. */

/* Create */
struct frame* ft_create_frame (void *kpage);

/* Search, insertion, deletion. */
struct frame* ft_get_frame (void *kpage);
struct frame* ft_insert_frame (struct frame *f);
struct frame* ft_replace_frame (struct frame *f_new);
struct frame* ft_delete_frame (struct frame *f);

/* Creation and destruction of frame table. */
struct list* ft_create (void);
void ft_destroy (void);

/* Loading and retrieving */
struct frame* ft_load (struct page_table_entry *pte, enum palloc_flags flags);
struct frame* ft_get (void *kpage);

/* Eviction */
void ft_evict (void);

#endif /* vm/frame.h */
