#include "threads/synch.h"
#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

void syscall_init (void);
static struct lock filesys_lock;
#endif /* userprog/syscall.h */