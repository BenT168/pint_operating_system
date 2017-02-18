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


static void syscall_handler (struct intr_frame *);

static int fd_id = 2;

void
syscall_init (void)
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

/* Tasks 2 : TOCOMMENT */
static void
check_memory_access(const void *ptr) {
  if (!ptr || !is_user_vaddr (ptr) ||
      ! pagedir_get_page (thread_current () ->pagedir, ptr)){
        exit(-1);
      }
}

/* Tasks 2 : TOCOMMENT */
int fd_add_file (struct file *file)
{
  struct fd_file *fd_desc = malloc(sizeof(struct fd_file));
  fd_desc->file = file;
  fd_desc->fd = fd_id++;
  list_push_back(&thread_current()->file_list, &fd_desc->elem);
  return fd_id;
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
   exit(-1);
   return NULL;
}

/* Tasks 2 : TOCOMMENT */
static void
syscall_handler (struct intr_frame *f)
{
  void *esp = f->esp;
  uint32_t *eax = &f->eax;

  check_memory_access (esp);
  int syscall_num = *(int *) esp;

  switch (syscall_num) {
    case SYS_HALT:
      halt();
      break;
    case SYS_EXIT:
      check_memory_access(esp + 4);
      int exit_status = *(int *)(esp + 4);
      exit (exit_status);
      break;
    case SYS_EXEC:
      check_memory_access(esp + 4);
      char *exec_file = *(char **)(esp + 4);
      *eax = exec (exec_file);
      break;
    case SYS_WAIT:
      check_memory_access(esp + 4);
      pid_t pid = *(pid_t *)(esp + 4);
      *eax = wait(pid);
      break;
    case SYS_CREATE:
      check_memory_access(esp + 8);
      const char *create_file = *(const char **)(esp + 4);
      unsigned create_init_size = *(unsigned*) (esp + 8);
      *eax = create (create_file, create_init_size);
      break;
    case SYS_REMOVE:
      check_memory_access(esp + 4);
      const char *remove_file = *(const char **)(esp + 4);
      *eax = remove (remove_file);
      break;
    case SYS_OPEN:
      check_memory_access(esp + 4);
      const char *open_file = *(char **)(esp + 4);
      *eax = open (open_file);
      break;
    case SYS_FILESIZE:
      check_memory_access(esp + 4);
      int filesize_fd = *(int *)(esp + 4);
      *eax = filesize (filesize_fd);
      break;
    case SYS_READ:
      check_memory_access(esp + 12);
      int read_fd = *(int *)(esp + 4);
      void *read_buffer = *(char **)(esp + 8);
      unsigned read_size = *(unsigned *)(esp + 12);
      *eax = read (read_fd, read_buffer, read_size);
      break;
    case SYS_WRITE:
      check_memory_access(esp + 12);
      int write_fd = *(int *)(esp + 4);
      void *write_buffer = *(char **)(esp + 8);
      unsigned write_size = *(unsigned *)(esp + 12);
      *eax = write (write_fd, write_buffer, write_size);
      break;
    case SYS_SEEK:
      check_memory_access(esp + 8);
      int seek_fd = *(int *)(esp + 4);
      unsigned seek_position = *(unsigned *)(esp + 8);
      seek (seek_fd, seek_position);
      break;
    case SYS_TELL:
      check_memory_access(esp + 4);
      int tell_fd = *(int *)(esp + 4);
      *eax = tell (tell_fd);
      break;
    case SYS_CLOSE:
      check_memory_access(esp + 4);
      int close_fd = *(int *)(esp + 4);
      close(close_fd);
      break;
    default:
      printf("System call number %d is invalid.\n", syscall_num);

  }
  /*
  printf ("system call!\n");

  uint32_t *args_pointer = f->esp;
  uint32_t syscall_num = *args_pointer;

  switch (syscall_num) {
    case SYS_HALT:
      halt();
      break;
    case SYS_EXIT:
      exit(*(args_pointer+4));
      break;
    case SYS_EXEC:
      f->eax = exec(*(args_pointer+4));
      break;
    case SYS_WAIT:
      f->eax = wait(*(args_pointer+4));
      break;
    case SYS_CREATE:
      f->eax = create(*(args_pointer+4), *(args_pointer+8));
      break;
    case SYS_REMOVE:
      f->eax = remove(*(args_pointer+4));
      break;
    case SYS_OPEN:
      f->eax = open(*(args_pointer+4));
      break;
    case SYS_FILESIZE:
      f->eax = filesize(*(args_pointer+4));
      break;
    case SYS_READ:
      f->eax = read(*(args_pointer+4), *(args_pointer+8), *(args_pointer+12));
      break;
    case SYS_WRITE:
      f->eax = write(*(args_pointer+4), *(args_pointer+8), *(args_pointer+12));
      break;
    case SYS_SEEK:
      seek(*(args_pointer+4), *(args_pointer+8));
      break;
    case SYS_TELL:
      f->eax = tell(*(args_pointer+4));
      break;
    case SYS_CLOSE:
      close(*(args_pointer+4));
      break;
    default:
      printf("System call not available.\n");
      break;
  }
  thread_exit ();
  */
}

/* TASK 2: Terminates Pintos by calling shutdown_power_off() */
void
halt (void) {
  shutdown_power_off();
}

/* TASK 2: Terminates the current user program, sending its exit status
   to the kernel. If the process's parent waits for it, this is the status
   that will be returned. Conventionally, a status of 0 indicates success
   and nonzero values indicate errors. */
void
exit (int status)
{
  struct thread *current = thread_current ();

  if (current -> parent) {
    // Remove process from the list_elem
    struct pid_exit_status *pid_exit_stat = malloc(sizeof(struct pid_exit_status));
    pid_exit_stat->pid = current->pid;
    pid_exit_stat->exit_status = status;
    list_push_back (&current->parent->pid_to_exit_status,
      &pid_exit_stat->elem);
  }

  printf ("%s: exit(%d)\n", thread_current()->name, status);
  thread_exit();
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

  if(proc_thread) { // a valid pointer
    sema_down(&proc_thread->load_sema);
  }

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

/* Tasks 2 : Creates a new file called file initially initial size bytes in
   size. Returns true if successful, false otherwise. */
bool
create (const char *file, unsigned initial_size) {
  check_memory_access (file);
  bool check_create = filesys_create(file, initial_size);
  return check_create;
}

/* Tasks 2 : Deletes the file called file. Returns true if successful, false
   otherwise. */
bool
remove (const char *file) {
  check_memory_access (file);
  bool check_remove = filesys_remove(file);
  return check_remove;
}

/* TASK 2: Opens the file called file.
   Returns a nonnegative integer handle called a "file descriptor" (fd),
   or -1 if the file could not be opened. */
int
open (const char *file)
{
  if (!file) return -1;

  check_memory_access (file);
  struct file *f = filesys_open(file);
  /* Create fd_file struct to be added to thread's file_descriptors */
  struct fd_file *fd_f = malloc(sizeof(struct fd_file));

  if (!f) return -1;
  if (fd_f == NULL) {
    file_close(f);
    return -1;
  }

  fd_f->file = f;
  fd_f->fd = fd_id++;
  list_push_back(&thread_current()->file_descriptors, &fd_f->elem);
  return fd_id;
}

/* Tasks 2 : Returns the size, in bytes, of the file open as fd.
 */
int
filesize (int fd) {
  struct file* file = fd_get_file(fd);
  return file_length(file);
}

/* Tasks 2 : Reads size bytes from the file open as fd into buffer. Returns the
   number of bytes actually read (0 at end of file), or -1 if the file could not
   be read (due to a condition other than end of file). */
int
read (int fd, void *buffer, unsigned size) {
  check_memory_access(buffer);
  char *buf = buffer;

  switch (fd) {
    case 0 :
      {
        unsigned i;
        for(i = 0; i < size; i++){
          *(buf + i) = input_getc ();
        }
        return size;
      }
      break;
    case 1 :
      exit(-1);
      break;
    default :
      return file_read(fd_get_file(fd), buffer, size);
      break;
  }
  return -1;
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
      exit(-1);
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

/* Tasks 2 : Changes the next byte to be read or written in open file fd to
   position, expressed in bytes from the beginning of the file. */
void
seek (int fd, unsigned position) {
  struct file* file = fd_get_file(fd);
  return file_seek(file, position);
}

/* Tasks 2 : Returns the position of the next byte to be read or written in open
   file fd, expressed in bytes from the beginning of the file. */
unsigned
tell (int fd) {
  struct file* file = fd_get_file(fd);
  return file_tell(file);
}

/* Tasks 2 : Closes file descriptor fd. Exiting or terminating a process
   implicitly closes all its open file descriptors, as if by calling this
   function for each one.
 */
void
close (int fd) {
  struct file* file = fd_get_file(fd);
  file_close(file);
}
