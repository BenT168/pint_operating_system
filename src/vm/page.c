#include "vm/page.h"
#include "userprog/pagedir.h"
#include "threads/thread.h"
#include "threads/palloc.h"
#include "vm/swap.h"
#include "vm/frame.h"


// Function needed for hash table
unsigned
page_hash_table(const struct hash_elem *h_elem, void *aux UNUSED) {
  struct page_table_entry* pte;

  pte = hash_entry(h_elem, struct page_table_entry, elem);

  int hash = (int) pte->vaddr;
  return hash_int(hash);
}


// Function needed for hash table

bool
is_lower_hash_elem(const struct hash_elem* a, const struct hash_elem* b, void *aux UNUSED) {
  const struct page_table_entry* pte_a;
  const struct page_table_entry* pte_b;

  pte_a = hash_entry(a, struct page_table_entry, elem);
  pte_b = hash_entry(b, struct page_table_entry, elem);

  return pte_a < pte_b;



// Initialise page table

void
page_table_init(struct hash* page_table_hash) {
  hash_init(page_table_hash, page_hash_table, is_lower_hash_elem, NULL);
}


// Get page table from hash table using key: virtual address
struct page_table_entry*
get_page_table_entry(struct hash* hash_table, void* vaddr) {
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
insert_page_table_entry(struct hash* hash_table, struct page_table_entry* pte) {
  assert(pte != NULL);

  hash_insert(pte, &pte->elem);
}


// Load page from file_d in page table
bool
load_file(struct page_table_entry* pte) {
  assert(pte->bit_set == FILE_BIT);

  struct file_d* file_d = pte->page_sourcefile;
  struct file* file = file_d->filename;
  int offset = file_d->file_offset;
  file_seek (file, (off_t) offset);
  
  // Allocate user page
  enum paloc_flags flag; 
  int read_bytes = file_d->read_bytes;
  if(read_bytes == 0) {
    flag = PAL_ZERO; 
  } else {
    flag = PAL_USER; 
  }
  void* frame = frame_allocate(flag);
  if(frame == NULL) {
    return false; 
  }

  // Load page 
  int zero_bytes = file_d->zero_bytes; 
  int read_bytes = file_d->read_bytes; 
  int bytes_read = file_read(file, (uint8_t) frame, read_bytes);
  
  if(bytes_read != read_bytes) {
    // File not read properly, so free frame and return false
    frame_free(frame);
    return false; 
  }
  // zero out memory 
  memset(frame + read_bytes, 0, zero_bytes); 

  // Add the page to the current process address space - add mapping 
  // from vaddr to frame
  uint32_t* pd = thread_current()->page_dir; 
  bool set_page = pagedir_set_page(pd, pte->vaddr, frame, frame->writable);
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
  assert(pte->bit_set == SWAP_BIT || pte->bit_set == FILE_BIT);
  
 // Allocate user page
  void* frame = frame_allocate(pte, PAL_USER); 
  if (frame == NULL) {
    return false; 
  }
  // Add the page to the current process address space - add mapping
  // from vaddr to frame
  uint32_t* pd = thread_current()->page_dir;
  bool set_page = pagedir_set_page(pd, pte->vaddr, frame, pte->writable);
  if(!set_page) {
    // Page not set properly, so free frame and return false
    frame_free(frame);
    return false;
  }

  // Swap from disk -> memory
  struct swap_slot* ss = (struct swap_slot*)malloc(sizeof(struct swap_slot)); 
  ss->swap_addr = pte->swap_index;  
  swap_load(pte->vaddr,ss->swap_addr); 
  
  pte->loaded = true; 
  
  return true;
}

// load memory mapped files 
bool 
load_mem_map_file(struct page_table_entry* pte) {
  return load_file(pte); 
}

// Inserts file in page table
bool
insert_file(struct file* file, off_t offset, uint8_t *upage,
                             uint32_t read_bytes, uint32_t zero_bytes,
                             bool writable) {
  struct thread* curr = thread_current();
  struct page_table_entry* pte = (struct page_table_entry*)malloc(sizeof(struct page_table_entry));

  if(pte != NULL) {
    pte->bit_set = FILE_BIT;
    pte->vaddr = upage;

    struct file_d* file_d = (struct file_d*)malloc(sizeof(struct file_d));
    file_d->filename = file;
    file_d->file_offset = offset;
    file_d->content_len = file_length(file);
    file_d->read_bytes = read_bytes;
    file_d->zero_bytes = zero_bytes;

    pte->file_d = file_d;
    pte>loaded = false;
    pte->writable = writable;

    hash_elem* h_elem = hash_insert(&curr->sup_page_table_entry, &pte->elem);
    if(h_elem == NULL) {
      return true;
    } else {
      return false;
    }
  }
  return false; 
}


// Inserts memory mapped file in page table
bool 
insert_mem_map_file(struct file* file, off_t offset, uint8_t *upage,
			     uint32_t read_bytes, uint32_t zero_bytes,
			     bool writable) {
  struct thread* curr = thread_current(); 
  struct page_table_entry* pte = (struct page_table_entry*)malloc(sizeof(struct page_table_entry)); 
  
  if(pte != NULL) {
    pte->bit_set = MMAP_BIT; 
    pte->vaddr = upage; 
    
    struct file_d* file_d = (struct file_d*)malloc(sizeof(struct file_d));
    file_d->filename = file;
    file_d->file_offset = offset;
    file_d->content_len = file_length(file);
    file_d->read_bytes = read_bytes; 
    file_d->zero_bytes = zero_bytes; 
    
    pte->file_d = file_d;
    pte>loaded = false; 
    pte->writable = writable;
    
    hash_elem* h_elem = hash_insert(&curr->sup_page_table_entry, &pte->elem);
    if(h_elem == NULL) {
      return true; 
    } else {
      return false;
    }
  } 
  return false; 
}

// Function for stack growth 
bool
grow_stack(void* vaddr) {
  struct thread* curr = thread_current(); 
  struct page_table_entry *pte = (struct page_table_entry*)malloc(sizeof(struct page_table_entry));
  
  if (pte != NULL) {
    void* round_vaddr = pg_round_down(vaddr); 
    pte->vaddr = round_vaddr;
    pte->loaded = true;
    pte->writable = true;
    pte->bit_set = SWAP_BIT;

    uint8_t *frame = frame_allocate(PAL_USER, spte);
   
    if(frame == NULL) {
      free(pte);
      return false; 
    }
    bool set_page = pagedir_set_page(curr->pagedir, round_vaddr, frame, pte->writable);
    if(!set_page) {
      free(pte);
      frame_free(frame);
      return false; 
    }
  } else {
    return false; 
  }
    if(hash_insert(&thread_current()->sup_page_table_entry, &pte->elem == NULL)) {
      return true; 
    }
    return false; 
}

