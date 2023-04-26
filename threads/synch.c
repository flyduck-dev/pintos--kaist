/* This file is derived from source code for the Nachos
   instructional operating system.  The Nachos copyright notice
   is reproduced in full below. */

/* Copyright (c) 1992-1996 The Regents of the University of California.
   All rights reserved.

   Permission to use, copy, modify, and distribute this software
   and its documentation for any purpose, without fee, and
   without written agreement is hereby granted, provided that the
   above copyright notice and the following two paragraphs appear
   in all copies of this software.

   IN NO EVENT SHALL THE UNIVERSITY OF CALIFORNIA BE LIABLE TO
   ANY PARTY FOR DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR
   CONSEQUENTIAL DAMAGES ARISING OUT OF THE USE OF THIS SOFTWARE
   AND ITS DOCUMENTATION, EVEN IF THE UNIVERSITY OF CALIFORNIA
   HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

   THE UNIVERSITY OF CALIFORNIA SPECIFICALLY DISCLAIMS ANY
   WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
   WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
   PURPOSE.  THE SOFTWARE PROVIDED HEREUNDER IS ON AN "AS IS"
   BASIS, AND THE UNIVERSITY OF CALIFORNIA HAS NO OBLIGATION TO
   PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR
   MODIFICATIONS.
   */

#include "threads/synch.h"
#include <stdio.h>
#include <string.h>
#include "threads/interrupt.h"
#include "threads/thread.h"

void donate_priority(struct lock *lock){
	lock->holder->priority = thread_current()->priority;
}

/* 세마포어(SEMA)를 VALUE로 초기화합니다. 세마포어는 양의 정수 값과 이를 조작하는 데 사용되는 두 개의 원자적 연산자를 가집니다:

down 또는 "P": 값을 양수로 만들 때까지 기다린 다음 값을 감소시킵니다.

up 또는 "V": 값을 증가시키고 (있는 경우) 대기 중인 스레드 중 하나를 깨웁니다. */

//세마포어는 공유 자원에 대한 접근을 조율하기 위해 사용되는 동기화 기법 중 하나입니다.
//down 연산은 세마포어 값을 감소시키며, 값이 0이면 자원에 접근할 수 없는 상태가 됩니다.
//이때 다른 스레드는 세마포어 값이 양수가 될 때까지 기다립니다. up 연산은 세마포어 값을 증가시키며,
//대기 중인 스레드 중 하나를 깨웁니다. 이를 통해 다른 스레드가 자원에 접근할 수 있도록 합니다.
void
sema_init (struct semaphore *sema, unsigned value) {
	ASSERT (sema != NULL);
	//waiters는 세마포어에서 기다리고 있는 스레드들의 연결 리스트입니다.
	//이 리스트는 struct semaphore 구조체의 waiters 멤버로 선언되어 있습니다.
	sema->value = value;
	list_init (&sema->waiters); // 세마포어(sema)가 가지고 있는 대기자(waiter) 리스트를 초기화
	//세마포어가 가지고 있는 대기자 리스트를 초기화하는 것은, 세마포어를 초기화하면서 대기자 리스트를 빈 상태로 만들기 위함입니다.
}

/* 세마포어(SEMA)에 대한 Down 또는 "P" 연산을 수행합니다. 이 함수는 SEMA의 값을 양수로 만들 때까지 기다린 다음 값을 원자적으로 감소시킵니다.

이 함수는 슬립할 수 있으므로 인터럽트 핸들러 내에서 호출해서는 안 됩니다. 인터럽트가 비활성화된 상태에서 이 함수를 호출할 수는 있지만, 슬립해야 하는 경우 다음 스케줄된 스레드에서는 인터럽트가 다시 활성화될 가능성이 높습니다. 이 함수는 sema_down 함수입니다. */
//세마포어(sema)에 대한 down 연산(P 연산)을 수행하는 함수입니다.

//down 연산은 세마포어 값을 감소시키고, 값이 0이면 자원에 접근할 수 없는 상태가 됩니다.
//이 함수는 세마포어 값이 양수가 될 때까지 대기하며, 값을 원자적으로 감소시킵니다. 이 함수는 슬립할 수 있으므로 인터럽트 핸들러 내에서 호출해서는 안 됩니다. 인터럽트가 비활성화된 상태에서는 호출할 수 있지만, 이 함수가 슬립해야 하는 경우 다음 스케줄된 스레드에서는 인터럽트가 다시 활성화될 가능성이 높습니다.
void
sema_down (struct semaphore *sema) {
	enum intr_level old_level;

	ASSERT (sema != NULL);
	ASSERT (!intr_context ());

	old_level = intr_disable ();
	while (sema->value == 0) {
		list_insert_ordered (&sema->waiters, &thread_current ()->elem, value_more, 0);
		thread_block ();
	}
	sema->value--;
	intr_set_level (old_level);
}

/* Down or "P" operation on a semaphore, but only if the
   semaphore is not already 0.  Returns true if the semaphore is
   decremented, false otherwise.

   This function may be called from an interrupt handler. */
bool
sema_try_down (struct semaphore *sema) {
	enum intr_level old_level;
	bool success;

	ASSERT (sema != NULL);

	old_level = intr_disable ();
	if (sema->value > 0)
	{
		sema->value--;
		success = true;
	}
	else
		success = false;
	intr_set_level (old_level);

	return success;
}

/* Up or "V" operation on a semaphore.  Increments SEMA's value
   and wakes up one thread of those waiting for SEMA, if any.

   This function may be called from an interrupt handler. */
void
sema_up (struct semaphore *sema) {
	enum intr_level old_level;

	ASSERT (sema != NULL);

	old_level = intr_disable ();
	if (!list_empty (&sema->waiters))
		thread_unblock (list_entry (list_pop_front (&sema->waiters),
					struct thread, elem));
	sema->value++;
	intr_set_level (old_level);
}

static void sema_test_helper (void *sema_);

/* Self-test for semaphores that makes control "ping-pong"
   between a pair of threads.  Insert calls to printf() to see
   what's going on. */
void
sema_self_test (void) {
	struct semaphore sema[2];
	int i;

	printf ("Testing semaphores...");
	sema_init (&sema[0], 0);
	sema_init (&sema[1], 0);
	thread_create ("sema-test", PRI_DEFAULT, sema_test_helper, &sema);
	for (i = 0; i < 10; i++)
	{
		sema_up (&sema[0]);
		sema_down (&sema[1]);
	}
	printf ("done.\n");
}

/* Thread function used by sema_self_test(). */
static void
sema_test_helper (void *sema_) {
	struct semaphore *sema = sema_;
	int i;

	for (i = 0; i < 10; i++)
	{
		sema_down (&sema[0]);
		sema_up (&sema[1]);
	}
}

/* Initializes LOCK.  A lock can be held by at most a single
   thread at any given time.  Our locks are not "recursive", that
   is, it is an error for the thread currently holding a lock to
   try to acquire that lock.

   A lock is a specialization of a semaphore with an initial
   value of 1.  The difference between a lock and such a
   semaphore is twofold.  First, a semaphore can have a value
   greater than 1, but a lock can only be owned by a single
   thread at a time.  Second, a semaphore does not have an owner,
   meaning that one thread can "down" the semaphore and then
   another one "up" it, but with a lock the same thread must both
   acquire and release it.  When these restrictions prove
   onerous, it's a good sign that a semaphore should be used,
   instead of a lock. */
void
lock_init (struct lock *lock) {
	ASSERT (lock != NULL);

	lock->holder = NULL;
	sema_init (&lock->semaphore, 1);
}

/* LOCK을 획득합니다. LOCK이 이미 다른 스레드에 의해 점유되어 있는 경우, 이 함수는 LOCK이 사용 가능할 때까지 슬립 상태로 대기합니다.

이 함수는 슬립할 수 있으므로 인터럽트 핸들러 내에서 호출해서는 안 됩니다. 인터럽트가 비활성화된 상태에서 이 함수를 호출할 수는 있지만, 슬립해야 하는 경우 인터럽트가 다시 활성화됩니다. 

위 코드는 잠금(lock)을 획득하는 함수입니다. lock은 공유 자원에 대한 동시 접근을 방지하기 위해 사용되는 동기화 기법 중 하나입니다.

이 함수는 현재 스레드가 lock을 이미 소유하고 있지 않은 경우, lock을 획득하기 위해 대기할 수 있습니다. lock이 이미 다른 스레드에 의해 점유되어 있는 경우, 이 함수는 lock이 사용 가능할 때까지 슬립 상태로 대기합니다.

이 함수는 슬립할 수 있으므로, 인터럽트 핸들러 내에서 호출해서는 안 됩니다. 인터럽트가 비활성화된 상태에서 이 함수를 호출할 수는 있지만, 슬립해야 하는 경우 인터럽트가 다시 활성화됩니다. /**/
void
lock_acquire (struct lock *lock) {
	ASSERT (lock != NULL);
	ASSERT (!intr_context ());
	ASSERT (!lock_held_by_current_thread (lock));
	if (lock->holder != NULL && lock->holder->priority < thread_current()->priority) {
		donate_priority (lock);
		thread_yield();
	}
	sema_down (&lock->semaphore);
	lock->holder = thread_current ();
}

/* Tries to acquires LOCK and returns true if successful or false
   on failure.  The lock must not already be held by the current
   thread.

   This function will not sleep, so it may be called within an
   interrupt handler. */
bool
lock_try_acquire (struct lock *lock) {
	bool success;

	ASSERT (lock != NULL);
	ASSERT (!lock_held_by_current_thread (lock));

	success = sema_try_down (&lock->semaphore);
	if (success)
		lock->holder = thread_current ();
	return success;
}

/* Releases LOCK, which must be owned by the current thread.
   This is lock_release function.

   An interrupt handler cannot acquire a lock, so it does not
   make sense to try to release a lock within an interrupt
   handler. */
void
lock_release (struct lock *lock) {
	ASSERT (lock != NULL);
	ASSERT (lock_held_by_current_thread (lock));
	if(lock->holder->priority != lock->holder->origin_priority){
		lock->holder->priority = lock->holder->origin_priority;
	}
	lock->holder = NULL;
	sema_up (&lock->semaphore);
}

/* Returns true if the current thread holds LOCK, false
   otherwise.  (Note that testing whether some other thread holds
   a lock would be racy.) */
bool
lock_held_by_current_thread (const struct lock *lock) {
	ASSERT (lock != NULL);

	return lock->holder == thread_current ();
}

/* One semaphore in a list. */
struct semaphore_elem {
	struct list_elem elem;              /* List element. */
	struct semaphore semaphore;         /* This semaphore. */
};

/* Initializes condition variable COND.  A condition variable
   allows one piece of code to signal a condition and cooperating
   code to receive the signal and act upon it. */
void
cond_init (struct condition *cond) {
	ASSERT (cond != NULL);

	list_init (&cond->waiters);
}

/* Atomically releases LOCK and waits for COND to be signaled by
   some other piece of code.  After COND is signaled, LOCK is
   reacquired before returning.  LOCK must be held before calling
   this function.

   The monitor implemented by this function is "Mesa" style, not
   "Hoare" style, that is, sending and receiving a signal are not
   an atomic operation.  Thus, typically the caller must recheck
   the condition after the wait completes and, if necessary, wait
   again.

   A given condition variable is associated with only a single
   lock, but one lock may be associated with any number of
   condition variables.  That is, there is a one-to-many mapping
   from locks to condition variables.

   This function may sleep, so it must not be called within an
   interrupt handler.  This function may be called with
   interrupts disabled, but interrupts will be turned back on if
   we need to sleep. */
void
cond_wait (struct condition *cond, struct lock *lock) {
	struct semaphore_elem waiter;

	ASSERT (cond != NULL);
	ASSERT (lock != NULL);
	ASSERT (!intr_context ());
	ASSERT (lock_held_by_current_thread (lock));

	sema_init (&waiter.semaphore, 0);
	list_insert_ordered (&cond->waiters, &waiter.elem, value_more, 0);
	lock_release (lock);
	sema_down (&waiter.semaphore);
	lock_acquire (lock);
}

/* If any threads are waiting on COND (protected by LOCK), then
   this function signals one of them to wake up from its wait.
   LOCK must be held before calling this function.

   An interrupt handler cannot acquire a lock, so it does not
   make sense to try to signal a condition variable within an
   interrupt handler. */
void
cond_signal (struct condition *cond, struct lock *lock UNUSED) {
	ASSERT (cond != NULL);
	ASSERT (lock != NULL);
	ASSERT (!intr_context ());
	ASSERT (lock_held_by_current_thread (lock));

	if (!list_empty (&cond->waiters))
		sema_up (&list_entry (list_pop_front (&cond->waiters),
					struct semaphore_elem, elem)->semaphore);
}

/* Wakes up all threads, if any, waiting on COND (protected by
   LOCK).  LOCK must be held before calling this function.

   An interrupt handler cannot acquire a lock, so it does not
   make sense to try to signal a condition variable within an
   interrupt handler. */
void
cond_broadcast (struct condition *cond, struct lock *lock) {
	ASSERT (cond != NULL);
	ASSERT (lock != NULL);

	while (!list_empty (&cond->waiters))
		cond_signal (cond, lock);
}
