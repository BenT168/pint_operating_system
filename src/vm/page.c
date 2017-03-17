#include "vm/page.h"
#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include <hash.h>
#include "userprog/pagedir.h"
#include "userprog/process.h"
#include "threads/thread.h"
#include "threads/malloc.h"
#include "threads/palloc.h"
#include "threads/vaddr.h"
#include "filesys/file.h"
#include "vm/swap.h"
#include "vm/frame.h"

static struct lock page_lock;

/* Task 3 : Hash's helper functions to initialise */
static unsigned page_hash_table(const struct hash_elem *h_elem, void *aux UNUSED);
static bool is_lower_hash_elem(const struct hash_elem* a, const struct hash_elem* b, void *aux UNUSED);
static void page_action_func (struct hash_elem *e, void *aux);

static void acquire_pagelock (void);
static void release_pagelock (void);

/* TASK 3 : Acquires lock over sup page table */
void
acquire_pagelock (void)
{
  lock_acquire (&page_lock);
}

/* TASK 3 : Releases lock from sup page table */
void
release_pagelock (void)
{
  lock_release (&page_lock);
}

/* TASK 3 : Returns a hash value for a sub page table. Hashes the page table's address as
   addresses should be unique. */
static unsigned
page_hash_table(const struct hash_elem *h_elem, void *aux UNUSED) {
  struct page_table_entry* pte = hash_entry(h_elem, struct page_table_entry, elem);
  int hash = (int) pte->vaddr;
  return hash_int(hash);
}

/* TASK 3 : Returns true if a hash table 'a' comes before hash table 'b'. Orders hash tables
   by their addresses. */
static bool
is_lower_hash_elem(const struct hash_elem* a, const struct hash_elem* b, void *aux UNUSED) {
  const struct page_table_entry* pte_a;
  const struct page_table_entry* pte_b;

  pte_a = hash_entry(a, struct page_table_entry, elem);
  pte_b = hash_entry(b, struct page_table_entry, elem);

  return pte_a->vaddr < pte_b->vaddr;

}

/* TASK 3 : Free sup page table */
static void
page_action_func (struct hash_elem *e, void *aux UNUSED) {
	struct page_table_entry *pte = hash_entry(e, struct page_table_entry, elem);
	free(pte);
}

/* TASK 3: Initialise page table */
void
page_table_init(struct hash* hash) {
  hash_init(hash, page_hash_table, is_lower_hash_elem, NULL);
  lock_init (&page_lock);
}

/* TASK 3: Destroy page table */
void
page_table_destroy (struct hash *hash) {
  if(hash == NULL) {
    return;
  }
	hash_destroy(hash, page_action_func);
}

// Get page table from hash table using key: virtual address
struct page_table_entry*
get_page_table_entry(struct hash* hash_table, void* vaddr) {
  struct page_table_entry* pte = (struct page_table_entry*)malloc(sizeof(struct page_table_entry));

  pte->vaddr = vaddr;

  struct hash_elem *h_elem = hash_find(hash_table,  &pte->elem);

  if(h_elem != NULL) {
    return hash_entry(h_elem, struct page_table_entry, elem);
  }
  return NULL;
}

// Insert page table entry in hash table
bool
insert_page_table_entry(struct hash* hash_table, struct page_table_entry* pte) {
  if(pte == NULL) {
    return false;
  }
  bool res = false;

  if(hash_insert(hash_table, &pte->elem) == NULL) {
    res = true;
  }

  return res;
}


bool
load_page(struct page_table_entry* pte) {
  bool res = false;
  //Check if page already loaded
  if(pte->loaded) {
    return true;
  }
  switch (pte->bit_set) {
    case FILE_BIT :
    case MMAP_BIT: res = load_file(pte); break;
    case SWAP_BIT : res =  load_swap(pte); break;
  }

  return res;

}
// Load page from file_d in page table
bool
load_file(struct page_table_entry* pte) {

  struct file_d* file_d = pte->page_sourcefile;
  struct file* file = file_d->filename;
  int offset = file_d->file_offset;
  void* upage = pte->vaddr;
  int zero_bytes = file_d->zero_bytes;
  int read_bytes = file_d->read_bytes;

  file_seek(file, offset);

  // Allocate user page
  enum palloc_flags flag;
  if(read_bytes == 0) {
    flag = PAL_ZERO;
  } else {
    flag = PAL_USER;
  }

  /*printf("\n");
  printf("load_file read_bytes: %d\n", read_bytes);
  printf("load_file zero_bytes: %d\n", zero_bytes);
  printf("load_file offset: %d\n", offset);
  printf("load_file upage: %d\n", upage);
  printf("load_file writable: %d\n", pte->writable);
  printf("\n");*/

  void* frame = frame_alloc(upage, flag);
  if(frame == NULL) {
    return false;
  }

  // Load page
  int bytes_read = file_read(file, frame, read_bytes);

  if(bytes_read != read_bytes) {
    // File not read properly, so free frame and return false
    frame_free(frame);
    return false;
  }
  // zero out memory
  if (zero_bytes > 0) {
    memset(frame + read_bytes, 0, zero_bytes);
  }

  // Add the page to the current process address space - add mapping
  // from vaddr to frame
  bool set_page = install_page(pte->vaddr, frame, pte->writable);
  if(!set_page) {
    // Page not set properly, so free frame and return false
    frame_free(frame);
    return false;

  }
  pte->loaded = true;

  return true;
}

// load swap page
bool
load_swap(struct page_table_entry* pte) {
  if(!(pte->bit_set == SWAP_BIT || pte->bit_set == FILE_BIT)) {
    return false;
  }

 // Allocate user page
  void* upage = pte->vaddr;
  void* frame = frame_alloc(upage, PAL_USER);
  if (frame == NULL) {
    return false;
  }
  // Add the page to the current process address space - add mapping
  // from vaddr to frame
  bool set_page = install_page(pte->vaddr, frame, pte->writable);

  struct frame *f = frame_get(frame);

  if(!set_page) {
    // Page not set properly, so free frame and return false
    frame_free(frame);
    return false;
  }

  f->writable = true;

  // Swap from disk -> memory
  struct swap_slot* ss = (struct swap_slot*)malloc(sizeof(struct swap_slot));
  ss->swap_addr = pte->swap_index;
  swap_load(pte->vaddr, ss);
  memset (f + pte->page_sourcefile->read_bytes, 0, pte->page_sourcefile->zero_bytes); // Set 0 bits at end of file if required
  f->writable = false;

  pte->phys_addr = frame;
  pte->bit_set = FILE_BIT;
  pte->loaded = true;

  return true;
}


// Inserts file in page table
bool
insert_file(struct file* file, off_t offset, uint8_t *upage,
                             uint32_t read_bytes, uint32_t zero_bytes,
                             bool writable, int bit_set) {
  struct thread* curr = thread_current();
  struct page_table_entry* pte = (struct page_table_entry*)malloc(sizeof(struct page_table_entry));

  if(pte == NULL) {
    return false;
  }

  pte->bit_set = bit_set;
  pte->vaddr = upage;

  struct file_d* file_d = (struct file_d*)malloc(sizeof(struct file_d));
  file_d->filename = file;
  file_d->file_offset = offset;
  file_d->read_bytes = read_bytes;
  file_d->zero_bytes = zero_bytes;

  pte->page_sourcefile = file_d;
  pte->loaded = false;
  pte->writable = writable;

  // Check for memory mapped files
  if(bit_set == MMAP_BIT) {
    pte->writable = true;
    pte->mapid = curr->mapid;
    if (!check_mmap(pte)) {
      free(pte);
      return false;
    }
  }

  bool success = false;
  // Check if kernel pages currently mapeed to upage
  if(pagedir_get_page(curr->pagedir, upage) == NULL) {
    success = insert_page_table_entry(&curr->sup_page_table, pte);
  }
  return success;
}


// Function for stack growth
bool
grow_stack(void* vaddr) {

  if((size_t)(PHYS_BASE - pg_round_down(vaddr) > MAXI_STACK_SIZE)) {
    return false;
  }

  struct page_table_entry *pte = (struct page_table_entry*)malloc(sizeof(struct page_table_entry));

  if (pte == NULL) {
    return false;
  }

    void* round_vaddr = pg_round_down(vaddr);
    pte->vaddr = round_vaddr;
    pte->loaded = true;
    pte->writable = true;
    pte->bit_set = SWAP_BIT;

    uint8_t *frame = frame_alloc(round_vaddr, PAL_USER);

    if(frame == NULL) {
      free_pte(pte);
      return false;
    }

    bool set_page = install_page(round_vaddr, frame, pte->writable);

    if(!set_page) {
      free(pte);
      frame_free(frame);
      return false;
    }

    if(hash_insert(&thread_current()->sup_page_table, &pte->elem) == NULL) {
      return true;
    }

    return false;
}

bool
check_mmap(struct page_table_entry *pte) {
	struct thread *cur = thread_current();
	struct list *mmaps = &cur->mmapped_files;

	struct vm_mmap *mmap = malloc(sizeof(struct vm_mmap));

	if (!mmap) {
	  return false;
	}

	mmap->mapid = cur->mapid;
	mmap->pte = pte;

	list_push_back (mmaps, &mmap->list_elem);

	return true;
}

// Freeing supplementary page table entry
void
free_pte(struct page_table_entry* pte) {
  free(pte->vaddr);
  free(pte->phys_addr);
  if (pte->page_sourcefile) {
       free(pte->page_sourcefile);
  }
}

void
remove_pte(struct hash* page_table, struct page_table_entry* pte) {
  struct hash_iterator iter;
  hash_first(&iter, page_table);

  void* addr = pte->vaddr;

  while (hash_next(&iter)) {
    struct page_table_entry *page = hash_entry(hash_cur(&iter), struct page_table_entry, elem);
    if(page->vaddr == addr) {
      hash_delete(page_table, &page->elem);
      break;
    }
  }
}
