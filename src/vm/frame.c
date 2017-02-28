#include "frame.h"
#include <hash.h>
#include <list.h>
#include "threads/malloc.h"
#include "threads/synch.h"
#include "threads/thread.h"
#include "userprog/pagedir.h"
#include "userprog/process.h"

static struct hash frames;
static struct lock frame_lock;
static struct list eviction_list;

static bool frame_less (const struct hash_elem *a, const struct hash_elem *b, void *aux);
static unsigned frame_hash (const struct hash_elem *e, void *aux);

static void acquire_framelock (void);
static void release_framelock (void);

/* TASK 3 : Acquires lock over frame table */
static void
acquire_framelock (void)
{
  lock_acquire (&frame_lock);
}

/* TASK 3 : Releases lock from frame table */
static void
release_framelock (void)
{
  lock_release (&frame_lock);
}

/* TASK 3 : Initializes the frame hash table and lock */
void
frame_init (void)
{
  hash_init(&frames, frame_hash, frame_less, NULL);
  list_init(&eviction_list);
  lock_init(&frame_lock);
}

/* TASK 3 : Evicts a frame and replaces it with a new one */
void
frame_evict (void)
{
  acquire_framelock();
  if (list_empty (&eviction_list)) {
    return NULL;
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
  lock_release (&frame_lock);

  if (!victim) {
    PANIC("Could not allocate a frame, but no frames are allocated");
  }

  //TODO : Accessed and Dirty Bit
}

/* TASK 3 : Allocates a new page, and adds it to the frame table */
void
frame_get (void * upage, bool zero, bool writable)
{
  void *kpage = palloc_get_page (PAL_USER | zero ? PAL_ZERO : 0 );

  /* evict a frame if not enough memory */
  if (!kpage)
  {
      acquire_framelock();
      frame_evict();
      kpage = palloc_get_page(PAL_USER | zero ? PAL_ZERO : 0);
      release_framelock();
  }
  else
  {
      /* build up the frame */
      struct frame *frame = malloc(sizeof(struct frame));
      frame->addr = kpage;
      frame->upage = upage;
      frame->frame_sourcefile = malloc(sizeof(struct file_d));
		  frame->writable = writable;
      frame->thread = thread_current();
      acquire_framelock();
      hash_insert(&frames, &frame->hash_elem);
      list_push_back (&eviction_list, &frame->list_elem);
      release_framelock();
  }
  return kpage;
}


/* TASK 3 : Free's a frame and removes it from frame table */
void
frame_free (void * addr)
{
  struct hash_elem * found_frame = hash_find(&frames, &frame_elem.hash_elem);
  struct frame frame_elem;
  frame_elem.addr = addr;

	if (found_frame) {
      lock_acquire(&lock_frame);
      struct frame *frame = hash_entry(found_frame, struct frame, hash_elem);
      list_remove (&frame->list_elem);
      palloc_free_page(frame->addr);
      hash_delete(&frames, &frame->hash_elem);
      lock_release(&lock_frame);
		  if (frame->frame_sourcefile) {
			     free(frame->frame_sourcefile);
		  }
		  free(frame);
   }
}

/* TASK 3 : Returns true if a frame 'a' comes before frame 'b'. Orders frames
   by their addresses. */
static bool
frame_less(const struct hash_elem *a_, const struct hash_elem *b_, void *aux UNUSED)
{
	const struct frame * a = hash_entry(a_, struct frame, hash_elem);
	const struct frame * b = hash_entry(b_, struct frame, hash_elem);
	return a->upage < b->upage;
}

/* TASK 3 : Returns a hash value for a frame. Hashes the frame's address as
   addresses should be unique. */
static unsigned
frame_hash(const struct hash_elem *fe, void *aux UNUSED)
{
	const struct frame * frame = hash_entry(fe, struct frame, hash_elem);
	return hash_int((unsigned) frame->upage);
}
