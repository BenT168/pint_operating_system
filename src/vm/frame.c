#include <hash.h>
#include <list.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "threads/malloc.h"
#include "threads/synch.h"
#include "threads/thread.h"
#include "threads/palloc.h"
#include "userprog/pagedir.h"
#include "userprog/process.h"
#include "vm/frame.h"
#include "vm/ptutils.h"

/* We maintain a global list of frames and correspondingly, use a global
   eviction policy.

   We want to ensure that no more than one frame is associated with a given
   kernel page. In order to do so, we implement the frame table as implemented
   as a hash table, hashing on the frame number of the frame - which is equal
   to the 20 high order bits of the frame's kernel base address. */
static struct hash *page_frames;

/*************************/
/**** SYNCHRONISATION ****/
/*************************/

static void acquire_ft_lock (void);
static void release_ft_lock (void);

/* Lock for frame table. */
static struct lock ft_lock;

static void
acquire_ft_lock (void)
{
  lock_acquire (&ft_lock);
}

static void
release_ft_lock (void)
{
  lock_release (&ft_lock);
}

/***********************/
/**** HASH FUNCTION ****/
/***********************/

unsigned ft_hash_func (const struct hash_elem *e, void *aux UNUSED)
{
  struct frame *f;
  f = hash_entry (e, struct frame, elem);

  /* Hash on frame number. */
  return hash_int ((int) f->frame_no);
}

bool ft_is_lower_hash_elem (const struct hash_elem *a,
                            const struct hash_elem *b, void *aux UNUSED)
{
  const struct frame *fa;
  const struct frame *fb;

  fa = hash_entry (a, struct frame, elem);
  fb = hash_entry (b, struct frame, elem);

  return hash_int ((int) fa->frame_no) < hash_int ((int) fb->frame_no);
}

/**************************/
/**** BASIC LIFE CYCLE ****/
/**************************/

/* Creates - but does not initialise - frame table, by dynamically allocating
   memory for a list. */
struct hash*
ft_create (void)
{
  if (page_frames)
  {
    printf("Frame table has already been created.\n");
    return page_frames;
  }
  page_frames = malloc (sizeof (struct hash));
  if (!page_frames)
  {
    printf("Frame table creation failure.\n");
    return NULL;
  }
  hash_init (page_frames, ft_hash_func, ft_is_lower_hash_elem, NULL);
  lock_init (&ft_lock);
  return page_frames;
}

void
ft_clear (void)
{
  hash_clear (page_frames, NULL);
}

void
ft_destroy (void)
{
  hash_destroy (page_frames, NULL);
  free(page_frames);
}

/* Retrieves frame table. */
struct hash*
ft_get_frame_table (void)
{
  return page_frames;
}

/*********************/
/**** PAGE FRAMES ****/
/*********************/

/**** CREATION ****/

/* Create a page frame (without actually mapping the entry to the frame). Returns a pointer to frame if successful and null otherwise. */
struct frame*
ft_create_frame (struct page_table_entry *pte, enum palloc_flags flags)
{
  struct frame *f = malloc (sizeof (struct frame));

  /* Obtain kernel page. */
  void *kpage = palloc_get_page (flags);
  if (!kpage)
  {
    printf("Failed to obtain kernel page while attempting to create page frame.\n");
    return NULL;
  }

  /* Initialise frame. */
  f->frame_no = ((unsigned) kpage) & S_PTE_ADDR;
  list_init (&f->entries);
  list_push_back (&f->entries, &pte->frame_elem);
  f->shared = 0;
  lock_init (&f->lock);

  /* Insert into frame table. */
  acquire_ft_lock ();
  hash_insert (page_frames, &f->elem);
  release_ft_lock ();

  return f;
}

/**** RETRIEVAL ****/

/* Returns frame associated with page table entry 'pte'. NULL if no frame
   exists. We also check that if a frame exists, it is in the frame table. */
struct frame*
ft_get_frame (struct page_table_entry *pte)
{
  if (pte->frame)
  {
    struct hash_elem *h_elem = hash_find (page_frames, &pte->elem);
    ASSERT (hash_find (page_frames, h_elem) != NULL);
  }
  return pte->frame;
}

/**** DELETION ****/

/* Deletes frame from frame table. */
void
ft_delete_frame (struct frame *f)
{
  if (!f)
    return;

  struct hash_elem *h_elem = hash_find (page_frames, &f->elem);

  if (!h_elem)
  {
    printf("Attempt to delete nonexistent frame.\n");
    return;
  }

  /* Obtain list of threads with virtual page mappings to this frame and clear
     the mappings. */
  struct list_elem *temp;
  for (temp = list_begin (&f->entries);
       temp != list_end (&f->entries); temp = list_next (temp))
  {
    /* Get page table entry. */
    struct page_table_entry *pte;
    pte = list_entry (temp, struct page_table_entry, frame_elem);

    /* Get thread from process id. */
    struct thread *t = get_tid_thread ((tid_t) pte->pid);

    /* Write page to file system. */
    ft_write_to_file (pte);

    /* Update supplementary page table entry. */
    lock_acquire (&pte->lock);

    pte->present = 0;
    pte->frame   = NULL;
    pte->type    = DISK;

    /* Get process physical page tables. */
    uint32_t *pd = t->pagedir;

    /* Clear physical frame. */
    uint32_t *upage = (uint32_t*) (pte->page_no << PGBITS);
    pagedir_clear_page (pd, upage);

    lock_release (&pte->lock);
  }
  /* Remove frame from frame table. */
  acquire_ft_lock ();
  hash_delete (page_frames, h_elem);
  release_ft_lock ();

  /* Free memory. */
  free (f);
}

/**** LOADING ****/

/* Load helpers. */
static bool install_page (void *upage, void *kpage, bool writable);

/* Loads frame 'f' into memory and associates with page table entry 'pte'. If
   'pte' is already associated with a frame, we replace this frame with 'f.
   That is, we have 'pte' point to the new frame 'f' and if  'pte''s old frame
   is now a ghost frame, we evict it.

   If 'f' is null or the loading procedure fails, we return null. Otherwise we
   return a pointer to the newly loaded frame.

   Each page frame is uniquely associated with a kernel address, so by
   returning a pointer to the page frame instead of the kernel address to which
   'pte' is now mapped, we simply add an additional layer of abstraction. To
   obtain the corresponding kernel address, we simply dereference the returned
   pointer and obtain the frame number. Moreover, by returning a pointer to the
   page frame, we obtain access to additional information within the frame
   struct. */
struct frame*
ft_load_frame (struct page_table_entry *pte, struct frame *f)
{
  if (!f)
  {
    printf("Attempt to load null frame.\n");
    return NULL;
  }
  if (!pte)
  {
    printf("Attempt to load null page table entry.\n");
    return NULL;
  }
  else if (pte->present)
  {
    /* Remove old mapping and create new mapping. */
    lock_acquire (&pte->lock);
    list_remove (&pte->frame_elem);

    lock_acquire (&f->lock);
    list_push_back (&f->entries, &pte->frame_elem);
    lock_release (&f->lock);

    pte->frame = f;
    lock_release (&pte->lock);
  }
  /* Obtain kernel page. */
  uint32_t *kpage = palloc_get_page (PAL_USER);
  while (!kpage)
  {
    ft_evict_frame ();
    kpage = palloc_get_page (PAL_USER);
  }

  /* We should never create (and initialise) a page frame unless there is an
     actual page table entry pointing it. Frames with no pages pointing to
     them can be referred to as "ghost frames" and should be throroughly
     avoided. Not only do they occupy memory unnecessarily, but if they are
     allowed to exist, they will likely be difficult to track and can easily
     lead to memory leaks. */

  /* Load physical frames. TODO: Check if race condition. */
  size_t page_read_bytes = pte->file_data->page_read_bytes < PGSIZE ?
                           pte->file_data->page_read_bytes : PGSIZE;
  size_t page_zero_bytes = PGSIZE - page_read_bytes;

  off_t bytes_read = file_read_at (pte->file_data->file, kpage, page_read_bytes,
                                   pte->file_data->file_ofs);
  if (bytes_read != (int) page_read_bytes)
  {
    palloc_free_page (kpage);
    return NULL;
  }
  memset (kpage + page_read_bytes, 0, page_zero_bytes);

  /* Add page to the process's address space. */
  void *upage = (void*) (((unsigned) pte->page_no) << PGBITS);
  if (!install_page (upage, kpage, pte->readwrite))
  {
    palloc_free_page (kpage);
    return false;
  }

  /* Update frame. */
  /* Frame 'f' is not null at this point. */
  f->frame_no = ((unsigned) kpage) & S_PTE_ADDR;
  list_init (&f->entries);
  f->shared = 0;
  lock_init (&f->lock);

  /* Insert entry into frames list of processes mapping to it. */
  list_push_back (&f->entries, &pte->frame_elem);

  if (list_size (&f->entries) > 1)
    f->shared = 1;

  /* Insert frame into list of page frames. */
  acquire_ft_lock ();
  hash_insert (page_frames, &f->elem);
  release_ft_lock ();

  /* Update supplementary page table entry. */
  lock_acquire (&pte->lock);

  /* Set present bit. */
  pte->present = 1;
  /* Set page type to memory-mapped. */
  pte->type = MMAP;
  /* Set frame of page table entry. */
  pte->frame = f;
  /* Page has just been loaded and so is not modified or referenced. */
  pte->referenced = 0;
  pte->modified   = 0;

  lock_release (&pte->lock);
  return pte->frame;
}

/**** EVICTION ****/

/* Evicts a frame. */
struct frame*
ft_evict_frame (void)
{
  struct frame *f = malloc (sizeof (struct frame));
  return f;
}

/**** COPYING ****/

/* Creates a new page frame; copying the data from the page frame 'f'.
   We return a poiner to the copied frame. This function does not update
   the supplementary page tables or the physical page tables. */
struct frame*
ft_copy_frame (struct frame *f)
{
  /* Obtain kernel page from frame 'f' */
  void *kpage = (void*) (((unsigned) f->frame_no) << PGBITS);

  struct frame *copy_f = malloc (sizeof (struct frame));

  /* Initialise. */
  copy_f->frame_no = ((unsigned) kpage) & S_PTE_ADDR;
  list_init (&copy_f->entries);
  copy_f->shared = 0;
  lock_init (&copy_f->lock);

  acquire_ft_lock ();
  hash_insert (page_frames, &copy_f->elem);
  release_ft_lock ();

  return copy_f;
}

bool ft_write_to_file (struct page_table_entry *pte)
{
  //TODO
  return true;
}

/* Adds a mapping from user virtual address UPAGE to kernel
   virtual address KPAGE to the page table.
   If WRITABLE is true, the user process may modify the page;
   otherwise, it is read-only.
   UPAGE must not already be mapped.
   KPAGE should probably be a page obtained from the user pool
   with palloc_get_page().
   Returns true on success, false if UPAGE is already mapped or
   if memory allocation fails. */
static bool
install_page (void *upage, void *kpage, bool writable)
{
  struct thread *t = thread_current ();

  /* Verify that there's not already a page at that virtual
     address, then map our page there. */
  return (pagedir_get_page (t->pagedir, upage) == NULL
          && pagedir_set_page (t->pagedir, upage, kpage, writable));
}
