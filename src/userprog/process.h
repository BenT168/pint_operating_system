#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include <list.h>
#include <hash.h>
#include "threads/thread.h"
#include "threads/synch.h"

struct process
  {
    tid_t tid;
    char name[15];
    struct file *file;
    bool child_load_success;
    struct semaphore load_sema;
    struct semaphore alive_sema;

    struct thread *parent;
    
    struct list child_procs;
    struct list file_descriptors;
    struct list pid_to_exit_status;
    struct list_elem elem;
  };

tid_t process_execute (const char *file_name);
int process_wait (tid_t);
void process_exit (void);
void process_activate (void);

#endif /* userprog/process.h */
