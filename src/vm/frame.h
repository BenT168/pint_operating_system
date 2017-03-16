#ifndef _VM_FRAME_H
#define _VM_FRAME_H

#include <hash.h>
#include <list.h>
#include <stdbool.h>
#include "threads/palloc.h"
#include "vm/page.h"

struct frame
{
  void *addr;                          /* page's physical memory address */
  void *upage;                         /* page's user virtual memory address */
  struct thread *thread;               /* owner thread of page */
  struct file_d *frame_sourcefile;     /* info stored if frame is memory mapped
                                          file */
  bool writable;                       /* boolean checking whether the frame
                                          table is writable */
  struct list_elem list_elem;
};

struct file_d
{
  struct file *filename;
  int file_offset;
  size_t read_bytes;
  size_t zero_bytes;
};

void frame_init (void);
void* frame_evict (enum palloc_flags flags);
void check_pagedir_accessed(struct frame* frame, bool pinned);
bool check_pagedir_dirty(struct frame* frame, struct page_table_entry* pte);
void* frame_alloc(void * upage, enum palloc_flags flags);
struct frame* frame_get(void *addr);
void frame_free (void * addr);

#endif /* vm/frame.h */
