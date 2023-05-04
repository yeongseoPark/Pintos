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

/*Initializes semaphore SEMA to VALUE.  A semaphore is a nonnegative integer
 * along with two atomic operators for manipulating it:
 * 세마포어를 초기화 하는 함수, 매개변수로 받은 세마포어를 waiters 리스트에 넣는다.
 *
 * - down or "P": wait for the value to become positive, then decrement it.
 *
 * - up or "V": increment the value (and wake up one waiting thread, if any). */
void sema_init (struct semaphore *sema, unsigned value) {
	ASSERT (sema != NULL);

	sema->value = value;
	list_init (&sema->waiters);
}

/*Down or "P" operation on a semaphore.  Waits for SEMA's value to become positive and then atomically decrements it.
 * This function may sleep, so it must not be called within an interrupt handler.
 * This function may be called with interrupts disabled, but if it sleeps then
 * the next scheduled thread will probably turn interrupts back on. This is sema_down function.
 * 세마 포어를 위한 P연산 함수. 세마포어의 value값이 양수가 될 때 까지 기다렸다가 세마포어의 value 값을 1씩 감소시킨다.
 * 즉, 자원 선점을 선언하는 상태로 만듦.
 *  */
void sema_down (struct semaphore *sema) {
	enum intr_level old_level;

	ASSERT (sema != NULL);
	ASSERT (!intr_context());

	old_level = intr_disable();
	while (sema->value == 0) {
        // ******************************LINE MODDED****************************** //
        // Project 1-2.2 : Thread - Priority Scheduling and Synchronization
        // LOCK, Semaphore, Condition Variable

        /*list_push_back(&sema->waiters, &thread_current ()->elem);*/
        list_insert_ordered(&sema->waiters, &thread_current()->elem, &cmp_priority, NULL);
        // *************************MODDED LINE ENDS HERE************************* //
		thread_block();
	}
	sema->value--;
	intr_set_level(old_level);
}

/* Down or "P" operation on a semaphore, but only if the
   semaphore is not already 0.  Returns true if the semaphore is
   decremented, false otherwise.

   This function may be called from an interrupt handler. */
bool sema_try_down (struct semaphore *sema) {
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

/* Up or "V" operation on a semaphore. Increments SEMA's value and wakes up one thread
 * of those waiting for SEMA, if any. This function may be called from an interrupt handler.
 * 세마포어를 위한 V 연산 함수. 세마포어의 value 값을 증가시키고, waiters 리스트에 있는 다음 스레드를 꺠운다.
 * 즉, 공유 자원의 반납을 선언하는 함수. 이 함수는 인터럽트 핸들러에 의해 호출 될 수 있다.(Preemtive 위해??)
 * */
void sema_up(struct semaphore *sema) {
	enum intr_level old_level;

	ASSERT (sema != NULL); // 세마포어가 선언되어 있지 않은 경우를 위한 예외 처리

	old_level = intr_disable(); // 인터럽트 OFF

    // wiater 리스트가 비어있지 않으면 = 즉, 세마포어를 기다리는 다음 스레드가 있으면
    if (!list_empty(&sema->waiters)){
        // ******************************LINE ADDED****************************** //
        // Project 1-2.2 : Thread - Priority Scheduling and Synchronization
        // LOCK, Semaphore, Condition Variable
        list_sort(&sema->waiters, &cmp_priority, NULL);
        // *************************ADDED LINE ENDS HERE************************* //
		thread_unblock(list_entry(list_pop_front(&sema->waiters), struct thread, elem));
    }
	sema->value++; // 세마포어 값 증가시켜줌

    // ******************************LINE ADDED****************************** //
    // Project 1-2.2 : Thread - Priority Scheduling and Synchronization
    // LOCK, Semaphore, Condition Variable
    /*printf("test_max_priority function called in sema_up\n"); //Debugging Project 2 : User Programs - Argument Passing*/
    test_max_priority();
    // *************************ADDED LINE ENDS HERE************************* //

	intr_set_level (old_level); // 인터럽트 ON
}

static void sema_test_helper (void *sema_);

/* Self-test for semaphores that makes control "ping-pong"
   between a pair of threads.  Insert calls to printf() to see
   what's going on. */
void sema_self_test (void) {
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
static void sema_test_helper (void *sema_) {
	struct semaphore *sema = sema_;
	int i;

	for (i = 0; i < 10; i++)
	{
		sema_down (&sema[0]);
		sema_up (&sema[1]);
	}
}

/* @threads/build/synch.c
 * Initializes LOCK.  A lock can be held by at most a single thread at any given time.
 * Our locks are not "recursive", that is, it is an error for the thread currently holding a lock
 * to try to acquire that lock.
 * LOCK 을 초기화 하는 함수. LOCK 은 한번에 한 스레드에게만 주어지고 소유될수 있다.
 * PintOS 에서 LOCK은 "재귀적"이지 않기 때문에, 어떤 스레드가 LOCK을 획득하기 위해 LOCK을 가지고 있어야 한다는 것은 말이 되지 않음.
 *
 * A lock is a specialization of a semaphore with an initial value of 1.
 * The difference between a lock and such a semaphore is twofold.
 * LOCK 은 세마포어가 1로 초기화 되는 특수한 세마포어이다.(LOCK = Binary Semaphore ??)
 * 하지만 두가지 관점에서 LOCK과 세마포어는 다르다.
 *
 * First, a semaphore can have a value greater than 1, but a lock can only be owned by
 * a single thread at a time.
 * 첫째로, 세마포어는 value가 1보다 큰 값을 가질 수 있다, 그러나, LOCK은 한번에 한 스레드밖에 소유하지 못한다.
 *
 * Second, a semaphore does not have an owner, meaning that one thread can "down" the semaphore
 * and then another one "up" it,
 * 둘째로, 세마포어는 특정한 소유권을 가지지 않는다. 즉, 아무 스레드나 세마포어 값을 증가, 혹은 감소 시킬 수 있다.
 * but with a lock the same thread must both acquire and release it.
 * 그러나 LOCK은 LOCK을 획득하고, 해당 LOCK을 반납하는 스레드는 동일해야 한다.
 * When these restrictions prove onerous, it's a good sign that a semaphore should be used, instead of a lock.
 * 이러한 과정(LOCK을 하나의 스레드가 획득하고 다시 반납하는)이 번거로운 상황에서는 LOCK 대신 세마포어를 사용하는것이 좋다.
 * */
void lock_init (struct lock *lock) {
	ASSERT (lock != NULL);

	lock->holder = NULL;
	sema_init (&lock->semaphore, 1);
}

// TODO
// 인터럽트 핸들러와 MUTEX간의 관계, 개념 재정리. 뭔가 Deadlock 유발 관련해서 서로 제약사항이 있는 것 같은데
// 인터럽트 핸들러의 개념을 아직 정확히 잘 모르겠음

/* Acquires LOCK, sleeping until it becomes available if necessary.
 * LOCK을 획득하는 함수, 당장 획득이 불가능하면 필요시까지 Sleep 상태로 존버.
 * The lock must not already be held by the current thread.
 * 획득하려고 하는 스레드는 이미 LOCK 을 가지고 있지 않아야 한다.
 * This function may sleep, so it must not be called within an interrupt handler.
 * This function may be called with interrupts disabled, but interrupts will be turned back on
 * if we need to sleep. */
void lock_acquire (struct lock *lock) {
	ASSERT (lock != NULL);
	ASSERT (!intr_context ());
	ASSERT (!lock_held_by_current_thread (lock));

    // ******************************LINE ADDED****************************** //
    // Project 1-2.3 : Priority Inversion Problem - Priority Donation
    // LOCK이 Holder를 가지고 있는지. 즉, LOCK이 점유되고 있는지 Check.
    // 다른 스레드가 LOCK을 점유하고 있으면 자신의 Priority를 Donation 하여
    // 현재 LOCK을 점유하는 스레드가 우선적으로 LOCK을 반환하도록 한다.
    // !CAUTION : lock_acquire 함수 내에서는 thread_current()는 LOCK을 얻고자 하는 스레드를 current로 취급한다.
    //            또한, lock_acquire 함수를 호출 할 수 있었다는 것은 지금 lock->holder 스레드 보다 priority가 높다는 것을 의미
    //            따라서 우선 순위의 대소 비교를 할 필요가 없다.

    /*if (lock -> holder != NULL){*/
    if (lock->holder){
        thread_current() -> wait_on_lock = lock; // 현재 스레드가 LOCK을 기다리고 있다고 알려준다.
        // donations 리스트에 넣어줄때는 FIFO(기부 순서)가 아닌 priority순으로 정렬하여 삽입
        // -> donor들이 나갈때도 priority 순으로 나가기 때문에.

        list_insert_ordered(&lock->holder->donations, &thread_current()->donation_elem, &cmp_donation_list_priority, NULL);
        /*list_push_back(&lock->holder->donations, &curr->donation_elem);*/
        donate_priority();
    }
    // *************************ADDED LINE ENDS HERE************************* //

	sema_down (&lock->semaphore);
    lock->holder = thread_current();

    // ******************************LINE ADDED****************************** //
    // Project 1-2.3 : Priority Inversion Problem - Priority Donation
    thread_current() -> wait_on_lock = NULL;
    // *************************ADDED LINE ENDS HERE************************* //
}

/* Tries to acquires LOCK and returns true if successful or false on failure.
 * The lock must not already be held by the current thread.
 * This function will not sleep, so it may be called within an interrupt handler. */
bool lock_try_acquire (struct lock *lock) {
	bool success;

	ASSERT (lock != NULL);
	ASSERT (!lock_held_by_current_thread (lock));

	success = sema_try_down (&lock->semaphore);
	if (success)
		lock->holder = thread_current();
	return success;
}

/* Releases LOCK, which must be owned by the current thread.
 * LOCK을 반환하는 함수, 현재 돌고 있는 스레드가 소유하고 있어야 한다.
 * This is lock_release function.
 * An interrupt handler cannot acquire a lock, so it does not make sense to try to release
 * a lock within an interrupt handler. */
void lock_release (struct lock *lock) {
	ASSERT (lock != NULL);
	ASSERT (lock_held_by_current_thread (lock));

    // ******************************LINE ADDED****************************** //
    // Project 1-2.3 : Priority Inversion Problem - Priority Donation
    // Function declared in thread.c
    remove_with_lock(lock);
    refresh_priority();

    lock->holder = NULL;
    // *************************ADDED LINE ENDS HERE************************* //
    /*printf("sema_up called in lock_release function\n"); //Debugging Project 2 : User Programs - Argument Passing*/
	sema_up (&lock->semaphore);
}

/* Returns true if the current thread holds LOCK, false
   otherwise.  (Note that testing whether some other thread holds
   a lock would be racy.) */
bool lock_held_by_current_thread (const struct lock *lock) {
	ASSERT (lock != NULL);

	return lock->holder == thread_current();
}

/* One semaphore in a list. */
struct semaphore_elem {
	struct list_elem elem;              /* List element. */
	struct semaphore semaphore;         /* This semaphore. */
};

/* Initializes condition variable COND.  A condition variable
   allows one piece of code to signal a condition and cooperating
   code to receive the signal and act upon it. */
void cond_init (struct condition *cond) {
	ASSERT (cond != NULL);

	list_init (&cond->waiters);
}

// cond_wait, cond_signal 또한 세마포어 에서 처럼 waiters 리스트를 가지지만,
// 세마포어에서의 waiters 리스트는 쓰레드들의 리스트인 반면
// 조건변수를 다루는 함수들에서의 waiters 리스트는 "세마포어"의 리스트이다.

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

void cond_wait (struct condition *cond, struct lock *lock) {
	struct semaphore_elem waiter;

	ASSERT (cond != NULL);
	ASSERT (lock != NULL);
	ASSERT (!intr_context ());
	ASSERT (lock_held_by_current_thread (lock));

	sema_init (&waiter.semaphore, 0);
    // ******************************LINE MODDED****************************** //
    // Project 1-2.2 : Thread - Priority Scheduling and Synchronization
    // LOCK, Semaphore, Condition Variable
	/*list_push_back (&cond->waiters, &waiter.elem);*/
    waiter.semaphore.priority = thread_current()->priority;
    list_insert_ordered(&cond->waiters, &waiter.elem, &cmp_sema_priority, NULL);
    // *************************ADDED LINE ENDS HERE************************* //
	lock_release(lock);
	sema_down(&waiter.semaphore);
	lock_acquire(lock);
}

/* If any threads are waiting on COND (protected by LOCK), then
   this function signals one of them to wake up from its wait.
   LOCK must be held before calling this function.

   An interrupt handler cannot acquire a lock, so it does not
   make sense to try to signal a condition variable within an
   interrupt handler. */
void cond_signal (struct condition *cond, struct lock *lock UNUSED) {
	ASSERT (cond != NULL);
	ASSERT (lock != NULL);
	ASSERT (!intr_context ());
	ASSERT (lock_held_by_current_thread (lock));

	if (!list_empty(&cond->waiters)){
        // ******************************LINE ADDED****************************** //
        // Project 1-2.2 : Thread - Priority Scheduling and Synchronization
        // LOCK, Semaphore, Condition Variable
        /*list_sort(&cond -> waiters, &cmp_sema_priority, NULL);*/
        // *************************ADDED LINE ENDS HERE************************* //
		sema_up(&list_entry (list_pop_front (&cond->waiters), struct semaphore_elem, elem)->semaphore);
    }
}

/* Wakes up all threads, if any, waiting on COND (protected by
   LOCK).  LOCK must be held before calling this function.

   An interrupt handler cannot acquire a lock, so it does not
   make sense to try to signal a condition variable within an
   interrupt handler. */
void cond_broadcast (struct condition *cond, struct lock *lock) {
	ASSERT (cond != NULL);
	ASSERT (lock != NULL);

	while (!list_empty (&cond->waiters))
		cond_signal (cond, lock);
}

// ******************************LINE ADDED****************************** //
// Project 1-2.2 : Thread - Priority Scheduling and Synchronization
// LOCK, Semaphore, Condition Variable
bool cmp_sema_priority (const struct list_elem *a, const struct list_elem *b, void *aux UNUSED)
{
    struct semaphore_elem *sema_a = list_entry(a, struct semaphore_elem, elem);
    struct semaphore_elem *sema_b = list_entry(b, struct semaphore_elem, elem);

    /*struct list_elem *sa_e = list_begin(&(sa->semaphore.waiters));
    struct list_elem *sb_e = list_begin(&(sb->semaphore.waiters));

    struct thread *sa_t = list_entry(sa_e, struct thread, elem);
    struct thread *sb_t = list_entry(sb_e, struct thread, elem);

    return (sa_t->priority) > (sb_t->priority);*/

    return sema_a->semaphore.priority > sema_b->semaphore.priority;
}
// *************************ADDED LINE ENDS HERE************************* //
