#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

#include "threads/interrupt.h"
#include "threads/thread.h"
#include "userprog/process.h"
#include "lib/user/syscall.h"

void syscall_init (void);

/* Tasks 2 : TOCOMMENT */
struct lock filesys_lock;

/* Tasks 2 : TOCOMMENT */
struct fd_file {
  struct file *file;
  int fd;
  struct list_elem elem;
};

/* Tasks 2 and later. */
int fd_add_file (struct file *file);
struct file* fd_get_file (int fd);
void halt (void);
void exit (int status);
pid_t exec (const char *file);
int wait (pid_t);
bool create (const char *file, unsigned initial_size);
bool remove (const char *file);
int open (const char *file);
int filesize (int fd);
int read (int fd, void *buffer, unsigned size);
int write (int fd, const void *buffer, unsigned size);
void seek (int fd, unsigned position);
unsigned tell (int fd);
void close (int fd);


#endif /* userprog/syscall.h */
