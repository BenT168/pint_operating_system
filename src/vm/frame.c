#include "vm/frame.h"
#include <hash.h>
#include <list.h>
#include "threads/malloc.h"
#include "threads/synch.h"
#include "threads/thread.h"
#include "threads/palloc.h"
#include "userprog/pagedir.h"
#include "userprog/process.h"
#include "userprog/syscall.h"
#include "vm/page.h"
#include "vm/swap.h"

static struct lock frame_lock;
static struct list eviction_list;

/* Task 3 : Frames helper functions to initialise */
static void acquire_framelock (void);
static void release_framelock (void);

/* TASK 3 : Acquires lock over frame table */
void
acquire_framelock (void)
{
  lock_acquire (&frame_lock);
}

/* TASK 3 : Releases lock from frame table */
void
release_framelock (void)
{
  lock_release (&frame_lock);
}

/* TASK 3 : Initializes the frame hash table and lock */
void
frame_init (void)
{
  list_init(&eviction_list);
  lock_init(&frame_lock);
}

/* TASK 3 : Evicts a frame and replaces it with a new one */
void
frame_evict (enum palloc_flags flags)
{
  //acquire_framelock();

  if (list_empty (&eviction_list)) {
    return;
  }

  struct list_elem *e = list_begin (&eviction_list);
  struct frame *victim = list_entry (e, struct frame, list_elem);

  while (pagedir_is_accessed (victim->thread->pagedir, victim->upage)) {
    pagedir_set_accessed (victim->thread->pagedir, victim->upage, false);
    e = list_next (e);
    if (e == list_end (&eviction_list)) {
      e = list_begin (&eviction_list);
    }
    victim = list_entry (e, struct frame, list_elem);
  }


  //release_framelock();

  // Accessed and Dirty (modified) Bit
  struct page_table_entry* pte = get_page_table_entry(&victim->thread->sup_page_table
  , victim->upage);
  struct list_elem *elem = list_begin (&eviction_list);
  victim = list_entry (elem, struct frame, list_elem);

  while(true) {

    if(pagedir_is_dirty(victim->thread->pagedir, victim->upage)) {

      if(pte->bit_set == SWAP_BIT) {
        struct swap_slot* ss = (struct swap_slot*)malloc(sizeof(struct swap_slot));
        ss->swap_frame = victim;
        ss->swap_addr = (block_sector_t) victim->upage;
        swap_store(ss);
      } else if(pte->bit_set == MMAP_BIT) {
        munmap(pte->mapid);
        //goto mmap;
      }
      pte->loaded = false;
      list_remove(elem);
      palloc_free_page(victim->upage);
      frame_free(victim);
      return;
    }

    e = list_next (e);
    if (e == list_end (&eviction_list)) {
      e = list_begin (&eviction_list);
    }
    victim = list_entry (e, struct frame, list_elem);
  }
}

/* TASK 3 : Allocates a new page, and adds it to the frame table */
void*
frame_alloc (void * upage, enum palloc_flags flags)
{
  //void *kpage = palloc_get_page (PAL_USER | zero ? PAL_ZERO : 0 );

  void* kpage = palloc_get_page(flags);

  //acquire_framelock();

  /* evict a frame if not enough memory */
  if (kpage == NULL) frame_evict(flags);

  if(frame_get(kpage) != NULL) {
      return kpage;
  }

  /* build up the frame */
  struct frame *frame = (struct frame*)malloc(sizeof(struct frame));
  frame->addr = kpage;
  frame->upage = upage;
  frame->frame_sourcefile = malloc(sizeof(struct file_d));
  frame->writable = false;
  frame->thread = thread_current();

  /* Initialize frame's page list */
  list_push_back (&eviction_list, &frame->list_elem);

  //release_framelock();
  return kpage;
}


/* TASK 3 : Free's a frame and removes it from frame table */
void
frame_free (void * addr)
{
  //acquire_framelock();
  struct frame *frame = frame_get(addr);
  list_remove (&frame->list_elem);
  palloc_free_page(addr);
  //release_framelock();
  if (frame->frame_sourcefile) {
	     free(frame->frame_sourcefile);
  }
  free(frame);
}

/* TASK 3: Returns pointer to vm_frame given kernel address */
struct frame* frame_get(void *addr) {
	struct list_elem *elem;
	for (elem = list_begin(&eviction_list); elem != list_end(&eviction_list);
		 elem = list_next(elem)) {
			struct frame *fr = list_entry(elem, struct frame, list_elem);
			if(fr->addr == addr) {
				return fr;
			}
	}
	return NULL;
}
