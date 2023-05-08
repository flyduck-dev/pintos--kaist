#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H
#include "threads/synch.h"
#include "stdbool.h"
#include "filesys/filesys.h"
#include "userprog/syscall.h"

struct lock filesys_lock;
void syscall_init (void);
void exit (int status);
void check_address(void *addr);
bool create (const char *file, unsigned initial_size);
struct file *process_get_file(int fd);
int process_add_file (struct file *f);
void remove_file_from_fdt(int fd);
int open (const char *file);
int read (int fd, void* buffer, unsigned size);
int write(int fd, const void *buffer, unsigned size);
int exec(const char *file_name);
#endif /* userprog/syscall.h */
