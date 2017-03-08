#include "vm/page.h"
#include "vm/frame.h"
#include "userprog/pagedir.h"
#include "threads/thread.h"


// Function needed for hash table
unsigned
page__hash_table(const struct hash_elem *h_elem, void *aux UNUSED) {
  struct page_table_entry* pte;

  pte = hash_entry(h_elem, struct page_table_entry, elem);

  int hash = (int) pte->vaddr;
  return hash_int(hash);
}


// Function needed for hash table

bool
is_lower_hash_elem(const struct hash_elem* a, hash_elem* b, void *aux UNUSED) {
  const struct page_table_entry* pte_a;
  const struct page_table_entry* pte_b;

  pte_a = hash_entry(a, struct page_table_entry, elem);
  pte_b = hash_entry(b, struct page_table_entry, elem);

  return pte_a < pte_b;
}


// Initialise page table

void
page_table_init(struct hash* page_table_hash) {
  hash_init(page_table_hash, page_hash_table, is_lower_hash_elem, NULL);
}


// Get page table from hash table using key: virtual address
struct page_table_entry*
get_page_table(struct hash* hash_table, void* vaddr) {
  struct page_table_entry* pte;

  pte->vaddr = vaddr;

  struct hash_elem* h_elem = hash_find(hash_table, &pte->elem);

  if(h_elem != NULL) {
    return hash_entry(h_elem, struct page_table_entry, elem);
  }
  return NULL;
}

// Insert page table in hash table
void
insert_page_table(struct hash* hash_table, struct page_table_entry* pte) {
  assert(pte != NULL);

  hash_insert(pte, &pte->elem);
}


// Load page from file_d in page table
bool
load_file(struct page_table_entry* pte) {
  //TODO
  assert(pte->bit_set == FILE_BIT);

  struct file_d* file_d = pte->page_sourcefile;
  struct file* file = file_d->filename;
  int offset = file_d->file_offset;

  file_seek (file, (off_t) offset);

  return true;
}

// load swap page
bool
load_swap(struct page_table_entry* pte) {
  //TODO
  assert(pte->bit_set == SWAP_BIT);

  void* vaddr = pte->vaddr;

  //swap_load(vaddr, )


  return false;
}

// Inserts file_d in page table
void
insert_file(struct file_d* file) {
//TODO
}
