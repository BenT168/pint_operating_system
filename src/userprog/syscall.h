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




/* Tasks 2 and later. */
void halt (void) NO_RETURN;
void exit (int status) NO_RETURN;
pid_t exec (const char *file);
int wait (pid_t);
bool create (const char *file, unsigned initial_size);
bool remove (const char *file);
int open (const char *file);
int filesize (int fd);
int read (int fd, void *buffer, unsigned length);
int write (int fd, const void *buffer, unsigned length);
void seek (int fd, unsigned position);
unsigned tell (int fd);
void close (int fd);

#endif /* userprog/syscall.h */
