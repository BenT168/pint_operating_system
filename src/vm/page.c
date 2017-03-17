#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include <hash.h>
#include "userprog/pagedir.h"
#include "threads/thread.h"
#include "threads/malloc.h"
#include "threads/palloc.h"
#include "threads/vaddr.h"
#include "filesys/file.h"
#include "vm/swap.h"
#include "vm/frame.h"
#include "vm/page.h"
#include "vm/ptutils.h"

static int
pt_create_hash_int (unsigned page_no, pid_t pid)
{
  return (int) (page_no * page_no + page_no + (int) pid);
}

/******************************/
/**** PAGE TABLE LIFE CYCLE ***/
/******************************/

/* Creates the page table by dynamically allocating memory for it.
   Returns true if page table is already initialised, or currently
   uninitialised and successfully initialised by the function. Returns false if
   page table could not be initialised. */
struct hash*
pt_create (struct thread *t)
{
  if (!t)
  {
    printf("Cannot create page table for null process.\n");
    return NULL;
  }

  if (t->pt)
  {
    printf("Page table has already been created.\n");
    return t->pt;
  }

  struct hash *pt = malloc (sizeof (struct hash));
  if (!pt)
  {
    printf("Malloc failure: Cannot create page table for process %d.\n", t->pid);
    return NULL;
  }
  t->pt = pt;
  return pt;
}

/* Intitialise page table.

   Returns true if initialisation was successful and false otherwise. */
bool
pt_init (struct hash *pt)
{
  return hash_init (pt, pt_hash_func, pt_is_lower_hash_elem, NULL);
}

/* Removes all entries from thread 't''s supplementary page table.

   TODO: Version 2: Add destructor. */
void
pt_clear (struct thread *t)
{
  if (!(t->pt))
    return;
  hash_clear (t->pt, NULL);
}

/* Destroys page table. Returns true if the function executes with no fatal
   errors. */
bool
pt_destroy (struct hash *pt)
{
  if (!pt)
  {
    hash_destroy (pt, NULL);
    free (pt);
  }
  return true;
}



/* Retrieves the page table. */
struct hash*
pt_get_page_table (struct thread *t)
{
  if (!(t->pt))
    printf("Warning: Page table uninitialised.\n");
  return t->pt;
}

/**********************/
/**** HASH FUNCTION ***/
/**********************/

/* Hash function for page table entries: computes and returns the hash value
   of 'hash_elem'. */
unsigned
pt_hash_func (const struct hash_elem *hash_elem, void *aux UNUSED)
{
  struct page_table_entry *pte;

  pte = hash_entry (hash_elem, struct page_table_entry, elem);

  int hash = pt_create_hash_int (pte->page_no, pte->pid);
  return hash_int (hash);
}

/* Comparator for hash elements.

   The actual hashed entries 'a' and 'b' are page table entries. This function
   returns true if and only if the page number corresponding to the page table
   entry of 'a' is less than that of 'b'. */
bool
pt_is_lower_hash_elem (const struct hash_elem *a, const struct hash_elem *b,
                       void *aux UNUSED)
{
  const struct page_table_entry *pte_a;
  const struct page_table_entry *pte_b;

  pte_a = hash_entry(a, struct page_table_entry, elem);
  pte_b = hash_entry(b, struct page_table_entry, elem);
  int hashint_a = pt_create_hash_int (pte_a->page_no, pte_a->pid);
  int hashint_b = pt_create_hash_int (pte_b->page_no, pte_b->pid);

  return hashint_a < hashint_b;
}

/***************************/
/**** PAGE TABLE ENTRIES ***/
/***************************/

/**** CREATION ****/

/* Creates a page table entry.

   'type' denotes the the page type (see enums in page.h).
   'flags' denots the flags of the page table entry.
   'file_data' points to the 'file_data' struct which contains the source file
   of the page; the offset at which to start reading; the number of bytes
   actually read into the page; and the remaining zero bytes in the page.

   Returns a pointer to the page table entry. */
struct page_table_entry*
pt_create_entry (struct thread *t, void *upage, struct file_d *file_data,
                 uint32_t flags_dword, enum page_type type)
{
  struct page_table_entry *pte = malloc (sizeof (struct page_table_entry));

  /* Initialise page table entry */
  pte->pid        = t->pid;
  pte->page_no    = ((unsigned) upage) & S_PTE_ADDR;
  pte->present    = flags_dword & S_PTE_P ? 1 : 0;
  pte->readwrite  = flags_dword & S_PTE_W ? 1 : 0;
  pte->referenced = flags_dword & S_PTE_R ? 1 : 0;
  pte->modified   = flags_dword & S_PTE_M ? 1 : 0;
  pte->type       = type;
  pte->file_data  = file_data;
  pte->frame      = NULL;

  return pte;
}

/**** SEARCH, INSERTION, DELETION ****/

/* Get a page table entry from the page table (implemented as a hash table),
   returning a pointer to page table entry if it exists and NULL otherwise. */
struct page_table_entry*
pt_get_entry (unsigned page_no)
{
  struct thread *t = thread_current ();
  struct hash *pt = pt_get_page_table (t);
  if (!pt)
  {
    printf("Thread %d: Cannot retrieve entry from null page table.\n",
            t->tid);
    return NULL;
  }

  struct page_table_entry pte;
  pte = (struct page_table_entry) {.page_no = page_no, .pid = t->pid};

  struct hash_elem *h_elem = hash_find (pt, &pte.elem);

  if (!h_elem)
  {
    return hash_entry (h_elem, struct page_table_entry, elem);
  }
  return NULL;
}

/* Inserts 'pte' into the supplementary page table of thread 't', returing NULL if no equal element is already in the table. If an equal element is already in the table, returns it without inserting 'pte'. */
struct page_table_entry*
pt_insert_entry (struct thread *t, struct page_table_entry *pte)
{
  struct hash *pt = pt_get_page_table (t);

  if (!pte)
    return NULL;
  if (!pt)
  {
    printf("Thread: %d: Cannot insert into null page table.\n",
            t->tid);
    return NULL;
  }
  struct hash_elem *h_elem = hash_insert (pt, &pte->elem);

  if (!h_elem)
    return pte;
  return hash_entry (h_elem, struct page_table_entry, elem);
}

/* Inserts 'pte_new' into the supplementary page table of thread 't' and
   returns a null pointer, if no equal element is already in the table. If an
   equal element is already in the table, returns it without inserting
   'pte_new'. */
struct page_table_entry*
pt_replace_entry (struct thread *t, struct page_table_entry *pte_new)
{
  struct hash *pt = pt_get_page_table (t);

  if (!pte_new)
    return NULL;
  if (!pt)
  {
    printf("Thread: %d: Cannot replace entry in null page table.\n",
            t->tid);
    return NULL;
  }

  struct hash_elem *old = hash_find (pt, &pte_new->elem);

  if (!old)
    hash_insert (pt, &pte_new->elem);

  return old ? hash_entry (old, struct page_table_entry, elem) : NULL;
}

/* Deletes the entry 'pte' from the supplementary page table of thread 't'. */
void
pt_delete_entry (struct thread *t, struct page_table_entry *pte)
{
  struct hash *pt = pt_get_page_table (t);

  if (!pt)
    printf("Thread: %d: Cannot delete entry from null page table.\n",
            t->tid);

  if (!pte)
  {
    hash_delete (pt, &pte->elem);
    free (pte);
  }
}

/********************************/
/**** PAGE TABLE INFORMATION ****/
/********************************/

/* Returns size of the supplementary page table of thread 't'. If the page
   table is NULL, then it returns 0. */
size_t pt_size (struct thread *t)
{
  struct hash *pt = pt_get_page_table (t);
  if (!pt)
  {
    printf("Thread %d: Warning: Retrieving size of null page table.\n", t->tid);
    return 0;
  }
  return hash_size (pt);
}

/* Returns true iff the supplementary page table of thread 't' is initialised
   and has no entries. */
bool pt_is_empty (struct thread *t)
{
  struct hash *pt = pt_get_page_table (t);
  if (!pt)
  {
    printf("Thread %d: Warning: Checking emptiness of null page table.\n", t->tid);
    return false;
  }
  return hash_size (pt) == 0;
}
