#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

void syscall_init (void);
static void syscall_exit (int status);
struct file* fd_get_file (int fd);

struct lock filesys_lock;

struct fd_file {
  struct file *file;
  int fd;
  struct list_elem elem;
};




#endif /* userprog/syscall.h */
