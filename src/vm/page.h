#ifndef VM_PAGE_H
#define VM_PAGE_H

#include <debug.h>
#include <stdint.h>
#include <hash.h>
#include <stdbool.h>
#include "filesys/file.h"
#include "filesys/off_t.h"
#include "threads/palloc.h"
#include "threads/thread.h"

// Bit set to know what we are dealing with
#define SWAP_BIT 0
#define FILE_BIT 1
#define MMAP_BIT 2

static struct lock page_lock;

struct page_table_entry {

  int bit_set;
  int swap_index;
  mapid_t mapid;

  void* phys_addr;                     /* page's physical memory address */
  void *vaddr;                         /* page's user virtual memory address */
  struct file_d *page_sourcefile;      /* info stored if page table is memory mapped
                                          file */
  bool writable;                       /* boolean checking whether the page
                                          table is writable */
  bool loaded;
  bool alive;

  struct hash_elem elem;
};

/* TASK 3: Struct for virtual memory's mapped info. */
struct vm_mmap
  {
    mapid_t mapid;                 /* Unique memory mapped identification */
    struct page_table_entry *pte; /* Pointer to the executing sup page table */
    struct list_elem list_elem;
  };

void page_table_init(struct hash* page_table_hash);
void page_table_destroy (struct hash *hash);
struct page_table_entry* get_page_table_entry(struct hash* hash_table, void* vaddr);
bool insert_page_table_entry(struct hash* hash_table, struct page_table_entry* pte);
bool load_page(struct page_table_entry* pte);
bool load_file(struct page_table_entry* pte);
bool load_swap(struct page_table_entry* pte);
bool insert_file(struct file* file, off_t offset, uint8_t *upage, uint32_t read_bytes, uint32_t zero_bytes, bool writable);
bool insert_mem_map_file(struct file* file, off_t offset, uint8_t *upage, uint32_t read_bytes, uint32_t zero_bytes, bool writable);
bool grow_stack(void* vaddr);
bool check_mmap(struct page_table_entry *pte);
void free_pte(struct page_table_entry* pte);


#endif
