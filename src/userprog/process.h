#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "threads/thread.h"
#include "threads/synch.h"
#include "vm/page.h"

tid_t process_execute (const char *file_name);
int process_wait (tid_t tid);
void process_exit (void);
void process_activate (void);

/* TASK 3 */
bool install_page ( void *upage, void *kpage, bool writable);

#endif /* userprog/process.h */
