#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

#include "threads/interrupt.h"
#include "threads/thread.h"
#include "userprog/process.h"
#include "lib/user/syscall.h"
#include "vm/frame.h"

/* Tasks 2 and later. */
void syscall_init (void);

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

/* TASK 3 */
mapid_t mmap(int fd, void* addr);
void munmap(mapid_t mapping);
void delete_mmap_entry(struct vm_mmap *mmap);

#endif /* userprog/syscall.h */
