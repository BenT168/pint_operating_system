#ifndef VM_PAGE_H
#define VM_PAGE_H

#define PGSIZE 4096

// Bit set to know what we are dealing with
#define SWAP_BIT 0
#define FILE_BIT 1

struct page_table_entry {

  int bit_set;
  void* vaddr;
  void* phys_addr;

  struct file_d *page_sourcefile;

  bool loaded;
  bool alive;

  struct hash_elem elem;

  int swap_index;

};


#endif
