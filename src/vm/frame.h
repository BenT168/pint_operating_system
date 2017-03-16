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

  struct list pids_entries;   /* List of page table entries mapping to this
                                 frame and corresponding process ids. Each pid
                                 is associated with a unique entry. */

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

struct pid_to_entry
{
  pid_t pid;
  struct page_table_entry *pte;
  struct list_elem elem;
}

/* Funtions for frame table. */

/* Hash function */
unsigned ft_hash_func (const struct hash_elem *e, void *aux);
bool ft_is_lower_hash_elem (const struct hash_elem *a,
                            const struct hash_elem *b,
                            void *aux);

/* Basic life cycle. */
struct hash* ft_create (void);
void         ft_clear (void);
void         ft_destroy (void);

/* Frame table retrieval. */
struct hash* ft_get_frame_table (void);

/* Functions for page frames. */

/* Create */
struct frame* ft_create_frame (struct page_table_entry *pte,
                               enum palloc_flags flags);

/* Search, insertion, deletion. */
struct frame* ft_get_frame (struct page_table_entry *pte);
struct frame* ft_load_frame (struct frame *f);
struct frame* ft_replace_frame (struct frame *f_new);
void          ft_delete_frame (struct frame *f);

/* Create copy preserving only data. */
struct frame* ft_copy_frame (struct frame *f);

/* Eviction */
struct frame* ft_evict_frame (void);
bool          ft_write_to_file (struct page_table_entry *pte);

/* Information */
size_t ft_size (struct hash *ft);
bool ft_is_empty (struct hash *ft);

#endif /* vm/frame.h */
