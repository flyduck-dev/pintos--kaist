#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/loader.h"
#include "userprog/gdt.h"
#include "threads/flags.h"
#include "intrinsic.h"

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
//유저 스택에 접근하기 위해서는, if_->rsp를 사용합니다. rsp는 "stack pointer"의 약자로, 현재 스택 프레임의 맨 위 주소를 가리키는 레지스터입니다. if_->rsp는 현재 스택 프레임에서 유저 스택의 맨 위 주소를 가리키게 됩니다. 따라서 이 코드에서 유저 스택에 데이터를 저장하거나 로드할 때는 if_->rsp를 사용하여 주소를 계산하면 됩니다.
//f->rsp는 현재 스택 프레임에서 유저 스택의 맨 위 주소
//유저 스택에 접근하기 위해서는 f->rsp를 사용하여 주소를 계산할 수 있습니다.
void
syscall_handler (struct intr_frame *f UNUSED) {
	/* Get the system call number. */
	int syscall_number = (int) f->R.rax;
	// TODO: Your implementation goes here.
	    // 현재 실행 중인 쓰레드의 상태를 커널 스택에 저장
    //push(thread_current()->kernel_esp);
    switch (syscall_number){
        // case SYS_TELL : //1
        //     create();
        //     break;
        // case SYS_CLOSE :  //1
        //     create();
        //     break;
        // case SYS_READ :  //3
        //     create();
        //     break;
        // case SYS_WRITE : //3
        //  check_user_memory(arg2, arg3);
        //  arg1 = get_kernel_address(arg1);
        //  int bytes_written = write(arg1, arg2, arg3);
        // f->R.rax = bytes_written;
        // case SYS_SEEK : 
        //     create();
        //     break;
        // case SYS_OPEN : 
        //     create();
        //     break;
        // case SYS_FILESIZE : 
        //     create();
        //     break;
        // case SYS_REMOVE : 
        //     create();
        //     break;
        // case SYS_CREATE :
        //  check_address((const void*) f->R.rdi);
        //  int initial_size = (int) f->R.rsi;
        //  bool create_success = create((const char *) f->R.rdi, initial_size);
        // set_syscall_return_value(create_success);
        case SYS_FORK : //시스템을 종료
			power_off(); //void
			break;
		case SYS_HALT : //시스템을 종료
			halt();  //void
			break;
		case SYS_WAIT:
            thread_exit((int)f->R.rdi);
            break;
        case SYS_EXIT: //현재 실행 중인 프로세스를 종료
            exit((int)f->R.rdi);
            break;
		case SYS_EXEC :
			exec(); //1
			break;
        // case SYS_WRITE:
        //     check_user_memory(arg2, arg3);
        //     arg1 = get_kernel_address(arg1);
        //     int bytes_written = write(arg1, arg2, arg3);
        //     set_syscall_return_value(bytes_written);
        //     break;
        // 다른 시스템 콜 처리 작업
        // ...
        default:
            printf("Unhandled system call!\n");
            thread_exit();
    }

    // // 저장된 현재 쓰레드의 상태를 로드하여 다시 해당 쓰레드를 실행
    // thread_current()->kernel_esp = pop();
	printf ("system call!\n");
	thread_exit ();
}
