#include "threads/thread.h"
#include <debug.h>
#include <stddef.h>
#include <random.h>
#include <stdio.h>
#include <string.h>
#include "threads/flags.h"
#include "threads/interrupt.h"
#include "threads/intr-stubs.h"
#include "threads/palloc.h"
#include "threads/synch.h"
#include "threads/vaddr.h"
#include "intrinsic.h"
#ifdef USERPROG
#include "userprog/process.h"
#endif

/* Random value for struct thread's `magic' member.
   Used to detect stack overflow.  See the big comment at the top
   of thread.h for details. */
#define THREAD_MAGIC 0xcd6abf4b

/* Random value for basic thread
   Do not modify this value. */
#define THREAD_BASIC 0xd42df210

/* thread/thread.c */
static struct list sleep_list;

/* List of processes in THREAD_READY state, that is, processes
   that are ready to run but not actually running. */
static struct list ready_list;

/* Idle thread. */
static struct thread *idle_thread;

/* Initial thread, the thread running init.c:main(). */
static struct thread *initial_thread;

/* Lock used by allocate_tid(). */
static struct lock tid_lock;

/* Thread destruction requests */
static struct list destruction_req;

/* Statistics. */
static long long idle_ticks;    /* # of timer ticks spent idle. */
static long long kernel_ticks;  /* # of timer ticks in kernel threads. */
static long long user_ticks;    /* # of timer ticks in user programs. */

/* Scheduling. */
#define TIME_SLICE 4            /* # 운영체제가 각 스레드에게 할당하는 최대 CPU 시간 of timer ticks to give each thread. */
//각 스레드는 최대 4개의 타이머 틱 동안 CPU를 점유할 수 있습니다.
static unsigned thread_ticks;   /* # 현재 실행 중인 스레드가 CPU를 점유한 시간을 카운트하는 변수 of timer ticks since last yield. */
//즉, thread_ticks는 실제로 스레드가 CPU를 점유한 시간을 카운트하는 변수
/* If false (default), use round-robin scheduler.
   If true, use multi-level feedback queue scheduler.
   Controlled by kernel command-line option "-o mlfqs". */
bool thread_mlfqs;

static void kernel_thread (thread_func *, void *aux);

static void idle (void *aux UNUSED);
static struct thread *next_thread_to_run (void);
static void init_thread (struct thread *, const char *name, int priority);
static void do_schedule(int status);
static void schedule (void);
static tid_t allocate_tid (void);

/* Returns true if T appears to point to a valid thread. */
#define is_thread(t) ((t) != NULL && (t)->magic == THREAD_MAGIC)

/* Returns the running thread.
 * Read the CPU's stack pointer `rsp', and then round that
 * down to the start of a page.  Since `struct thread' is
 * always at the beginning of a page and the stack pointer is
 * somewhere in the middle, this locates the curent thread. */
#define running_thread() ((struct thread *) (pg_round_down (rrsp ())))


// Global descriptor table for the thread_start.
// Because the gdt will be setup after the thread_init, we should
// setup temporal gdt first.
static uint64_t gdt[3] = { 0, 0x00af9a000000ffff, 0x00cf92000000ffff };

/* Initializes the threading system by transforming the code
   that's currently running into a thread.  This can't work in
   general and it is possible in this case only because loader.S
   was careful to put the bottom of the stack at a page boundary.

   Also initializes the run queue and the tid lock.

   After calling this function, be sure to initialize the page
   allocator before trying to create any threads with
   thread_create().

   It is not safe to call thread_current() until this function
   finishes. */
   //주석1 : 실행 대기열(run queue)과 스레드 식별자(tid) 락을 초기화
void
thread_init (void) {
	ASSERT (intr_get_level () == INTR_OFF); //주석2 : 현재 인터럽트가 비활성화되어 있어야한다.

	/* Reload the temporal gdt for the kernel
	 * This gdt does not include the user context.
	 * The kernel will rebuild the gdt with user context, in gdt_init (). */
	struct desc_ptr gdt_ds = { //주석3 :  메모리 보호 및 세그먼트(Segment) 정의에 사용되는 데이터 구조체초기화, 
		.size = sizeof (gdt) - 1,
		.address = (uint64_t) gdt
	};
	lgdt (&gdt_ds); //주석4 ;  GDT를 로드

	/* Init the globla thread context */ //주석4: 단순 초기화
	lock_init (&tid_lock); //tid_lock은 스레드 식별자(tid)를 할당하는 데 사용되는 뮤텍스(lock)
	list_init (&ready_list);
	list_init (&sleep_list);
	list_init (&destruction_req);
	//list_init() 함수는 실행 대기열(ready_list) 및 파괴 요청(destruction_req)을 초기화하는 함수입니다. 실행 대기열은 실행 가능한 모든 스레드를 저장하는 큐이며, 스케줄러가 이 큐에서 스레드를 선택하여 실행합니다. 파괴 요청은 스레드 파괴를 지연시키기 위해 사용되는 큐입니다. 스레드 파괴는 해당 스레드가 더 이상 필요하지 않을 때, 메모리에서 삭제되는 과정을 말합니다. 그러나, 다른 스레드가 해당 스레드를 참조하고 있을 때, 해당 스레드를 즉시 파괴할 수 없습니다. 이 경우, 파괴 요청 큐에 스레드를 추가하여, 나중에 스레드를 파괴하도록 지연시킵니다.

	/* Set up a thread structure for the running thread. */
	initial_thread = running_thread ();
	init_thread (initial_thread, "main", PRI_DEFAULT);
	initial_thread->status = THREAD_RUNNING;
	initial_thread->tid = allocate_tid ();
}

/* Starts preemptive thread scheduling by enabling interrupts.
   Also creates the idle thread. 
주석5: 인터럽트를 활성화하여 선점형 스레드 스케줄링을 시작합니다.
또한 idle 스레드를 생성합니다. */
void
thread_start (void) {
	/* Create the idle thread. */
	struct semaphore idle_started;
	sema_init (&idle_started, 0);
	thread_create ("idle", PRI_MIN, idle, &idle_started);// "idle"이라는 이름의 스레드를 생성

	/* 선점형 스레드 스케줄링을 시작합니다. Start preemptive thread scheduling. *//* */
	intr_enable (); //인터럽트를 활성화하고, 스레드 스케줄링을 시작

	/* idle 스레드가 idle_thread를 초기화할 때까지 대기합니다. Wait for the idle thread to initialize idle_thread. *//*  */
	sema_down (&idle_started); //"sema_down" 함수를 사용하여 "idle_started" 세마포어를 대기합니다.
	//이 함수는 "idle" 스레드가 "idle_thread"를 초기화한 후 세마포어를 릴리즈하면, 현재 스레드가 실행을 재개합니다.

}

/* Called by the timer interrupt handler at each timer tick.
   Thus, this function runs in an external interrupt context. */
   ///* 타이머 인터럽트 핸들러에 의해 각각의 타이머 틱마다 호출됩니다.
//따라서, 이 함수는 외부 인터럽트 컨텍스트에서 실행됩니다. */
//주석9: thread_tick() 함수에서는 thread_ticks를 1씩 증가시키고, thread_ticks가 TIME_SLICE 이상이 되면 스레드를 선점하는 작업을 수행
void
thread_tick (void) {
	struct thread *t = thread_current ();

	/* Update statistics. */
	if (t == idle_thread)  // 현재 스레드가 idle 스레드인 경우
		idle_ticks++;  // idle 스레드의 CPU 사용 시간 통계 업데이트
#ifdef USERPROG
	else if (t->pml4 != NULL) // 현재 스레드가 유저 프로세스인 경우
		user_ticks++; // 유저 프로세스의 CPU 사용 시간 통계 업데이트
#endif
	else // 현재 스레드가 커널 스레드인 경우
		kernel_ticks++; // 커널 스레드의 CPU 사용 시간 통계 업데이트

	/* 선점 처리 실행 */ /* Enforce preemption. */
	if (++thread_ticks >= TIME_SLICE) // TIME_SLICE 시간마다 스레드를 선점할 수 있도록 타이머 틱 카운트 증가
		intr_yield_on_return (); // 타이머 인터럽트 리턴 시 선점 처리 실행
		// 현재 실행 중인 스레드가 CPU를 점유한 시간을 thread_ticks 변수로 카운트하면서, TIME_SLICE 값에 도달하면 스레드를 선점해야 한다는 의미입니다. 즉, 스레드가 CPU를 점유하는 시간이 TIME_SLICE 값을 넘어가면, 해당 스레드는 다른 스레드에게 CPU를 양보해야 합니다.
		//intr_yield_on_return() 함수는 현재 실행 중인 스레드가 선점되도록 하기 위해, 현재 실행 중인 코드의 실행을 중단하고 인터럽트가 반환될 때 선점 처리를 실행하도록 합니다. 따라서, intr_yield_on_return() 함수를 호출하면, 현재 실행 중인 스레드는 선점되어 다른 스레드에게 CPU를 양보합니다.
}

/* Prints thread statistics. */
void
thread_print_stats (void) {
	printf ("Thread: %lld idle ticks, %lld kernel ticks, %lld user ticks\n",
			idle_ticks, kernel_ticks, user_ticks);
}

/* Creates a new kernel thread named NAME with the given initial
   PRIORITY, which executes FUNCTION passing AUX as the argument,
   and adds it to the ready queue.  Returns the thread identifier
   for the new thread, or TID_ERROR if creation fails.

   If thread_start() has been called, then the new thread may be
   scheduled before thread_create() returns.  It could even exit
   before thread_create() returns.  Contrariwise, the original
   thread may run for any amount of time before the new thread is
   scheduled.  Use a semaphore or some other form of
   synchronization if you need to ensure ordering.

   The code provided sets the new thread's `priority' member to
   PRIORITY, but no actual priority scheduling is implemented.
   Priority scheduling is the goal of Problem 1-3. */
tid_t
thread_create (const char *name, int priority,
		thread_func *function, void *aux) {
	struct thread *t;
	tid_t tid;

	ASSERT (function != NULL);

	/* Allocate thread. */
	t = palloc_get_page (PAL_ZERO);
	if (t == NULL)
		return TID_ERROR;

	/* Initialize thread. */
	init_thread (t, name, priority);
	tid = t->tid = allocate_tid ();

	/* Call the kernel_thread if it scheduled.
	 * Note) rdi is 1st argument, and rsi is 2nd argument. */
	t->tf.rip = (uintptr_t) kernel_thread;
	t->tf.R.rdi = (uint64_t) function;
	t->tf.R.rsi = (uint64_t) aux;
	t->tf.ds = SEL_KDSEG;
	t->tf.es = SEL_KDSEG;
	t->tf.ss = SEL_KDSEG;
	t->tf.cs = SEL_KCSEG;
	t->tf.eflags = FLAG_IF;

	/* Add to run queue. */
	thread_unblock (t);

	return tid;
}

/* Puts the current thread to sleep.  It will not be scheduled
   again until awoken by thread_unblock().

   This function must be called with interrupts turned off.  It
   is usually a better idea to use one of the synchronization
   primitives in synch.h. */
   /* 현재 스레드를 슬립 상태로 만듭니다.
thread_unblock() 함수에 의해 깨어날 때까지 스케줄되지 않습니다.

이 함수는 인터럽트가 꺼진 상태에서 호출해야 합니다.
일반적으로는 synch.h 파일에 있는 동기화 프리미티브를 사용하는 것이 좋습니다. */
void
thread_block (void) {
	ASSERT (!intr_context ());
	ASSERT (intr_get_level () == INTR_OFF);
	thread_current ()->status = THREAD_BLOCKED;
	schedule (); //스케줄러에게 다음에 실행될 스레드를 선택하도록 합니다. 이후, 선택된 스레드가 THREAD_RUNNING 상태로 변경되고, 해당 스레드의 컨텍스트가 복원되어 실행됩니다. 
}

/* Transitions a blocked thread T to the ready-to-run state.
   This is an error if T is not blocked.  (Use thread_yield() to
   make the running thread ready.)

   This function does not preempt the running thread.  This can
   be important: if the caller had disabled interrupts itself,
   it may expect that it can atomically unblock a thread and
   update other data. */
void
thread_unblock (struct thread *t) {
	enum intr_level old_level;
	old_level = intr_disable ();
	ASSERT (t->status == THREAD_BLOCKED); //안됐다.
	list_push_back (&ready_list, &t->elem);
	t->status = THREAD_READY;
	intr_set_level (old_level);
}

bool less_func(const struct list_elem *a,
               const struct list_elem *b,
               void *aux UNUSED) {
    struct thread *process_a = list_entry(a, struct thread, elem);
    struct thread *process_b = list_entry(b, struct thread, elem);
    // Compare the values of process_a and process_b.
    // If process_a is less than process_b, return true.
    // Otherwise, return false.
    return (process_a->wake_time < process_b->wake_time);
}

void
thread_sleep (int64_t ticks) {
	enum intr_level old_level;
	ASSERT (!intr_context ());
	old_level = intr_disable ();
	struct thread *curr = thread_current ();
	if (curr != idle_thread) {
		curr -> wake_time = ticks; // 깨울시간 저장 (구조체를 깨울 시간 추가한 모양으로 변경)
		//list_push_back (&sleep_list, &curr->elem);
		list_insert_ordered(&sleep_list,&curr->elem, less_func,NULL);
	}
	thread_block();
	intr_set_level (old_level);
}

//thread_awake() 함수는 현재 시간(ticks)이 특정 스레드의 wake_time보다 크거나 같은 경우,
//스레드를 깨우는 작업을 수행합니다.
//기존에 슬립 상태에 있는 스레드 목록을 순회하여, 해당 조건을 만족하는 스레드를 찾아야 합니다. 만약 조건을 만족하는 스레드를 찾으면,
//스레드를 슬립 상태에서 깨우기 위해 list_remove() 함수를 호출하고, 스레드 상태를 THREAD_READY로 변경하여 ready_list에 추가합니다.
 void ///* Sleep queue에서 깨워야 할 thread를 찾아서 wake */
 thread_awake (int64_t ticks) {//ticks을 받아온다.
 	while (!list_empty(&sleep_list)) {
     	struct thread *t = list_entry (list_front (&sleep_list), struct thread, elem);
         if (t->wake_time > ticks) {
             break;
        }
 		struct thread *wakeup_thread = list_entry(list_pop_front(&sleep_list), struct thread, elem);
// 		//wakeup_thread->status = THREAD_READY;
// 		//list_push_back (&ready_list, &wakeup_thread->elem);
 		thread_unblock(wakeup_thread); // 이 함수를 부르면 옆과 같은 세팅이 된다 t->status == THREAD_BLOCKED
 	}
}
/*
void ///* Sleep queue에서 깨워야 할 thread를 찾아서 wake */
//void thread_awake (int64_t ticks) {
//  struct list_elem *elem, *next;
// for (elem = list_begin (&sleep_list); elem != list_end (&sleep_list); elem = next) {
//    struct thread *t = list_entry (elem, struct thread, elem);
//    next = list_next (elem);

//    if (t->wake_time <= ticks) {
//      list_remove (elem);
//      thread_unblock (t);
//	  next = list_begin (&sleep_list); // 수정된 부분
//    }
//  }
//}

/* Returns the name of the running thread. */
const char *
thread_name (void) {
	return thread_current ()->name;
}

/* Returns the running thread.
   This is running_thread() plus a couple of sanity checks.
   See the big comment at the top of thread.h for details. */
struct thread *
thread_current (void) {
	struct thread *t = running_thread ();
	/* Make sure T is really a thread.
	   If either of these assertions fire, then your thread may
	   have overflowed its stack.  Each thread has less than 4 kB
	   of stack, so a few big automatic arrays or moderate
	   recursion can cause stack overflow. */
	ASSERT (is_thread (t));
	ASSERT (t->status == THREAD_RUNNING);
	return t;
}

/* Returns the running thread's tid. */
tid_t
thread_tid (void) {
	return thread_current ()->tid;
}

/* Deschedules the current thread and destroys it.  Never
   returns to the caller. */
void
thread_exit (void) {
	ASSERT (!intr_context ());

#ifdef USERPROG
	process_exit ();
#endif

	/* Just set our status to dying and schedule another process.
	   We will be destroyed during the call to schedule_tail(). */
	intr_disable ();
	do_schedule (THREAD_DYING);
	NOT_REACHED ();
}

/* Yields the CPU.  The current thread is not put to sleep and
   may be scheduled again immediately at the scheduler's whim. */
void
thread_yield (void) {
	struct thread *curr = thread_current ();
	enum intr_level old_level;

	ASSERT (!intr_context ());

	old_level = intr_disable ();
	if (curr != idle_thread)
		list_push_back (&ready_list, &curr->elem);
	do_schedule (THREAD_READY);
	intr_set_level (old_level);
}

/* Sets the current thread's priority to NEW_PRIORITY. */
void
thread_set_priority (int new_priority) {
	thread_current ()->priority = new_priority;
}

/* Returns the current thread's priority. */
int
thread_get_priority (void) {
	return thread_current ()->priority;
}

/* Sets the current thread's nice value to NICE. */
void
thread_set_nice (int nice UNUSED) {
	/* TODO: Your implementation goes here */
}

/* Returns the current thread's nice value. */
int
thread_get_nice (void) {
	/* TODO: Your implementation goes here */
	return 0;
}

/* Returns 100 times the system load average. */
int
thread_get_load_avg (void) {
	/* TODO: Your implementation goes here */
	return 0;
}

/* Returns 100 times the current thread's recent_cpu value. */
int
thread_get_recent_cpu (void) {
	/* TODO: Your implementation goes here */
	return 0;
}

/* Idle thread.  Executes when no other thread is ready to run.

   The idle thread is initially put on the ready list by
   thread_start().  It will be scheduled once initially, at which
   point it initializes idle_thread, "up"s the semaphore passed
   to it to enable thread_start() to continue, and immediately
   blocks.  After that, the idle thread never appears in the
   ready list.  It is returned by next_thread_to_run() as a
   special case when the ready list is empty. */
static void
idle (void *idle_started_ UNUSED) {
	struct semaphore *idle_started = idle_started_;

	idle_thread = thread_current ();
	sema_up (idle_started);

	for (;;) {
		/* Let someone else run. */
		intr_disable ();
		thread_block ();

		/* Re-enable interrupts and wait for the next one.

		   The `sti' instruction disables interrupts until the
		   completion of the next instruction, so these two
		   instructions are executed atomically.  This atomicity is
		   important; otherwise, an interrupt could be handled
		   between re-enabling interrupts and waiting for the next
		   one to occur, wasting as much as one clock tick worth of
		   time.

		   See [IA32-v2a] "HLT", [IA32-v2b] "STI", and [IA32-v3a]
		   7.11.1 "HLT Instruction". */
		asm volatile ("sti; hlt" : : : "memory");
	}
}

/* Function used as the basis for a kernel thread. */
static void
kernel_thread (thread_func *function, void *aux) {
	ASSERT (function != NULL);

	intr_enable ();       /* The scheduler runs with interrupts off. */
	function (aux);       /* Execute the thread function. */
	thread_exit ();       /* If function() returns, kill the thread. */
}


/* Does basic initialization of T as a blocked thread named
   NAME. */
static void
init_thread (struct thread *t, const char *name, int priority) {
	ASSERT (t != NULL);
	ASSERT (PRI_MIN <= priority && priority <= PRI_MAX);
	ASSERT (name != NULL);

	memset (t, 0, sizeof *t);
	t->status = THREAD_BLOCKED;
	strlcpy (t->name, name, sizeof t->name);
	t->tf.rsp = (uint64_t) t + PGSIZE - sizeof (void *);
	t->priority = priority;
	t->magic = THREAD_MAGIC;
}

/* Chooses and returns the next thread to be scheduled.  Should
   return a thread from the run queue, unless the run queue is
   empty.  (If the running thread can continue running, then it
   will be in the run queue.)  If the run queue is empty, return
   idle_thread. */
static struct thread *
next_thread_to_run (void) {
	if (list_empty (&ready_list))
		return idle_thread;
	else
		return list_entry (list_pop_front (&ready_list), struct thread, elem);
}

/* Use iretq to launch the thread */
void
do_iret (struct intr_frame *tf) {
	__asm __volatile(
			"movq %0, %%rsp\n"
			"movq 0(%%rsp),%%r15\n"
			"movq 8(%%rsp),%%r14\n"
			"movq 16(%%rsp),%%r13\n"
			"movq 24(%%rsp),%%r12\n"
			"movq 32(%%rsp),%%r11\n"
			"movq 40(%%rsp),%%r10\n"
			"movq 48(%%rsp),%%r9\n"
			"movq 56(%%rsp),%%r8\n"
			"movq 64(%%rsp),%%rsi\n"
			"movq 72(%%rsp),%%rdi\n"
			"movq 80(%%rsp),%%rbp\n"
			"movq 88(%%rsp),%%rdx\n"
			"movq 96(%%rsp),%%rcx\n"
			"movq 104(%%rsp),%%rbx\n"
			"movq 112(%%rsp),%%rax\n"
			"addq $120,%%rsp\n"
			"movw 8(%%rsp),%%ds\n"
			"movw (%%rsp),%%es\n"
			"addq $32, %%rsp\n"
			"iretq"
			: : "g" ((uint64_t) tf) : "memory");
}

/* Switching the thread by activating the new thread's page
   tables, and, if the previous thread is dying, destroying it.

   At this function's invocation, we just switched from thread
   PREV, the new thread is already running, and interrupts are
   still disabled.

   It's not safe to call printf() until the thread switch is
   complete.  In practice that means that printf()s should be
   added at the end of the function. */
static void
thread_launch (struct thread *th) {
	uint64_t tf_cur = (uint64_t) &running_thread ()->tf;
	uint64_t tf = (uint64_t) &th->tf;
	ASSERT (intr_get_level () == INTR_OFF);

	/* The main switching logic.
	 * We first restore the whole execution context into the intr_frame
	 * and then switching to the next thread by calling do_iret.
	 * Note that, we SHOULD NOT use any stack from here
	 * until switching is done. */
	__asm __volatile (
			/* Store registers that will be used. */
			"push %%rax\n"
			"push %%rbx\n"
			"push %%rcx\n"
			/* Fetch input once */
			"movq %0, %%rax\n"
			"movq %1, %%rcx\n"
			"movq %%r15, 0(%%rax)\n"
			"movq %%r14, 8(%%rax)\n"
			"movq %%r13, 16(%%rax)\n"
			"movq %%r12, 24(%%rax)\n"
			"movq %%r11, 32(%%rax)\n"
			"movq %%r10, 40(%%rax)\n"
			"movq %%r9, 48(%%rax)\n"
			"movq %%r8, 56(%%rax)\n"
			"movq %%rsi, 64(%%rax)\n"
			"movq %%rdi, 72(%%rax)\n"
			"movq %%rbp, 80(%%rax)\n"
			"movq %%rdx, 88(%%rax)\n"
			"pop %%rbx\n"              // Saved rcx
			"movq %%rbx, 96(%%rax)\n"
			"pop %%rbx\n"              // Saved rbx
			"movq %%rbx, 104(%%rax)\n"
			"pop %%rbx\n"              // Saved rax
			"movq %%rbx, 112(%%rax)\n"
			"addq $120, %%rax\n"
			"movw %%es, (%%rax)\n"
			"movw %%ds, 8(%%rax)\n"
			"addq $32, %%rax\n"
			"call __next\n"         // read the current rip.
			"__next:\n"
			"pop %%rbx\n"
			"addq $(out_iret -  __next), %%rbx\n"
			"movq %%rbx, 0(%%rax)\n" // rip
			"movw %%cs, 8(%%rax)\n"  // cs
			"pushfq\n"
			"popq %%rbx\n"
			"mov %%rbx, 16(%%rax)\n" // eflags
			"mov %%rsp, 24(%%rax)\n" // rsp
			"movw %%ss, 32(%%rax)\n"
			"mov %%rcx, %%rdi\n"
			"call do_iret\n"
			"out_iret:\n"
			: : "g"(tf_cur), "g" (tf) : "memory"
			);
}

/* Schedules a new process. At entry, interrupts must be off.
 * This function modify current thread's status to status and then
 * finds another thread to run and switches to it.
 * It's not safe to call printf() in the schedule(). */
static void
do_schedule(int status) {
	ASSERT (intr_get_level () == INTR_OFF);
	ASSERT (thread_current()->status == THREAD_RUNNING);
	while (!list_empty (&destruction_req)) {
		struct thread *victim =
			list_entry (list_pop_front (&destruction_req), struct thread, elem);
		palloc_free_page(victim);
	}
	thread_current ()->status = status;
	schedule ();
}

// scheduling 함수는 thread_yield(), thread_block(), thread_exit()
// 함수 내의 거의 마지막 부분에 실행되어 CPU 의 소유권을 현재 실행중인 스레드에서 다음에 실행될 스레드로 넘겨주는 작업을 한다.
static void
schedule (void) {
	struct thread *curr = running_thread ();
	struct thread *next = next_thread_to_run ();

	ASSERT (intr_get_level () == INTR_OFF);
	ASSERT (curr->status != THREAD_RUNNING);
	ASSERT (is_thread (next));
	/* Mark us as running. */
	next->status = THREAD_RUNNING;

	/* Start new time slice. */
	thread_ticks = 0;

#ifdef USERPROG
	/* Activate the new address space. */
	process_activate (next);
#endif

	if (curr != next) {
		/* If the thread we switched from is dying, destroy its struct
		   thread. This must happen late so that thread_exit() doesn't
		   pull out the rug under itself.
		   We just queuing the page free reqeust here because the page is
		   currently used by the stack.
		   The real destruction logic will be called at the beginning of the
		   schedule(). */
		if (curr && curr->status == THREAD_DYING && curr != initial_thread) { // initial_thread는 시스템 초기화 단계에서 생성된 스레드로, 종료될 수 없는 스레드
			ASSERT (curr != next);
			list_push_back (&destruction_req, &curr->elem); //destruction_req 큐에 추가
		}

		/* Before switching the thread, we first save the information
		 * of current running. */
		thread_launch (next);
	}
}

/* Returns a tid to use for a new thread. */
static tid_t
allocate_tid (void) {
	static tid_t next_tid = 1;
	tid_t tid;

	lock_acquire (&tid_lock);
	tid = next_tid++;
	lock_release (&tid_lock);

	return tid;
}
