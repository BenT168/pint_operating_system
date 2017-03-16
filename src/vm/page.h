#ifndef VM_PAGE_H
#define VM_PAGE_H

#include <debug.h>
#include <stdint.h>
#include <hash.h>
#include <stdbool.h>
#include "filesys/file.h"
#include "filesys/off_t.h"
#include "threads/palloc.h"

// Bit set to know what we are dealing with
#define SWAP_BIT 0
#define FILE_BIT 1
#define MMAP_BIT 2

/* Every page table entry has a pointer pointing to the file that the contains
   the data that goes into the page. At any given point, the page's data may be
   on the file system, in a swap slot, in a page frame or the page may be an
   all zero page. The enums below store this information.

   'SWAP' pages are in the process' swap partition on disk and not in main
   memory. Note that each process has a swap partition on disk. However, a process' pages do
   not have fixed addresses in the swap partition to which they are written when
   the process is started up or when a frame is evicted from memory. Instead, when a page is swapped out, a swap slot in the swap partition is dynamically created and the page written to this slot. The swap table - which has room
   for one disk address per virtual page - is updated accordingly. A page in main memory has no copy on disk. The pages' entries is the swap table contain an invalid disk address or a bit marking them as not in use.

   'DISK' pages are in the file system. They are not in a swap partition and have not been mapped to memory.

   'MMAP' pages are memory-mapped pages. These pages have been loaded into page frames in main memory.

   'ALLZERO' pages are pages initialised with all zeros. They are used for inter-process communication via the copy-on-write mechanism. */
enum page_type
{
  SWAP    = 001,
  DISK    = 002,
  MMAP    = 003,
  ALLZERO = 004
};

/* The page table will implemented as a hash table, using the provided
   hash table implementation. */

/* Ideally, the page number and process id would be used as an index to the
   page table. However, since the provided hash table is implemented as a
   bucket of values instead of a map from keys from values, we store the page
   number and process id within the page table entry and use them as a
   unique identifier of page table entries.

   Page table entries are hashed and stored the page table. */
struct page_table_entry
{
  unsigned present:1;        /* Denotes whether page is mapped. */
  unsigned readwrite:1;      /* 0 for READ-ONLY access, 1 for READ and
                                WRITE access. */
  unsigned referenced:1;     /* 1 if page was referenced during the last
                                clock cycle. 0 if it wasn't. */
  unsigned modified:1;       /* 1 if page was modified during the last
                                clock cycle. 0 if it wasn't. */
  unsigned page_no:20;       /* Page number: equivalent to the 20 high
                                order bits of any 32-bit virtual address into
                                the page. Used as a unique identifier of the
                                page table entry. */
  void* frame;               /* If the page is unmapped, frame = NULL,
                                otherwise 'frame' points to the
                                corresponding page frame. */
  enum page_type type;       /* Page type: see enum above. */

  struct file_d *file_data;  /* Pointer to file data containing
                                actual page data. */
  pid_t pid;                 /* Id of process to which this page belongs. */

  struct lock lock;          /* Lock to implement mutually exclusive access to
                                page table entry. */
  struct hash_elem elem;
};

/* @DEPRECATED */
enum pte_flags
{
  PRESENT    = 001 << 3,
  READWRITE  = 001 << 2,
  REFERENCED = 001 << 1,
  MODIFIED   = 001
};

/* The process image held in the file 'filename' is subdivided into a number of pages. Each page consists of 'read_bytes + zero_bytes' bytes of the file starting at 'file_offset'. */
/* 'read_bytes' denotes the number of bytes read from file 'filename', starting at 'file_offset'. The 'file_d' data structure accompanies the 'page_table_entry' data structure. 'zero_bytes' denotes the number of unused bytes in the page.*/
struct file_d
{
  struct file *file;
  off_t file_ofs;
  size_t page_read_bytes;
  size_t page_zero_bytes;
};

/* Functions for page table. */

/* Basic life cycle. */
struct hash* pt_create (struct thread *t);
bool         pt_init (struct hash *pt);
void         pt_clear (struct thread *t);
bool         pt_destroy (struct hash *pt);

/* Page table retrieval. */
struct hash* pt_get_page_table (struct thread *t);

/* For hash function. */
unsigned pt_hash_func (const struct hash_elem *hash_elem, void *aux);
bool     pt_is_lower_hash_elem (const struct hash_elem *a,
                                const struct hash_elem *b,
                                void *aux);

/* Functions for page table entries */

/* Creation. */
struct page_table_entry*
pt_create_entry (struct thread *t,
                 void *upage,
                 struct file_d *file_data,
                 uint32_t flags_dword,
                 enum page_type type);

/* Search, insertion, deletion. */
struct page_table_entry*
pt_get_entry (struct thread *t,
              unsigned page_no);

struct page_table_entry*
pt_insert_entry (struct thread *t,
                 struct page_table_entry *pte);

struct page_table_entry*
pt_replace_entry (struct thread *t,
                  struct page_table_entry *pte);

void
pt_delete_entry (struct thread *t,
                 struct page_table_entry *pte);

/* Information. */
size_t pt_size (struct hash *pt);
bool pt_is_empty (struct thread *t);

#endif
