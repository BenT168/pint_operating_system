#include "userprog/syscall.h"
#include "userprog/pagedir.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/malloc.h"
#include "threads/vaddr.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "devices/shutdown.h"
#include "devices/input.h"

#define MAX_NUM_SYSCALLS 322
#define STDOUT_FILENO 1
#define STDIN_FILENO 0
#define MAX_BUFFER_LENGTH 512

static void syscall_handler (struct intr_frame *);

/* A 'syscall_dispatcher' type is a generic function pointer. It is used to
   call the appropriate system call function. A system call can have a
   maximum of 3 arguments. */
typedef int (*syscall_dispatcher) (intptr_t, intptr_t, intptr_t);

/* Each system call is associated with a unique system call number, which in
   turn is associated with a unique function implementing that system call.
   Leveraging this bijection, the system call map is a mapping from
   system call numbers to the functions that implement the corresponding system
   call. */
static syscall_dispatcher syscall_map[MAX_NUM_SYSCALLS];

static struct lock filesys_lock;

static void
verify_user_addr (const void *uaddr)
{
  struct thread *cur = thread_current ();

  if (!uaddr || !is_user_vaddr (uaddr) ||
      !pagedir_get_page (cur->pagedir, uaddr))
  {
    sys_exit (-1);
  }
}

static void
acquire_filesys_lock (void)
{
  lock_acquire (&filesys_lock);
}

static void
release_filesys_lock (void)
{
  lock_release (&filesys_lock);
}

void
syscall_init (void)
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");

  /* We need casts in order to preserve the actual argument and return types of
     the system call procedures. */
  syscall_map[SYS_HALT]     = (syscall_dispatcher) sys_halt;
  syscall_map[SYS_EXIT]     = (syscall_dispatcher) sys_exit;
  syscall_map[SYS_EXEC]     = (syscall_dispatcher) sys_exec;
  syscall_map[SYS_WAIT]     = (syscall_dispatcher) sys_wait;
  syscall_map[SYS_CREATE]   = (syscall_dispatcher) sys_create;
  syscall_map[SYS_REMOVE]   = (syscall_dispatcher) sys_remove;
  syscall_map[SYS_OPEN]     = (syscall_dispatcher) sys_open;
  syscall_map[SYS_FILESIZE] = (syscall_dispatcher) sys_filesize;
  syscall_map[SYS_READ]     = (syscall_dispatcher) sys_read;
  syscall_map[SYS_WRITE]    = (syscall_dispatcher) sys_write;
  syscall_map[SYS_SEEK]     = (syscall_dispatcher) sys_seek;
  syscall_map[SYS_TELL]     = (syscall_dispatcher) sys_tell;
  syscall_map[SYS_CLOSE]    = (syscall_dispatcher) sys_close;

  /* File system code is regarded as a critical section. */
  lock_init (&filesys_lock);
}

/* Tasks 2 : TOCOMMENT */
static void
syscall_handler (struct intr_frame *f)
{
  syscall_dispatcher syscall_procedure;
  int syscall_num;
  int syscall_ret_val;

  int* stack_ptr = f->esp;
  syscall_num = *stack_ptr;

  syscall_procedure = syscall_map[syscall_num];
  syscall_ret_val = syscall_procedure (*(stack_ptr + 1),
                                       *(stack_ptr + 2),
                                       *(stack_ptr + 3));
  f->eax = syscall_ret_val;
}

void
sys_halt (void)
{
  shutdown_power_off ();
}

int
sys_exit (int status)
{
  struct thread *cur = thread_current ();

  cur->exit_status = status;

  printf ("%s: exit(%d)\n", cur->name, status);

  thread_exit ();
  return status;
}

// TODO
pid_t
sys_exec (const char *file)
{
  verify_user_addr ((void *) file);

  acquire_filesys_lock ();

  tid_t tid = process_execute (file);

  release_filesys_lock ();

  return tid;
}

int
sys_wait (pid_t pid)
{
  return process_wait (pid);
}

bool
sys_create (const char *file, unsigned initial_size)
{
  verify_user_addr ((void *) file);

  acquire_filesys_lock ();
  bool success = filesys_create (file, initial_size);
  release_filesys_lock ();

  return success;
}

bool
sys_remove (const char *file)
{
  verify_user_addr ((void *) file);

  acquire_filesys_lock ();
  bool success = filesys_remove (file);
  release_filesys_lock ();
  return success;
}

int
sys_open (const char *file)
{
  verify_user_addr ((void *) file);

  acquire_filesys_lock ();
  struct file *file_ptr = filesys_open (file);
  release_filesys_lock ();

  int fd;
  if (!file_ptr)
    fd = 1;
  else
    fd = thread_add_new_file (file_ptr);

  return fd;
}

int
sys_filesize (int fd)
{
  struct thread *cur = thread_current ();
  struct file_handle *handle = thread_get_file_handle (&cur->file_list, fd);

  if (handle)
    return file_length (handle->file);

  // File with given file descriptor not found. Exit with status -1.
  return sys_exit (-1);
}

int
sys_read (int fd, void *buffer, unsigned size)
{
  struct thread *cur = thread_current ();
  int bytes_read = 0;

  // Verify buffer points to valid user address.
  verify_user_addr ((void *) buffer);

  if (fd == STDOUT_FILENO)
  {
    // Attempt to read from standard output.
    bytes_read = -1;
    return sys_exit (bytes_read);
  }
  else if (fd == STDIN_FILENO)
  {
    acquire_filesys_lock ();

    uint8_t *u8t_buffer = (uint8_t *) buffer;
    for (unsigned i = 0; i < size; i++, bytes_read++)
    {
      // Read one byte at a time.
      u8t_buffer[i] = input_getc ();
    }

    release_filesys_lock ();
  }
  else
  {
    // Get file handle if it exists.
    struct file_handle *handle = thread_get_file_handle (&cur->file_list, fd);

    if (!handle)
    {
      bytes_read = -1;
      return sys_exit (bytes_read);
    }

    acquire_filesys_lock ();
    bytes_read = file_read (handle->file, buffer, size);
    release_filesys_lock ();
  }
  return bytes_read;
}

int
sys_write (int fd, const void *buffer, unsigned size)
{
  struct thread *cur = thread_current ();
  int bytes_written = 0;

  verify_user_addr ((void *) buffer);

  if (fd == STDIN_FILENO)
  {
    // Attempt to write to standard input.
    bytes_written = -1;
    return sys_exit (bytes_written);
  }
  else if (fd == STDOUT_FILENO)
  {
    acquire_filesys_lock ();

    while (size > MAX_BUFFER_LENGTH)
    {
      putbuf ((char *) (buffer + bytes_written), MAX_BUFFER_LENGTH);
      bytes_written += MAX_BUFFER_LENGTH;
      size -= MAX_BUFFER_LENGTH;
    }
    putbuf ((char *) buffer, size);
    bytes_written += size;

    release_filesys_lock ();
  }
  else
  {
    // Get file handle if it exists.
    struct file_handle *handle = thread_get_file_handle (&cur->file_list, fd);

    if (!handle)
    {
      bytes_written = -1;
      return sys_exit (bytes_written);
    }

    acquire_filesys_lock ();
    bytes_written = file_write (handle->file, buffer, size);
    release_filesys_lock ();
  }
  return bytes_written;
}

void
sys_seek (int fd, unsigned position)
{

}

unsigned
sys_tell (int fd)
{

}

void
sys_close (int fd)
{

}

/* Tasks 2 : TOCOMMENT */
static void
check_memory_access(const void *ptr) {
  if (!ptr || !is_user_vaddr (ptr) ||
      ! pagedir_get_page (thread_current () ->pagedir, ptr)){
        sys_exit(-1);
      }
}

/* Tasks 2 : TOCOMMENT */
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
   sys_exit(-1);
   return NULL;
}

/* TASK 2: Writes size bytes from buffer to the open file fd.
   Returns the number of bytes actually written, which may be less than size
   if some bytes could not be written. */
int
write (int fd, const void *buffer, unsigned size)
{
  check_memory_access (buffer);

  switch (fd) {
    case 0 :
      sys_exit(-1);
      break;
    case 1 :
      putbuf (buffer, size);
      return size;
      break;
    default :
      return file_write(fd_get_file(fd), buffer, size);
      break;
  }
  return -1;
}
