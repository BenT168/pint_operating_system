#include "vm/frame.h"
#include <hash.h>
#include <list.h>
#include "threads/malloc.h"
#include "threads/synch.h"
#include "threads/thread.h"
#include "threads/palloc.h"
#include "userprog/pagedir.h"
#include "userprog/process.h"
#include "vm/page.h"
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

unsigned ft_hash_func (const struct hash_elem *e, void *aux)
{
  struct frame *f;
  f = hash_entry (e, struct frame, elem);

  /* Hash on frame number. */
  return hash_int ((int) f->frame_no);
}

bool ft_is_lower_hash_elem (const struct hash_elem *a,
                            const struct hash_elem *b, void *aux)
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
  page_frames = malloc (sizeof (hash));
  if (!page_frames)
  {
    printf("Frame table creation failure.\n");
    return NULL;
  }
  hash_init (page_frames);
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
ft_create_frame (struct page_table_entry *pte, enum palloc_flags)
{
  struct frame *f = malloc (sizeof (struct frame));

  /* Obtain kernel page. */
  void *kpage = palloc_get_page (flags);

  /* Initialise frame. */
  f->frame_no = kpage & S_PTE_ADDR;
  list_init (&f->pids);
  list_push_back (&f->pids, &thread_current()->frame_elem);
  f->shared = 0;
  lock_init (&f->lock);

  /* Insert into frame table. */
  acquire_ft_lock ();
  hash_insert (page_frames, &f->elem);
  release_ft_lock ();

  return f;
}

/**** SEARCH, LOADING, DELETION ****/

/* Returns frame associated with page table entry 'pte'. NULL if no frame
   exists. We also check that if a frame exists, it is in the frame table. */
struct frame*
ft_get_frame (struct page_table_entry *pte)
{
  if (pte->frame)
  {
    struct hash_elem *h_elem = hash_find (page_frames, &pte->frame->elem);
    ASSERT (hash_find (page_frames, h_elem) != NULL);
  }
  return pte->frame;
}

struct frame*
ft_load_frame (struct frame *f)
{

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
  for (temp = list_begin (&f->pids_entries);
       temp != list_end (&f->pids_entries); temp = list_next (temp))
  {
    /* Get process id - page table entry mapping. */
    struct pid_to_entry *pid_e = list_entry (temp, struct pid_to_entry, elem);

    /* Get thread from process id. */
    struct thread *t = get_tid_thread ((tid_t) pid_e->pid);

    /* Write page to file system. */
    ft_write_to_file (pid_e->pte);

    /* Update supplementary page table entry. */
    lock_acquire (&pid_e->pte->lock);

    pid_e->pte->present = 0;
    pid_e->pte->frame   = NULL;
    pid_e->pte->type    = DISK;

    /* Get process physical page tables. */
    uint32_t *pd = t->pagedir;

    /* Clear physical frame. */
    uintptr_t *upage = (uintptr_t) (pid_e->pte->page_no << PGBITS);
    pagedir_clear_page (pd, upage);

    lock_release (&pid_e->pte->lock);
  }
  /* Remove frame from frame table. */
  acquire_ft_lock ();
  hash_delete (page_frames, h_elem);
  release_ft_lock ();

  /* Free memory. */
  free (f);
}

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
struct frame*
ft_load_frame (struct page_table_entry *pte, enum palloc_flags flags)
{
  struct frame *f;

  if (!pte)
  {
    printf("Attempt to load null page table entry.\n");
    return NULL;
  }
  if (pte->frame)
    /* Entry is already mapped, so return the corresponding page frame. */
    return pte->frame;
  }
  else
  {
    /* Obtain kernel page. */
    void *kpage = palloc_get_page (flags);
    while (!kpage)
    {
      f = ft_evict_frame ();
      kpage = palloc_get_page (flags);
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
    f = malloc (sizeof (struct frame));
    if (!f)
    {
      printf("Frame initialisation failure: malloc failure.\n");
      return NULL;
    }

    /* Read virtual page data file into kernel page. */
    ASSERT ((pte->file_data->page_read_bytes +
             pte->file_data->page_zero_bytes) % PGSIZE == 0);

    // TODO: Lock needed for synchronisation.
    file_read_at (pte->file_data->file, kpage, PGSIZE,
                  pte->file_data->file_ofs);

    /* Frame 'f' is not null at this point. */
    f->frame_no = kpage & S_PTE_ADDR;
    list_init (&f->pids);
    f->shared = 0;
    lock_init (&f->lock);

    /* Insert pid into frames list of processes mapping to it. */
    list_push_back (&f->pids, pte->pid);

    if (list_size (&f->pids) > 1)
      f->shared = 1;

    /* Insert frame into list of page frames. */
    acquire_ft_lock ();
    list_push_back (page_frames, &frame->elem);
    release_ft_lock ();

    /* Update page table entry. */
    lock_acquire (&pte->lock);

    /* Set present bit. */
    pte->present = 1;
    /* Set page type to memory-mapped. */
    pte->type = MMAP;
    /* Set frame of page table entry. */
    pte->frame = frame;
    /* Page has just been loaded and so is not modified or referenced. */
    pte->referenced = 0;
    pte->modified   = 0;

    /* Add mapping in current thread's page tables. */
    uint32_t *pd = thread_current ()->pagedir;
    uintptr_t *upage = (uintptr_t) (pte->page_no << PGBITS);
    bool load_success = pagedir_set_page (pd, upage, kpage, pte->readwrite);

    if (!load_success)
    {
      printf("Memory allocation failure: cannot update process'
                       individual page tables with new mapping.\n");
      palloc_free_page (kpage);
      free (f);
      lock_release (&pte->lock);
      return NULL;
    }
    lock_release (&pte->lock);
  }
  return pte->frame;
}

/* Retrieves the page frame that corresponds to kernel address 'kpage'. If no
   frame holds this address, we return NULL. */
struct frame*
ft_get (void *kpage)
{
  struct list_elem *e;
  unsigned frame_no = kpage & S_PTE_ADDR;

  for (e = list_elem (page_frames); e != list_end (page_frames);
       e = list_next (e)) {
    struct frame *f = list_entry (e, struct frame, elem);

    if (f->frame_no == frame_no)
      return f;
  }
  printf("Frame with kernel address: %p not found.\n", kpage);
  return NULL;
}

/* Evicts a frame. */
void
ft_evict (void)
{

}
