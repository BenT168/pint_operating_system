#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "threads/malloc.h"
#include "devices/shutdown.h"

static void syscall_handler (struct intr_frame *);
static void syscall_exit (int );
static int syscall_wait (pid_t);
static int syscall_write (int , const void*, unsigned);
static int syscall_open (const char*);


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

/* TASK 2: Terminates the current user program, sending its exit status
   to the kernel. If the process's parent waits for it, this is the status
   that will be returned. Conventionally, a status of 0 indicates success
   and nonzero values indicate errors. */
static void
exit (int status)
{
  thread_current()->exit_status = status;
  printf ("%s: exit(%d)\n", thread_current()->name, status);
  //too many argument using to function : ?? thread_exit(status);
}

/* TASK 2: Waits for a child process pid and retrieves the child's exit status.
   If pid is still alive, waits until it terminates. Then, returns the status
   that pid passed to exit. If pid did not call exit(), but was terminated
   by the kernel (e.g. killed due to an exception), wait(pid) returns -1. */
static int
wait (pid_t pid)
{
  return process_wait(pid);
}

/* TASK 2: Writes size bytes from buffer to the open file fd.
   Returns the number of bytes actually written, which may be less than size
   if some bytes could not be written. */
static int
write (int fd, const void *buffer, unsigned size)
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

/* TASK 2: Opens the file called file.
   Returns a nonnegative integer handle called a "file descriptor" (fd),
   or -1 if the file could not be opened. */
static int
open (const char *file)
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
	      return fd_file->file;
	  }
   }
   return NULL;
}

/* TASK 2: Terminates Pintos by calling shutdown_power_off() */
void halt (void) {
  shutdown_power_off();
}

/* TASK 2: Runs the executable whose name is given in cmd line,
   passing any given arguments, and returns the new process's program id (pid).
   Returns pid -1, which otherwise should not be a valid pid,
   if the program cannot load or run for any reason. */
pid_t exec (const char *cmd_line) {

}
