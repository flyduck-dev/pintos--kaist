#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

void syscall_init (void);
void exit (int status);
int write(int fd, const void *buffer, unsigned size);
void check_address(void *addr);
#endif /* userprog/syscall.h */
