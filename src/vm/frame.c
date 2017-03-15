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
   eviction policy. */
static struct list *page_frames;


/**** SYNCHRONISATION ****/

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

/**** MAIN FUNCTIONS ****/

/* Creates - but does not initialise - frame table, by dynamically allocating
   memory for a list. */
struct list*
ft_create (void)
{
  if (page_frames)
  {
    fprintf(stderr, "Frame table has already been created.\n");
    return page_frames;
  }
  page_frames = malloc (sizeof (list));
  if (!page_frames)
  {
    fprintf(stderr, "Frame table creation failure.\n");
    return NULL;
  }
  list_init (page_frames);
  lock_init (&ft_lock);
  return page_frames;
}

void
ft_destroy (void)
{
  // TODO
  free(page_frames);
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
ft_load (struct page_table_entry *pte, enum palloc_flags flags)
{
  if (!pte)
  {
    fprintf(stderr, "Attempt to load null page table entry.\n");
    return NULL;
  }
    /* Entry is already mapped, so return the corresponding page frame. */
    return pte->frame;
  }
  else
  {
    /* Obtain kernel page. */
    void *kpage = palloc_get_page (flags);
    if (!kpage)
    {
      fprintf(stderr, "Frame memory allocation error: could not obtain kernel
                       page.\n");
      //TODO: Frame eviction instead
      return NULL;
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
    struct frame *f = malloc (sizeof (struct frame));
    if (!f)
    {
      fprintf(stderr, "Frame initialisation failure: malloc failure.\n");
      return NULL;
    }

    /* Read virtual page data file into kernel page. */
    ASSERT ((pte->file_data->page_read_bytes +
             pte->file_data->page_zero_bytes) % PGSIZE == 0);

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

    /* Add mapping in current thread's page tables. */
    uint32_t *pd = thread_current ()->pagedir;
    uintptr_t *upage = (uintptr_t) (pte->page_no << PGBITS);
    bool load_success = pagedir_set_page (pd, upage, kpage, pte->readwrite);

    if (!load_success){
      fprintf(stderr, "Memory allocation failure: cannot update process'
                       individual page tables with new mapping.\n", );
      palloc_free_page (kpage);
      free (f);
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
  fprintf(stderr, "Frame with kernel address: %p not found.\n", kpage);
  return NULL;
}

/* Evicts a frame. */
void
ft_evict (void)
{

}
