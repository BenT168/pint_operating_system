#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

#include "threads/interrupt.h"
#include "threads/thread.h"
#include "userprog/process.h"
#include "lib/user/syscall.h"

void syscall_init (void);
void syscall_handler (struct intr_frame *f UNUSED);
void syscall_exit (int exit);
int syscall_wait (pid_t pid);
int syscall_write (int fd , const void* buffer, unsigned size);
int syscall_open (const char* file);
int fd_add_file (struct file *file);
struct file* fd_get_file (int fd);

struct lock filesys_lock;

struct fd_file {
  struct file *file;
  int fd;
  struct list_elem elem;
};




#endif /* userprog/syscall.h */
