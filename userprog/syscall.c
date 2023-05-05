#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include <list.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/loader.h"
#include "threads/palloc.h"
#include "threads/flags.h"
#include "threads/vaddr.h"
#include "threads/synch.h"
#include "userprog/gdt.h"
#include "userprog/process.h"
#include "filesys/filesys.h"
#include "filesys/file.h"
#include "intrinsic.h"
#include "threads/init.h"

void syscall_entry (void);
void syscall_handler (struct intr_frame *);
// 제작 함수
// void check_address(void *addr);
// struct page * check_address(void *addr); 
// int add_file_to_fdt(struct file *file);
// void remove_file_from_fdt(int fd);
// static struct file *find_file_by_fd(int fd);

typedef int pid_t;

// void halt(void);
 void exit(int status);
// tid_t fork(const char *thread_name, struct intr_frame *f);
// pid_t exec (const char *cmd_lime);
int wait (pid_t pid); 
// bool create(const char *file, unsigned initial_size);
// bool remove(const char *file);
// int open(const char *file);
// int filesize(int fd);
// int read(int fd, void *buffer, unsigned size);
 int write(int fd, const void *buffer, unsigned size);
// void seek(int fd, unsigned position);
// unsigned tell(int fd);
// void close(int fd);
/* System call.
 *
 * Previously system call services was handled by the interrupt handler
 * (e.g. int 0x80 in linux). However, in x86-64, the manufacturer supplies
 * efficient path for requesting the system call, the `syscall` instruction.
 *
 * The syscall instruction works by reading the values from the the Model
 * Specific Register (MSR). For the details, see the manual. */

#define MSR_STAR 0xc0000081         /* Segment selector msr */
#define MSR_LSTAR 0xc0000082        /* Long mode SYSCALL target */
#define MSR_SYSCALL_MASK 0xc0000084 /* Mask for the eflags */

void
syscall_init (void) {
	write_msr(MSR_STAR, ((uint64_t)SEL_UCSEG - 0x10) << 48  |
			((uint64_t)SEL_KCSEG) << 32);
	write_msr(MSR_LSTAR, (uint64_t) syscall_entry);

	/* The interrupt service rountine should not serve any interrupts
	 * until the syscall_entry swaps the userland stack to the kernel
	 * mode stack. Therefore, we masked the FLAG_FL. */
	write_msr(MSR_SYSCALL_MASK,
			FLAG_IF | FLAG_TF | FLAG_DF | FLAG_IOPL | FLAG_AC | FLAG_NT);
}

void check_address(void *addr)
{
    // kernel VM 못가게, 할당된 page가 존재하도록(빈공간접근 못하게)
    struct thread *cur = thread_current();
    if (is_kernel_vaddr(addr) || pml4_get_page(cur->pml4, addr) == NULL)
    {
        exit(-1);
    }
}
void
syscall_handler (struct intr_frame *f UNUSED) {
	/* Get the system call number. */
	int syscall_number = (int) f->R.rax;
	//printf("syscall num : \n\n\n%d\n\n\n", syscall_number);
	// TODO: Your implementation goes here.
	    // 현재 실행 중인 쓰레드의 상태를 커널 스택에 저장
    //push(thread_current()->kernel_esp);
    switch (syscall_number){
		case SYS_HALT: // 0 개
			power_off();
      		break;
    	case SYS_EXIT: // 1개
			exit(f->R.rdi);
			NOT_REACHED();
      		break;
		case SYS_FORK: // 1개
      		break;
    	case SYS_EXEC: // 1개
      		break;
    	case SYS_WAIT: // 1개
			f->R.rax = process_wait(f->R.rdi);
      		break;
    	case SYS_CREATE: // 2개
      		break;
    	case SYS_REMOVE: // 1개
      		break; 
    	case SYS_OPEN: // 1개
      		break;
    	case SYS_FILESIZE: // 1개
      		break;
    	case SYS_READ: // 3개
      		break;
    	case SYS_WRITE: // 3개
			f->R.rax = write(f->R.rdi, f->R.rsi, f->R.rdx);
			//write((int)*(uint32_t *)(f->esp+4), (void *)*(uint32_t *)(f->esp + 8), (unsigned)*((uint32_t *)(f->esp + 12)));
      		break;
    	case SYS_SEEK: // 2개
      		break;
    	case SYS_TELL: // 1개
      		break;
    	case SYS_CLOSE: // 1개
      		break;
		// case SYS_CREATE: 
		// 	//check_address(f->R.rdi);
		// 	//f->R.rax = create(f->R.rdi, f->R.rsi);
		// 	break;
		// case SYS_EXIT:
		// 	exit(f->R.rdi);
		// 	break;
		// case SYS_HALT:
		// 	//halt();
		// 	break;
		// case SYS_WRITE:                  /* Write to a file. */
        //     if(f->R.rdi == 1){
        //         printf("%s", f->R.rsi);
        //     }
        //    break;
        default:
            printf("system call!\n");
            thread_exit();
    }
}

void exit (int status) {
	struct thread *cur = thread_current();
	cur->exit_status = status;
	printf("%s: exit(%d)\n", thread_name(), status);
  	thread_exit ();
}

int write (int fd, const void *buffer, unsigned size) {
	check_address(buffer);
  	if (fd == 1) {
    putbuf(buffer, size);
    return size;
  }
  return -1; 
}