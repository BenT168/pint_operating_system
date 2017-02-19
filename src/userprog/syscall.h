#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

#include "threads/interrupt.h"
#include "threads/thread.h"
#include "userprog/process.h"
#include "lib/user/syscall.h"

void syscall_init (void);

void     sys_halt (void);
int      sys_exit (int status);
pid_t    sys_exec (const char *file);
int      sys_wait (pid_t);
bool     sys_create (const char *file, unsigned initial_size);
bool     sys_remove (const char *file);
int      sys_open (const char *file);
int      sys_filesize (int fd);
int      sys_read (int fd, void *buffer, unsigned size);
int      sys_write (int fd, const void *buffer, unsigned size);
void     sys_seek (int fd, unsigned position);
unsigned sys_tell (int fd);
void     sys_close (int fd);

/* Tasks 2 : TOCOMMENT */
struct fd_file {
  struct file *file;
  int fd;
  struct list_elem elem;
};

/* Tasks 2 and later. */
int fd_add_file (struct file *file);
struct file* fd_get_file (int fd);

#endif /* userprog/syscall.h */
