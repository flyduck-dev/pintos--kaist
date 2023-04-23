#include "devices/timer.h"
#include <debug.h>
#include <inttypes.h>
#include <round.h>
#include <stdio.h>
#include "threads/interrupt.h"
#include "threads/io.h"
#include "threads/synch.h"
#include "threads/thread.h"

/* See [8254] for hardware details of the 8254 timer chip. */

#if TIMER_FREQ < 19
#error 8254 timer requires TIMER_FREQ >= 19
#endif
#if TIMER_FREQ > 1000
#error TIMER_FREQ <= 1000 recommended
#endif

/* Number of timer ticks since OS booted. */
static int64_t ticks;

/* Number of loops per timer tick.
   Initialized by timer_calibrate(). */
static unsigned loops_per_tick;

static intr_handler_func timer_interrupt;
static bool too_many_loops (unsigned loops);
static void busy_wait (int64_t loops);
static void real_time_sleep (int64_t num, int32_t denom);

/* Sets up the 8254 Programmable Interval Timer (PIT) to
   interrupt PIT_FREQ times per second, and registers the
   corresponding interrupt. */
   //주석 6 8254 Programmable Interval Timer (PIT)를 설정하여 PIT_FREQ 횟수만큼 인터럽트를 발생 & 등록
   //PIT_FREQ 만큼의 주기로 인터럽트를 발생시키도록 하는 코드
   //PIT는 컴퓨터의 하드웨어적인 타이머이며, 주어진 주기로 인터럽트를 발생, 운영체제의 스케줄링을 지원
   //PIT_FREQ는 매크로 상수로, PIT가 발생시키는 인터럽트 주기를 지정합니다.
   //timer_init() 함수에서 PIT를 설정하고 인터럽트를 등록하여 스케줄링에 사용됩니다.
   //1초에 100번의 인터럽트가 발생하게 됩니다.
   //8254 Programmable Interval Timer(PIT)에서 주기적으로 발생하는 인터럽트는, 운영체제가 스케줄링을 수행할 수 있도록, 다음 작업으로 제어를 넘겨주는 역할을 합니다. PIT 인터럽트가 발생하면, 현재 실행 중인 프로세스나 스레드는 실행 중지되고, 운영체제는 다음에 실행할 프로세스나 스레드를 선택하여 실행합니다. 따라서, PIT 주기를 적절히 설정함으로써, 스케줄링 성능을 향상시키는 것이 가능합니다.
void
timer_init (void) {
	/* 8254 input frequency divided by TIMER_FREQ, rounded to
	   nearest. */
	uint16_t count = (1193180 + TIMER_FREQ / 2) / TIMER_FREQ;

	outb (0x43, 0x34);    /* CW: counter 0, LSB then MSB, mode 2, binary. */
	outb (0x40, count & 0xff);
	outb (0x40, count >> 8);
	//이 코드는 8254 PIT를 초기화하고, 인터럽트 주기를 설정하는 과정을 담고 있습니다. 

	intr_register_ext (0x20, timer_interrupt, "8254 Timer"); // 인터럽트 벡터와 그에 대응하는 인터럽트 핸들러 함수를 등록
	//주어진 코드는 8254 Programmable Interval Timer(PIT)에 대응하는 인터럽트 벡터 0x20과 timer_interrupt 함수를 등록하는 코드입니다.
	//주석 7:timer_interrupt 함수는 PIT 인터럽트가 발생할 때마다 호출되며,
	//PIT 인터럽트 핸들러로서, 시스템 클럭의 역할을 합니다.
	//timer_interrupt 함수는 운영체제의 스케줄링 작업을 위해,
	//PIT 인터럽트가 발생하는 일정 주기마다 호출되어, 현재 실행 중인 프로세스나 스레드를 중지하고 다음 실행할 프로세스나 스레드를 선택하는 등의 작업을 수행합니다.
	//마지막으로, "8254 Timer"은 해당 인터럽트 핸들러를 설명하는 문자열로, 디버깅 및 추적에 사용됩니다.

}

/* Calibrates loops_per_tick, used to implement brief delays. */
void
timer_calibrate (void) {
	unsigned high_bit, test_bit;

	ASSERT (intr_get_level () == INTR_ON);
	printf ("Calibrating timer...  ");

	/* Approximate loops_per_tick as the largest power-of-two
	   still less than one timer tick. */
	loops_per_tick = 1u << 10;
	while (!too_many_loops (loops_per_tick << 1)) {
		loops_per_tick <<= 1;
		ASSERT (loops_per_tick != 0);
	}

	/* Refine the next 8 bits of loops_per_tick. */
	high_bit = loops_per_tick;
	for (test_bit = high_bit >> 1; test_bit != high_bit >> 10; test_bit >>= 1)
		if (!too_many_loops (high_bit | test_bit))
			loops_per_tick |= test_bit;

	printf ("%'"PRIu64" loops/s.\n", (uint64_t) loops_per_tick * TIMER_FREQ);
}

/* Returns the number of timer ticks since the OS booted. */
int64_t
timer_ticks (void) {
	enum intr_level old_level = intr_disable ();
	int64_t t = ticks;
	intr_set_level (old_level);
	barrier ();
	return t;
}

/* Returns the number of timer ticks elapsed since THEN, which
   should be a value once returned by timer_ticks(). */
int64_t
timer_elapsed (int64_t then) { //then os 시작 시간 ex) 10
	return timer_ticks () - then; //2초밖에 안 지났다면 .. 2초
}

/* Suspends execution for approximately TICKS timer ticks. */
void
timer_sleep (int64_t ticks) {
	int64_t start = timer_ticks (); //os 시작 시간 ex)10

	ASSERT (intr_get_level () == INTR_ON);
	//bust-waiting 수정
	if(timer_elapsed (start) < ticks) {
		//printf("bust-waiting 수정");
		thread_sleep(start + ticks);
	}
	//while (timer_elapsed (start) < ticks) //timer_elapsed : 쓰레드의 시작 시간, timer_elapsed() 함수는 특정시간 이후로 경과된 시간(ticks) 를 반환한다. 즉, timer_elapsed(start) 는 start 이후로 경과된 시간(ticks)을 반환한다.
	//	thread_yield (); //2초 < ticks(어쨌든 자야할 시간)
}

/* Suspends execution for approximately MS milliseconds. */
void
timer_msleep (int64_t ms) {
	real_time_sleep (ms, 1000);
}

/* Suspends execution for approximately US microseconds. */
void
timer_usleep (int64_t us) {
	real_time_sleep (us, 1000 * 1000);
}

/* Suspends execution for approximately NS nanoseconds. */
void
timer_nsleep (int64_t ns) {
	real_time_sleep (ns, 1000 * 1000 * 1000);
}

/* Prints timer statistics. */
void
timer_print_stats (void) {
	printf ("Timer: %"PRId64" ticks\n", timer_ticks ());
}

/* Timer interrupt handler. */
//주석 8 :ticks는 시스템이 부팅된 이후 PIT 인터럽트가 몇 번 발생했는지를 나타내는 값입니다. 
//timer 인터럽트는 매 tick 마다 ticks 라는 변수를 증가시킴으로서 시간을 잰다. 
static void
timer_interrupt (struct intr_frame *args UNUSED) {
	ticks++; //시스템 부팅 이후 8254 Programmable Interval Timer(PIT) 인터럽트가 발생한 횟수
//8254 Programmable Interval Timer(PIT) 인터럽트가 발생한 횟수를 계속 늘려나가는 것은, 운영체제에서 시스템의 성능 및 동작을 모니터링하기 위한 목적
//이 값은 다양한 용도로 활용될 수 있습니다.
//ticks 변수는 운영체제가 실행 중인 시간을 측정하는 데 사용될 수 있습니다.
//ticks 변수는 타이머 인터럽트가 발생할 때마다 1씩 증가하므로,
//운영체제가 실행된 시간을 초 단위로 계산하려면 ticks 값을 PIT 인터럽트 발생 주기로 나누면 됩니다. 
//ticks 변수는 프로세스의 실행 시간을 측정하는 데도 활용될 수 있습니다.
//각 프로세스마다 실행 시간을 측정하기 위해, 프로세스가 실행되는 동안 ticks 값을 저장하고,
//실행 종료 시 ticks 값의 차이를 계산하여 해당 프로세스의 실행 시간을 측정할 수 있습니다.
	thread_tick (); //현재 실행 중인 스레드의 CPU 시간을 증가시키고, 우선순위를 조절하는 등의 스레드 스케줄링 작업을 수행
	thread_awake(ticks);
}

/* Returns true if LOOPS iterations waits for more than one timer
   tick, otherwise false. */
static bool
too_many_loops (unsigned loops) {
	/* Wait for a timer tick. */
	int64_t start = ticks;
	while (ticks == start)
		barrier ();

	/* Run LOOPS loops. */
	start = ticks;
	busy_wait (loops);

	/* If the tick count changed, we iterated too long. */
	barrier ();
	return start != ticks;
}

/* Iterates through a simple loop LOOPS times, for implementing
   brief delays.

   Marked NO_INLINE because code alignment can significantly
   affect timings, so that if this function was inlined
   differently in different places the results would be difficult
   to predict. */
static void NO_INLINE
busy_wait (int64_t loops) {
	while (loops-- > 0)
		barrier ();
}

/* Sleep for approximately NUM/DENOM seconds. */
static void
real_time_sleep (int64_t num, int32_t denom) {
	/* Convert NUM/DENOM seconds into timer ticks, rounding down.

	   (NUM / DENOM) s
	   ---------------------- = NUM * TIMER_FREQ / DENOM ticks.
	   1 s / TIMER_FREQ ticks
	   */
	int64_t ticks = num * TIMER_FREQ / denom;

	ASSERT (intr_get_level () == INTR_ON);
	if (ticks > 0) {
		/* We're waiting for at least one full timer tick.  Use
		   timer_sleep() because it will yield the CPU to other
		   processes. */
		timer_sleep (ticks);
	} else {
		/* Otherwise, use a busy-wait loop for more accurate
		   sub-tick timing.  We scale the numerator and denominator
		   down by 1000 to avoid the possibility of overflow. */
		ASSERT (denom % 1000 == 0);
		busy_wait (loops_per_tick * num / 1000 * TIMER_FREQ / (denom / 1000));
	}
}
