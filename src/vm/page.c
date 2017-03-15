#include "vm/page.h"
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
#include "vm/ptutils.h"

/* Global page table. */
static struct hash *page_table;

/* Creates the page table by dynamically allocating memory for it.
   Returns true if page table is already initialised, or currently
   uninitialised and successfully initialised by the function. Returns false if
   page table could not be initialised. */
struct hash*
pt_create (void)
{
  if (page_table)
  {
    fprintf(stderr, "Page table has already been created.\n");
    return page_table;
  }
  page_table = malloc (sizeof (hash));
  if (!page_table)
  {
    fprintf(stderr, "Page table creation failure.\n");
    return NULL;
  }
  return page_table;
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
pt_get_page_table (void)
{
  if (!page_table)
    fprintf(stderr, "Warning: Page table uninitialised\n");
  return page_table;
}

/* Hash function for page table entries: computes and returns the hash value of
   'hash_elem'. */
unsigned
pt_hash_func (const struct hash_elem *hash_elem, void *aux UNUSED)
{
  struct page_table_entry *pte;

  pte = hash_entry (hash_elem, struct page_table_entry, elem);

  int hash = (int) pte->page_no;
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

  return pte_a->page_no < pte_b->page_no;
}

/* Intitialise page table.

   Returns true if initialisation was successful and false otherwise. */
bool
pt_init (struct hash *pt)
{
  return hash_init (pt, pt_hash_func, pt_is_lower_hash_elem, NULL);
}

/* Get a page table entry from the page table (implemented as a hash table),
   returning a pointer to page table entry if it exists and NULL otherwise. */
struct page_table_entry*
pt_get_entry (struct hash *pt, unsigned page_no)
{
  struct page_table_entry *pte = (struct page_table_entry*)
                                  malloc (sizeof (struct page_table_entry));
  if (!pte)
  {
    fprintf(stderr, "Malloc Failure in function: pt_get_entry\n");
    return NULL;
  }

  pte->page_no = page_no;
  struct hash_elem *h_elem = hash_find (pt, &pte->elem);

  if (!h_elem)
  {
    return hash_entry (h_elem, struct page_table_entry, &pte->elem);
  }
  free (pte);
  return NULL;
}

/* Inserts the page table entry pointed to by 'pte' into the page table
   (implemented as a hash table) 'pt'.

   Returns 'pte' if no equal element is already in the table. If an equal
   element is already in the table, we return it without inserting the entry
   pointed to by 'pte'. Returns NULL if 'pte' is NULL. All this relies on the
   implementation of 'hash_insert'. */
struct page_table_entry*
pt_insert_entry (struct hash *pt, struct page_table_entry *pte)
{
  if (!pte)
    return NULL;
  struct hash_elem *h_elem = hash_insert (pt, &pte->elem);

  if (!h_elem)
    return pte;
  return hash_entry (h_elem, struct page_table_entry, &pte->elem);
}

/* Deletes the entry 'pte' from the page table. */
void
pt_delete_entry (struct hash *pt, struct page_table_entry *pte)
{
  
}

/* Adds an unmapped page to the page table.

   'type' denotes the the page type (see enums in page.h).
   'flags' denots the flags of the page table entry.
   'file_data' points to the 'file_data' struct which contains the source file
   of the page; the offset at which to start reading; the number of bytes
   actually read into the page; and the remaining zero bytes in the page. */
bool
pt_add_page (void *upage, struct file_d *file_data, uint32_t flags_dword,
             enum page_type type)
{
  struct page_table_entry *pte = malloc (sizeof (page_table_entry));

  /* Initialise page table entry */
  pte->page_no    = ((unsigned) upage) & S_PTE_ADDR;
  pte->present    = flags_dword & S_PTE_P ? 1 : 0;
  pte->readwrite  = flags_dword & S_PTE_W ? 1 : 0;
  pte->referenced = flags_dword & S_PTE_R ? 1 : 0;
  pte->modified   = flags_dword & S_PTE_M ? 1 : 0;
  pte->type       = type;
  pte->file_data  = file_data;
  pte->frame      = NULL;

  struct hash *pt = pt_get_page_table ();

  /* Insert entry into page table */
  if (!pt_insert_entry (pt, pte))
    return false;
  return true;
}
