#ifndef VM_PAGE_H
#define VM_PAGE_H

#include <debug.h>
#include "filesys/off_t.h"
#include "threads/palloc.h"

// Bit set to know what we are dealing with
#define SWAP_BIT 0
#define FILE_BIT 1
#define MMAP_BIT 2

struct page_table_entry {

  int bit_set;
  void* vaddr;
  void* phys_addr;

  struct file_d *page_sourcefile;

  bool loaded;
  bool alive;
  bool writable; 

  struct hash_elem elem;

  int swap_index;

};



unsigned page_hash_table(const struct hash_elem *h_elem, void *aux UNUSED);
bool is_lower_hash_elem(const struct hash_elem* a, const struct hash_elem* b, void *aux UNUSED);
void page_table_init(struct hash* page_table_hash);
struct page_table_entry* get_page_table_entry(struct hash* hash_table, void* vaddr);
void insert_page_table_entry(struct hash* hash_table, struct page_table_entry* pte);
bool load_file(struct page_table_entry* pte);
bool load_swap(struct page_table_entry* pte);
bool load_mem_map_file(struct page_table_entry* pte);
bool insert_file(struct file* file, off_t offset, uint8_t *upage,
                             uint32_t read_bytes, uint32_t zero_bytes,
                             bool writable);
bool insert_mem_map_file(struct file* file, off_t offset, uint8_t *upage,
                             uint32_t read_bytes, uint32_t zero_bytes,
                             bool writable);
bool grow_stack(void* vaddr);

#endif
