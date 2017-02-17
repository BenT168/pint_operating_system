#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "deviecs/shutdown.h"

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

void
halt (void)
{
  shutdown_power_off ();
}

void
exit (int status)
{
  struct thread *cur = thread_current ();
  uint32_t *pd;

  printf("%s: exit(%d)\n", cur->name, status);
  process_exit ();
}

pid_t
exec (const char *file)
{

}

int
wait (pid_t pid)
{
  if (pid == -1)
  {

  }
}

/* Verifies that the pointer 'vaddr' is a valid user pointer. If it is, we
   return true. If it is not, we return false. */
bool
verify_user_address (uint32_t *pd, const void *vaddr)
{
  /* Asserts that the user pointer is not null; that it is a user address
  (below PHYS_BASE); and that the page in which it exists is mapped to physical
  memory. */

  uint32_t *pd = active_pd ();
  return vaddr != NULL &&
  is_user_vaddr (vaddr) && pagedir_get_page (pd, vaddr) != NULL;
}

/* Reads a byte at user virtual address UADDR.
UADDR must be below PHYS_BASE.
Returns the byte value if successful, -1 if a segfault
occurred. */
static int
get_user (const uint8_t *uaddr)
{
  int result;
  asm ("movl $1f, %0; movzbl %1, %0; 1:"
  : "=&a" (result) : "m" (*uaddr));
return result;
}
/* Writes BYTE to user address UDST.
UDST must be below PHYS_BASE.
Returns true if successful, false if a segfault occurred. */
static bool
put_user (uint8_t *udst, uint8_t byte)
{
int error_code;
asm ("movl $1f, %0; movb %b2, %1; 1:"
: "=&a" (error_code), "=m" (*udst) : "q" (byte));
return error_code != -1;
}
