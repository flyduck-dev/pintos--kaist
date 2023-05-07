#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H
#include "threads/synch.h"
#include "stdbool.h"

struct lock filesys_lock;
void syscall_init (void);
void exit (int status);
int write(int fd, const void *buffer, unsigned size);
void check_address(void *addr);
bool create (const char *file, unsigned initial_size);
#endif /* userprog/syscall.h */
