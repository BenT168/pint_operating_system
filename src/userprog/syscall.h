#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

#include "threads/interrupt.h"
#include "threads/thread.h"
#include "userprog/process.h"
#include "lib/user/syscall.h"

void syscall_init (void);
int fd_add_file (struct file *file);
void halt (void);
pid_t exec (const char *cmd_line);

struct file* fd_get_file (int fd);

struct lock filesys_lock;

struct fd_file {
  struct file *file;
  int fd;
  struct list_elem elem;
};




#endif /* userprog/syscall.h */
