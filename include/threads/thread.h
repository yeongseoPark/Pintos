#ifndef THREADS_THREAD_H
#define THREADS_THREAD_H

#include <debug.h>
#include <list.h>
#include <stdint.h>
#include "threads/interrupt.h"
#ifdef VM
#include "vm/vm.h"
#endif

// ******************************LINE ADDED****************************** //
//Project 2-2-2 : User Programs - System Call - File Descriptor
#include "threads/synch.h"
#define FDT_PAGES 3
#define FDCOUNT_LIMIT FDT_PAGES *(1 << 9) // Limiting fd_idx
// *************************ADDED LINE ENDS HERE************************* //

/* States in a thread's life cycle. */
// 스레드의 상태를 정의한다. 4가지 상태를 가진다. Running , Ready, Blocked, Dying.
enum thread_status {
	THREAD_RUNNING,     /* Running thread. */
	THREAD_READY,       /* Not running but ready to run. */
	THREAD_BLOCKED,     /* Waiting for an event to trigger. */
	THREAD_DYING        /* About to be destroyed. */
};

/* Thread identifier type. 스레드 타입 정의.
   You can redefine this to whatever type you like. */
typedef int tid_t;
#define TID_ERROR ((tid_t) -1)          /* Error value for tid_t. */

/* Thread priorities. */
// 스레드의 우선순위 범위는 0~63사이로 정의되며 기본값은 31이다.
#define PRI_MIN 0                       /* Lowest priority. */
#define PRI_DEFAULT 31                  /* Default priority. */
#define PRI_MAX 63                      /* Highest priority. */

/* A kernel thread or user process.
 *
 * Each thread structure is stored in its own 4 kB page.  The
 * thread structure itself sits at the very bottom of the page
 * (at offset 0).  The rest of the page is reserved for the
 * thread's kernel stack, which grows downward from the top of
 * the page (at offset 4 kB).  Here's an illustration:
 *
 *      4 kB +---------------------------------+
 *           |          kernel stack           |
 *           |                |                |
 *           |                |                |
 *           |                V                |
 *           |         grows downward          |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           +---------------------------------+
 *           |              magic              |
 *           |            intr_frame           |
 *           |                :                |
 *           |                :                |
 *           |               name              |
 *           |              status             |
 *      0 kB +---------------------------------+
 *
 * The upshot of this is twofold:
 *
 *    1. First, `struct thread' must not be allowed to grow too
 *       big.  If it does, then there will not be enough room for
 *       the kernel stack.  Our base `struct thread' is only a
 *       few bytes in size.  It probably should stay well under 1
 *       kB.
 *
 *    2. Second, kernel stacks must not be allowed to grow too
 *       large.  If a stack overflows, it will corrupt the thread
 *       state.  Thus, kernel functions should not allocate large
 *       structures or arrays as non-static local variables.  Use
 *       dynamic allocation with malloc() or palloc_get_page()
 *       instead.
 *
 * The first symptom of either of these problems will probably be
 * an assertion failure in thread_current(), which checks that
 * the `magic' member of the running thread's `struct thread' is
 * set to THREAD_MAGIC.  Stack overflow will normally change this
 * value, triggering the assertion. */

/* The `elem' member has a dual purpose.  It can be an element in
 * the run queue (thread.c), or it can be an element in a
 * semaphore wait list (synch.c).  It can be used these two ways
 * only because they are mutually exclusive: only a thread in the
 * ready state is on the run queue, whereas only a thread in the
 * blocked state is on a semaphore wait list. */

struct thread {
	/* Owned by thread.c. */
	tid_t tid;                          /* Thread identifier. */
	enum thread_status status;          /* Thread state. */
	char name[16];                      /* Name (for debugging purposes). */
	int priority;                       /* Priority. */

    // ******************************LINE ADDED****************************** //
    // Project 1-1 : Alarm Clock - Busy Waiting -> Sleep-Awake
    // 잠드는 프로세스가 꺠어날 tick을 저장할 변수
    int64_t wakeup_tick;
    // *************************ADDED LINE ENDS HERE************************* //

    /* Shared between thread.c and synch.c. */
    struct list_elem elem;              /* List element. */

    // ******************************LINE ADDED****************************** //
    // Project 1-2.3 : Priority Inversion Problem - Priority Donation
    int init_priority; // 최초에 할당받은 Priority를 담는 변수

    struct lock *wait_on_lock; // 해당 스레드가 대기하고 있는 LOCK을 담아두는 LOCK 구조체 변수
    struct list donations; // for Multiple Donation - Doner List
    struct list_elem donation_elem; // for Multiple Donation - Donor Thread
    // *************************ADDED LINE ENDS HERE************************* //



    // ******************************LINE ADDED****************************** //
    // Project 2-2-1: User Programs - System Call - Basics
    int exit_status; // System Call 구현시 상태 체크 위한 플래그 변수. Used in userprog/syscall.c

    struct intr_frame parent_if;

    struct list child_list;
    struct list_elem child_elem;

    struct semaphore wait_sema;
    struct semaphore fork_sema;
    struct semaphore free_sema;

    // Project 2-2-2 : User Programs - System Call - File Descriptor
    struct file **fd_table;
    int fd_idx;

    int stdin_count;
    int stdout_count;

    struct file *running;
    // *************************ADDED LINE ENDS HERE************************* //

#ifdef USERPROG
	/* Owned by userprog/process.c. */
	uint64_t *pml4;                     /* Page map level 4 */
#endif
#ifdef VM
	/* Table for whole virtual memory owned by thread. */
	struct supplemental_page_table spt;
#endif

	/* Owned by thread.c. */
	struct intr_frame tf;               /* Information for switching */
	unsigned magic;                     /* Detects stack overflow. */
};

/* If false (default), use round-robin scheduler.
   If true, use multi-level feedback queue scheduler.
   Controlled by kernel command-line option "-o mlfqs". */
extern bool thread_mlfqs;

void thread_init (void);
void thread_start (void);

void thread_tick (void);
void thread_print_stats (void);

typedef void thread_func (void *aux);
tid_t thread_create (const char *name, int priority, thread_func *, void *);

void thread_block (void);
void thread_unblock (struct thread *);

struct thread *thread_current (void);
tid_t thread_tid (void);
const char *thread_name (void);

void thread_exit (void) NO_RETURN;
void thread_yield (void);

int thread_get_priority (void);
void thread_set_priority (int);

int thread_get_nice (void);
void thread_set_nice (int);
int thread_get_recent_cpu (void);
int thread_get_load_avg (void);

void do_iret (struct intr_frame *tf);

// ******************************LINE ADDED****************************** //
// Project 1-1 : Alarm Clock - Busy Waiting -> Sleep-Awake
void thread_sleep(int64_t ticks);
void thread_awake(int64_t ticks);
void update_next_tick_to_awake(int64_t ticks);
int64_t get_next_tick_to_awake(void);

// Project 1-2.1 : Thread - RoundRobin Scheduling -> Priority Scheduling
void test_max_priority(void);
bool cmp_priority(const struct list_elem *target, const struct list_elem *compare, void *aux UNUSED);

// Project 1-2.2 : Thread - Priority Scheduling and Synchronization
// LOCK, Semaphore, Condition Variable
bool cmp_sema_priority(const struct list_elem *a, const struct list_elem *b, void *aux UNUSED);

// Project 1-2.3 : Priority Inversion Problem - Priority Donation
void donate_priority(void);
void remove_with_lock(struct lock *lock);
void refresh_priority(void);
bool cmp_donation_list_priority (const struct list_elem *a, const struct list_elem *b, void *aux UNUSED);
// *************************ADDED LINE ENDS HERE************************* //

#endif /* threads/thread.h */
