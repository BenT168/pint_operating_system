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
#define SWAP_BIT 0	  	               /*bit referring to page in the swap partition*/
#define FILE_BIT 1 	 	                 /*bit referring to page referring to a file*/
#define MMAP_BIT 2  	                 /*bit referring to page representing a memory map file*/

#define MAXI_STACK_SIZE (1 << 26)

struct page_table_entry {

  int bit_set;                         /* Used to store the page's current status. */
  int swap_index;                      /* Index used for swapping */
  mapid_t mapid;                       /* Unique id for memory mapped file */
  void* phys_addr;                     /* page's physical memory address */
  void *vaddr;                         /* page's user virtual memory address */
  struct file_d *page_sourcefile;      /* info stored if page table is memory mapped
                                          file */
  bool writable;                       /* boolean checking whether the page
                                          table is writable */
  bool loaded;                         /* boolean checking whether the page
                                          table is loadable */
  struct hash_elem elem;               /* Used to store the pte in the page table. */
};

/* TASK 3: Struct for virtual memory's mapped info. */
struct vm_mmap
  {
    mapid_t mapid;                 /* Unique memory mapped identification */
    struct page_table_entry *pte; /* Pointer to the executing sup page table */
    struct list_elem list_elem;   /* Used to store the memory map files  */
  };

void page_table_init(struct hash* page_table_hash);
void page_table_destroy (struct hash *hash);
struct page_table_entry* get_page_table_entry(struct hash* hash_table, void* vaddr);
bool insert_page_table_entry(struct hash* hash_table, struct page_table_entry* pte);
bool load_page(struct page_table_entry* pte);
bool load_file(struct page_table_entry* pte);
bool load_swap(struct page_table_entry* pte);
bool insert_file(struct file* file, off_t offset, uint8_t *upage, uint32_t read_bytes, uint32_t zero_bytes, bool writable, int bit_set);
bool grow_stack(void* vaddr);
bool check_mmap(struct page_table_entry *pte);
void free_pte(struct page_table_entry* pte);


#endif /* vm/frame.h */
