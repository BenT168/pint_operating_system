#ifndef _VM_FRAME_H
#define _VM_FRAME_H

#include <hash.h>
#include <list.h>
#include <stdbool.h>

struct frame
{
  void *addr;                          /* page's physical memory address */
  void *upage;                         /* page's user virtual memory address */
  struct thread *thread;               /* owner thread of page */
  struct file_d *frame_sourcefile;     /* info stored if frame is memory mapped
                                          file */
  bool writable;                       /* boolean checking whether the frame
                                          table is writable */
  struct hash_elem frame_elem;         /* hash elem for all frame list */
  struct list_elem list_elem;
};

struct file_d
{
  struct file *filename;
  int file_offset;
  int content_len;
};

void frame_init (void);
void frame_evict (void);
void * frame_get(void * upage, bool zero, bool writable);
void frame_free (void * addr);



#endif /* vm/frame.h */
