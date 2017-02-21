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
#define MAX_SYSCALL_ARGS 3
#define FILE_OPEN_FAILURE -1

static void check_memory_access(const void *);
static void acquire_filelock (void);
static void release_filelock (void);
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

static struct lock filelock;

/* TASK 2: Checks that the pointer is legal:
     - checks that it is not a null pointer;
     - checks that it is pointing to a user virtual address and not kernel;
     - checks that it is not unmapped;
   If any of these conditions is false it exits with code -1. */
static void
check_memory_access(const void *ptr) {
  if (ptr == NULL || !is_user_vaddr (ptr) ||
    pagedir_get_page (thread_current () ->pagedir, ptr) == NULL) {
      exit(-1);
  }
}

/* TASK 2: Acquires lock over file system */
static void
acquire_filelock (void)
{
  lock_acquire (&filelock);
}

/* TASK 2: Releases lock from file system */
static void
release_filelock (void)
{
  lock_release (&filelock);
}

/* TASK 2: TOCOMMENT */
void
syscall_init (void)
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");

  /* We need casts in order to preserve the actual argument and return types of
     the system call procedures. */
  syscall_map[SYS_HALT]     = (syscall_dispatcher) halt;
  syscall_map[SYS_EXIT]     = (syscall_dispatcher) exit;
  syscall_map[SYS_EXEC]     = (syscall_dispatcher) exec;
  syscall_map[SYS_WAIT]     = (syscall_dispatcher) wait;
  syscall_map[SYS_CREATE]   = (syscall_dispatcher) create;
  syscall_map[SYS_REMOVE]   = (syscall_dispatcher) remove;
  syscall_map[SYS_OPEN]     = (syscall_dispatcher) open;
  syscall_map[SYS_FILESIZE] = (syscall_dispatcher) filesize;
  syscall_map[SYS_READ]     = (syscall_dispatcher) read;
  syscall_map[SYS_WRITE]    = (syscall_dispatcher) write;
  syscall_map[SYS_SEEK]     = (syscall_dispatcher) seek;
  syscall_map[SYS_TELL]     = (syscall_dispatcher) tell;
  syscall_map[SYS_CLOSE]    = (syscall_dispatcher) close;

  /* File system code is regarded as a critical section. */
  lock_init (&filelock);
}

/* TASK 2: This function parses the input system call code and redirects
   to the relevant system call function. */
static void
syscall_handler (struct intr_frame *f)
{
  check_memory_access (f->esp);
  /* arguments to system calls must be in user address space. */
  check_memory_access (f->esp + 4 * MAX_SYSCALL_ARGS);

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

/* TASK 2: Terminates Pintos by calling shutdown_power_off() */
void
halt (void)
{
  shutdown_power_off ();
}

/* TASK 2: Terminates the current user program, sending its exit status
   to the kernel. If the process's parent waits for it, this is the status
   that will be returned. Conventionally, a status of 0 indicates success
   and nonzero values indicate errors. */
void
exit (int status)
{
  struct thread *cur = thread_current ();

  /* Send the information about the child's exit status to the parent ( if
    one exists) before this thread dies */
  if(cur->parent != NULL) {
    struct pid_exit_status* pid_exit_status = malloc(sizeof(struct pid_exit_status));
    pid_exit_status->pid = cur->pid;
    pid_exit_status->exit_status = status;
    list_push_back(&cur->parent->pid_to_exit_status, &pid_exit_status->elem);
  }
  cur->exit_status = status;

  char *save_ptr;
  char *proper_thread_name = (char *) strtok_r (cur->name, " ", &save_ptr);

  printf ("%s: exit(%d)\n", proper_thread_name, status);

  thread_exit ();
}


/* TASK 2: Runs the executable whose name is given in cmd line,
   passing any given arguments, and returns the new process's program id (pid).
   Returns pid -1, which otherwise should not be a valid pid,
   if the program cannot load or run for any reason. */
pid_t
exec (const char *cmd_line)
{
  check_memory_access(cmd_line);
  tid_t thread_id = process_execute(cmd_line);
  pid_t process_id = (pid_t) thread_id;

  if (thread_id == TID_ERROR) return -1;

  struct thread* proc_thread = get_tid_thread(thread_id);

  if(proc_thread) {
    sema_down(&proc_thread->load_sema);
  }

  /* Return -1 if the program cannot load or run */
  if (!thread_current ()->child_load_success) return -1;

  return process_id;
}

/* TASK 2: Waits for a child process pid and retrieves the child's exit status.
   If pid is still alive, waits until it terminates. Then, returns the status
   that pid passed to exit. If pid did not call exit(), but was terminated
   by the kernel (e.g. killed due to an exception), wait(pid) returns -1. */
int
wait (pid_t pid)
{
  if (pid == -1) return -1;
  tid_t thread_id = (tid_t) pid;
  return process_wait(thread_id);
}

/* TASK 2: Creates a new file called file initially initial size bytes in
   size. Returns true if successful, false otherwise. */
bool
create (const char *file, unsigned initial_size)
{
  check_memory_access (file);
  acquire_filelock ();
  bool success = filesys_create (file, initial_size);
  release_filelock ();
  return success;
}

/* TASK 2: Deletes the file called file. Returns true if successful, false
   otherwise. */
bool
remove (const char *file)
{
  check_memory_access(file);
  acquire_filelock ();
  bool success = filesys_remove (file);
  release_filelock ();
  return success;
}

/* TASK 2: Opens the file called file.
   Returns a nonnegative integer handle called a "file descriptor" (fd),
   or -1 if the file could not be opened. */
int
open (const char *file)
{
  check_memory_access ((void *) file);

  acquire_filelock ();
  struct file *file_ptr = filesys_open (file);
  release_filelock ();

  int fd;
  if (!file_ptr)
    fd = FILE_OPEN_FAILURE;
  else
    fd = thread_add_new_file (file_ptr);

  return fd;
}

/* TASK 2: Returns the size, in bytes, of the file open as fd.
 */
int
filesize (int fd)
{
  struct thread *cur = thread_current ();
  struct file_handle *handle = thread_get_file_handle (&cur->file_list, fd);

  if (handle) return file_length (handle->file);

  /* File with given file descriptor not found. Exit with status -1. */
  exit (-1);
}

/* TASK 2: Reads size bytes from the file open as fd into buffer. Returns the
   number of bytes actually read (0 at end of file), or -1 if the file could not
   be read (due to a condition other than end of file). */
int
read (int fd, void *buffer, unsigned size)
{
  struct thread *cur = thread_current ();
  int bytes_read = 0;

  /* Verify buffer points to valid user address. */
  check_memory_access ((void *) buffer);

  if (fd == STDOUT_FILENO)
  {
    /* Attempt to read from standard output. */
    bytes_read = -1;
    exit (bytes_read);
  }
  else if (fd == STDIN_FILENO)
  {
    acquire_filelock ();

    uint8_t *u8t_buffer = (uint8_t *) buffer;
    for (unsigned i = 0; i < size; i++, bytes_read++)
    {
      /* Read one byte at a time. */
      u8t_buffer[i] = input_getc ();
    }

    release_filelock ();
  }
  else
  {
    acquire_filelock ();
    /* Get file handle if it exists. */
    struct file_handle *handle = thread_get_file_handle (&cur->file_list, fd);

    if (!handle)
    {
      bytes_read = -1;
      exit (bytes_read);
    }

    int l = file_length(handle->file);
    bytes_read = file_read (handle->file, buffer, size);
    int i_size = size;
    if (bytes_read == i_size) {
					release_filelock ();
					return bytes_read;
				} else if (bytes_read == l && bytes_read != i_size) {
					release_filelock ();;
					return 0;
				} else {
					release_filelock ();;
					exit(-1);
					return -1;
				}
  }
  return bytes_read;
}

/* TASK 2: Writes size bytes from buffer to the open file fd.
   Returns the number of bytes actually written, which may be less than size
   if some bytes could not be written. */
int
write (int fd, const void *buffer, unsigned size)
{
  struct thread *cur = thread_current ();
  int bytes_written = 0;

  check_memory_access ((void *) buffer);

  if (fd == STDIN_FILENO)
  {
    /* Attempt to write to standard input. */
    bytes_written = -1;
    exit (bytes_written);
  }
  else if (fd == STDOUT_FILENO)
  {
    acquire_filelock ();

    if (size < 200) {
      putbuf(buffer, size);
      bytes_written = size;
    } else {
      while (size > MAX_BUFFER_LENGTH)
      {
        putbuf ((char *) (buffer + bytes_written), MAX_BUFFER_LENGTH);
        bytes_written += MAX_BUFFER_LENGTH;
        size -= MAX_BUFFER_LENGTH;
      }
      putbuf ((char *) (buffer + bytes_written), size);
      bytes_written += size;
    }
    release_filelock ();
  }
  else
  {
    /* Get file handle if it exists. */
    struct file_handle *handle = thread_get_file_handle (&cur->file_list, fd);

    if (!handle)
    {
      bytes_written = -1;
      exit (bytes_written);
    }

    acquire_filelock ();
    bytes_written = file_write (handle->file, buffer, size);
    release_filelock ();
  }
  return bytes_written;
}

/* TASK 2: Changes the next byte to be read or written in open file fd to
   position, expressed in bytes from the beginning of the file. */
void
seek (int fd, unsigned position)
{
  struct thread *cur = thread_current ();
  struct file_handle* fd_file = thread_get_file_handle(&cur->file_list, fd);

  acquire_filelock ();
  file_seek(fd_file->file, position);
  release_filelock ();
}

/* TASK 2: Returns the position of the next byte to be read or written in open
   file fd, expressed in bytes from the beginning of the file. */
unsigned
tell (int fd)
{
  struct thread *cur = thread_current ();
  struct file_handle* fd_file = thread_get_file_handle(&cur->file_list, fd);

  acquire_filelock ();
  unsigned sys_tell = file_tell(fd_file->file);
  release_filelock ();

  return sys_tell;
}

/* TASK 2: Closes file descriptor fd. Exiting or terminating a process
   implicitly closes all its open file descriptors, as if by calling this
   function for each one.
 */
void
close (int fd)
{
  struct thread *cur = thread_current ();
  struct file_handle* fd_file = thread_get_file_handle(&cur->file_list, fd);
  if(fd_file == NULL) exit(-1);

  acquire_filelock ();
  list_remove(&fd_file->elem); /* Removes file for thread's list of files */
  file_close(fd_file->file);   /* closes the file */
  free(fd_file);               /* frees memory calloced for struct sys_file */
  release_filelock ();
}
