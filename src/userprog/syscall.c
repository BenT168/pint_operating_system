#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"

static void syscall_handler (struct intr_frame *);

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f UNUSED) 
{
  printf ("system call!\n");
  thread_exit ();
}


static void
syscall_exit (int status)
{
  thread_current()->proc.exit_status = status;
  
  thread_exit (status);
}

static int
syscall_wait (pid_t pid)
{
  return process_wait(pid);
}


static int 
syscall_write (int fd, const void *buffer, unsigned size)
{
  if (fd == STDOUT_FILENO) {
    putbuf(buffer, size);
    return size;
  }
  lock_acquire(&filesys_lock);
  struct file *file = fd_get_file(fd);
  if (file != NULL) {
    int bytes = file_write(file, buffer, size);
	lock_release(&filesys_lock);
	return bytes;
  }
  lock_release(&filesys_lock);
  return -1;
}

static int 
syscall_open (const char *file)
{
  lock_acquire(&filesys_lock);
  struct file *f = filesys_open(file);
  if (f != NULL) {
    int fd = fd_add_file(f);
    lock_release(&filesys_lock);
	return fd; 
  }
  lock_release(&filesys_lock);
  return -1;
}

int fd_add_file (struct file *file)
{
  struct fd_file *fd_file = malloc(sizeof(struct fd_file));
  fd_file->file = file;
  fd_file->fd = thread_current()->fd;
  thread_current()->fd++;
  list_push_back(&thread_current()->file_list, &fd_file->elem);
  return fd_file->fd;
}

struct file* fd_get_file (int fd)
{
  struct thread *t = thread_current();
  struct list_elem *e;

  for (e = list_begin (&t->file_list); e != list_end (&t->file_list);
       e = list_next (e)) {
    struct fd_file *fd_file = list_entry (e, struct fd_file, elem);
      if (fd == fd_file->fd) {
	      return fd_file>file;
	  }
   }
   return NULL;
}

