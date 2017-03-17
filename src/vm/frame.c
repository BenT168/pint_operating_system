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
  struct list_elem *e = list_begin (&eviction_list);
  struct frame *victim = list_entry (e, struct frame, list_elem);
  struct page_table_entry* pte = get_page_table_entry(&victim->thread->sup_page_table
  , victim->upage);

  lock_acquire(&victim->single_frame_lock);
  while(true) {
    if(!victim->writable){
        bool accessable = true;
        for (e = list_begin(&eviction_list); e != list_end(&eviction_list); e = list_next(e)){
          /* Check that the frame has been accessed */
          if(pagedir_is_accessed (victim->thread->pagedir, victim->upage)) {
              pagedir_set_accessed (victim->thread->pagedir, victim->upage, false);
              accessable = false;
          }
        }
        if (accessable) {
          for (e = list_begin(&eviction_list); e != list_end(&eviction_list); e = list_next(e)){
            /* If frame has been accessed and has a MMAP_BIT, then unmap */
            if(accessable && pte->bit_set == MMAP_BIT ) {
                    munmap(pte->mapid);
                    goto mmap;
            }
            break;
          }
        }
    }

    if ( e == list_end (&eviction_list)) {
      e = list_begin (&eviction_list);
    }
    victim = list_entry (e, struct frame, list_elem);
  }

  lock_release(&victim->single_frame_lock);

  /* If frame has not been modified and not been accessed */
  if(check_pagedir_not_dirty(victim, pte)&& !pagedir_is_accessed (victim->thread->pagedir, victim->upage)) {
        pte->loaded = false;
        acquire_framelock();
        list_remove(e);
        list_push_back(&eviction_list, e);
        release_framelock();
  }

  mmap:
  acquire_framelock();
  victim = palloc_get_page(flags);
  release_framelock();

  return victim;
}


/* TASK 3: Helper function to check if pagedir accessed */
bool check_pagedir_not_dirty(struct frame* frame, struct page_table_entry* pte) {
  /* Check if frame has not been modified */
  if(!pagedir_is_dirty(frame->thread->pagedir, frame->upage)) {
      /* If page table entry is a SWAP_BIT, then swap frame out */
      if(pte->bit_set == SWAP_BIT) {
        struct swap_slot* ss = (struct swap_slot*)malloc(sizeof(struct swap_slot));
        ss->swap_frame = frame;
        size_t index = swap_store(frame->upage);
        ss->swap_addr = index;
        pagedir_clear_page(frame->thread->pagedir, pte->vaddr);
      }
      /* Free the frame */
      frame_free(frame);
      return true;
  }
  return false;
}


/* TASK 3 : Allocates a new page, and adds it to the frame table */
void*
frame_alloc (void * upage, enum palloc_flags flags)
{
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
    lock_init(&frame->single_frame_lock);

    /* Initialize frame's page list */
    acquire_framelock();
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
  /* Get frame mapped to address and unmap the address */
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
  /* Go through eviction list and get frame with the address addr */
	for (elem = list_begin(&eviction_list); elem != list_end(&eviction_list);
		 elem = list_next(elem)) {
			struct frame *fr = list_entry(elem, struct frame, list_elem);
			if(fr->addr == addr) {
				return fr;
			}
	}
  /* If no frame found with address addr, then return NULL */
	return NULL;
}
