#include <hash.h>
#include <list.h>
#include <stdio.h>
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

/* Create a page frame (without actually mapping the entry to the frame). */
struct frame*
ft_create_frame (struct page_table_entry *pte, enum palloc_flags flags)
{
  struct frame *f = malloc (sizeof (struct frame));

  /* Obtain kernel page. */
  void *kpage = palloc_get_page (flags);

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

/* Loads virtual page of page table entry, 'pte', into page frame. 'flags'
   indicates what type of page to obtain as well as some other information
   (see palloc.c and palloc.h). If successful, the function returns a pointer to
   the page frame to which 'pte' is now mapped. If unsuccessful, the function
   returns a NULL pointer.

   If 'pte' is already mapped to a page frame, we simply return this page frame.

   Each page frame is uniquely associated with a kernel address, so by
   returning a pointer to the page frame instead of the kernel address to which
   'pte' is now mapped, we simply add an additional layer of abstraction. To
   obtain the corresponding kernel address, we simply dereference the returned
   pointer and obtain the frame number. Moreover, by returning a pointer to the
   page frame, we obtain access to additional information within the frame
   struct. */
// TODO: Change documentation.
struct frame*
ft_load_frame (struct page_table_entry *pte, struct frame *f)
{
  if (!pte)
  {
    printf("Attempt to load null page table entry.\n");
    return NULL;
  }
  else
  {
    if (pte->present)
    {
      /* Entry is already mapped, so return the corresponding page frame. */
      ASSERT (pte->frame != NULL);
      return pte->frame;
    }
    ASSERT (!pte->present);

    /* Obtain kernel page. */
    void *kpage = palloc_get_page (PAL_ZERO);
    while (!kpage)
    {
      ft_evict_frame ();
      kpage = palloc_get_page (PAL_ZERO);
    }

    /* This is a key insight.

       We should never create (and initialise) a page frame unless there is an
       actual page table entry pointing it. Frames with no pages pointing to
       them can be referred to as "ghost frames" and should be throroughly
       avoided. Not only do they occupy memory unnecessarily, but if they are
       allowed to exist, they will likely be difficult to track and can easily
       lead to memory leaks.

       In order to avoid "ghost franes", we make sure that this function,
       'ft_load', presents the only entry point for the creation and
       initialisation of page frames. */
    // TODO: Change above comment

    /* Load segment. */


    /* Read virtual page data file into kernel page. */
    ASSERT ((pte->file_data->page_read_bytes +
             pte->file_data->page_zero_bytes) % PGSIZE == 0);

    // TODO: Lock needed for synchronisation.
    /* Load segment (see 'load_segment' in process.c). */
    struct file_d *f_data = pte->file_data;

    load_segment (f_data->file, f_data->file_ofs, )
    file_read_at (pte->file_data->file, kpage, PGSIZE,
                  pte->file_data->file_ofs);

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

    /* Update page table entry. */
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

    /* Add mapping in current thread's page tables. */
    uint32_t *pd = thread_current ()->pagedir;
    uint32_t *upage = (uint32_t*) (pte->page_no << PGBITS);
    bool load_success = pagedir_set_page (pd, upage, kpage, pte->readwrite);

    if (!load_success)
    {
      printf("Memory allocation failure: cannot update process' individual page tables with new mapping.\n");
      palloc_free_page (kpage);
      free (f);
      lock_release (&pte->lock);
      PANIC ("Could not loas physical frames.");
    }
    lock_release (&pte->lock);
    return pte->frame;
  }
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
