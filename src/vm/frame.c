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
void*
frame_evict (enum palloc_flags flags)
{
  //acquire_framelock();

  if (list_empty (&eviction_list)) {
    return;
  }

  struct list_elem *e = list_begin (&eviction_list);
  struct frame *victim = list_entry (e, struct frame, list_elem);
  struct page_table_entry* pte = get_page_table_entry(&victim->thread->sup_page_table
  , victim->upage);

  while(true) {
    bool pinned = true;
    if(!victim->writable){
      if(pagedir_is_accessed (victim->thread->pagedir, victim->upage)) {
        pagedir_set_accessed (victim->thread->pagedir, victim->upage, false);
        pinned = false;
      }
      if(pinned) {
        if(pte->bit_set == MMAP_BIT) {
            munmap(pte->mapid);
            goto mmap;
        }
      }
       if(check_pagedir_dirty(victim, pte)) {
          pte->loaded = false;
          list_remove(e);
          remove_pte(&victim->thread->sup_page_table, pte);
          pagedir_clear_page(victim->thread->pagedir, victim->upage);
          palloc_free_page(victim->upage);
          frame_free(victim);
          break;
       }
    }
     e = list_next (e);
     if (e == list_end (&eviction_list)) {
     e = list_begin (&eviction_list);
    }
    victim = list_entry (e, struct frame, list_elem);
  }

  mmap:
  return  palloc_get_page(flags);
}


/* TASK 3: Helper function to check if pagedir accessed */
bool check_pagedir_dirty(struct frame* frame, struct page_table_entry* pte) {
  if(pagedir_is_dirty(frame->thread->pagedir, frame->upage)) {
      if(pte->bit_set == SWAP_BIT) {
        struct swap_slot* ss = (struct swap_slot*)malloc(sizeof(struct swap_slot));
        ss->swap_frame = frame;
        ss->swap_addr = (block_sector_t) frame->upage;
        swap_store(ss);
      }
      return true;
  }
  return false;
}


/* TASK 3 : Allocates a new page, and adds it to the frame table */
void*
frame_alloc (void * upage, enum palloc_flags flags)
{
  //void *kpage = palloc_get_page (PAL_USER | zero ? PAL_ZERO : 0 );

  acquire_framelock();

  void* kpage = palloc_get_page(flags);

  release_framelock();

  /* evict a frame if not enough memory or
  Check that frame not mapped to kernel page */
  if (kpage == NULL || frame_get(kpage) != NULL){
    return frame_evict(flags);
  }
  if(kpage != NULL) {

    /* build up the frame */
    struct frame *frame = (struct frame*)malloc(sizeof(struct frame));
    frame->addr = kpage;
    frame->upage = upage;
    frame->frame_sourcefile = malloc(sizeof(struct file_d));
    frame->writable = false;
    frame->thread = thread_current();

    acquire_framelock();
    /* Initialize frame's page list */
    list_push_back (&eviction_list, &frame->list_elem);

    release_framelock();
  }

  return kpage;

}


/* TASK 3 : Free's a frame and removes it from frame table */
void
frame_free (void * addr)
{
  acquire_framelock();
  struct frame *frame = frame_get(addr);
  list_remove (&frame->list_elem);
  palloc_free_page(addr);
  release_framelock();
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
