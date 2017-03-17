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
  struct list_elem list_elem;          /* Used to store the frame in the page table. */
  struct lock single_frame_lock;       /* Lock for synchronisation */
};

struct file_d
{
  struct file *filename;               /* File associated with frame */
  int file_offset;                     /* Offset of the file */
  size_t read_bytes;                   /* Bytes to read in file */
  size_t zero_bytes;                   /* Bytes to set to sero in file */
};

void frame_init (void);
void* frame_evict (enum palloc_flags flags);
bool check_pagedir_not_dirty(struct frame* frame, struct page_table_entry* pte);
void* frame_alloc(void * upage, enum palloc_flags flags);
struct frame* frame_get(void *addr);
void frame_free (void * addr);

#endif /* vm/frame.h */
