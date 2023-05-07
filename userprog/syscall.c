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
#include "threads/flags.h"
#include "intrinsic.h"
#include "filesys/filesys.h"
#include "filesys/file.h"

void syscall_entry (void);
void syscall_handler (struct intr_frame *);

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

/* The main system call interface */
void
syscall_handler (struct intr_frame *f UNUSED) {
	int syscall_number = (int) f->R.rax;
	switch (syscall_number){
		case SYS_HALT: // 0 개
			power_off();
			NOT_REACHED();
      		break;
    	case SYS_EXIT: // 1개
			exit(f->R.rdi);
			NOT_REACHED();
      		break;
		case SYS_FORK: // 1개
            //f->R.rax = fork(f->R.rdi, f);
      		break;
    	case SYS_EXEC: // 1개
			//exec(f->R.rdi);
      		//if (exec(f->R.rdi) == -1) {
			//	exit(-1);
			//}
            break;
    	case SYS_WAIT: // 1개
			f->R.rax = process_wait(f->R.rdi);
      		break;
    	case SYS_CREATE: // 2개
			//f->R.rax = create(f->R.rdi, f->R.rsi);
      		break;
    	case SYS_REMOVE: // 1개
			//f->R.rax = remove(f->R.rdi);
      		break; 
    	case SYS_OPEN: // 1개
			//f->R.rax = open(f->R.rdi);
      		break;
    	case SYS_FILESIZE: // 1개
			//f->R.rax = filesize(f->R.rdi);
      		break;
    	case SYS_READ: // 3개
			//f->R.rax = read(f->R.rdi, f->R.rsi, f->R.rdx);
      		break;
    	case SYS_WRITE: // 3개
			f->R.rax = write(f->R.rdi, f->R.rsi, f->R.rdx);
      		break;
    	case SYS_SEEK: // 2개
			//seek(f->R.rdi, f->R.rsi);
      		break;
    	case SYS_TELL: // 1개
			//f->R.rax = tell(f->R.rdi);
      		break;
    	case SYS_CLOSE: // 1개
			//close(f->R.rdi);
      		break;
        default:
            //exit(-1);
			break;
    }
}

void exit (int status) {
	struct thread *cur = thread_current();
	printf("%s: exit(%d)\n", thread_name(), status);
  	thread_exit ();
}

int write(int fd, const void *buffer, unsigned size) {
	check_address(buffer);
	if (fd == 0) return -1;
	if (fd == 1) {
		putbuf(buffer, size);  // 표준출력을 처리하는 함수 putbuf()
		return size;
	} 
}

void check_address(void *addr)
{
	// kernel VM 못가게, 할당된 page가 존재하도록(빈공간접근 못하게)
	struct thread *cur = thread_current();
	if (addr == NULL || !is_user_vaddr(addr) || pml4_get_page(cur->pml4, addr) == NULL)
	{
		exit(-1);
	}
}
