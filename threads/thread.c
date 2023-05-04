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

// Project 1-2.3 : Priority Inversion Problem - Priority Donation
/* Based on gitbook PROJECT 1 : THREADS - Priority Scheduling
 * "Implement priority donation. You will need to account for all different situations
 * in which priority donation is required. Be sure to handle multiple donations,
 * in which multiple priorities are donated to a single thread.
 * You must also handle nested donation: if H is waiting on a lock that M holds and M is waiting on a lock
 * that L holds, then both M and L should be boosted to H's priority.
 * If necessary, you may impose a reasonable limit on depth of nested priority donation, such as 8 levels.*/
#define NESTING_DEPTH 8

/* Random value for struct thread's `magic' member.
   Used to detect stack overflow.  See the big comment at the top
   of thread.h for details. */
#define THREAD_MAGIC 0xcd6abf4b

/* Random value for basic thread
   Do not modify this value. */
#define THREAD_BASIC 0xd42df210

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
#define TIME_SLICE 4            /* # of timer ticks to give each thread. */
static unsigned thread_ticks;   /* # of timer ticks since last yield. */

/* If false (default), use round-robin scheduler.
   If true, use multi-level feedback queue scheduler.
   Controlled by kernel command-line option "-o mlfqs". */
bool thread_mlfqs;

static void kernel_thread(thread_func *, void *aux);

static void idle(void *aux UNUSED);

static struct thread *next_thread_to_run(void);

static void init_thread(struct thread *, const char *name, int priority);

static void do_schedule(int status);

static void schedule(void);

static tid_t allocate_tid(void);

// ******************************LINE ADDED****************************** //
// Project 1-1 : Alarm Clock - Busy Waiting -> Sleep-Awake

// sleep_list 구조체는 Sleep 상태에 있는 스레드들을 저장해두는 리스트 구조체
static struct list sleep_list;

// next_tick_to_awake 변수는 sleep_list에서 대기중인 스레드들의
// wakeup_tick중 최솟값을 저장 즉, 다음에 wake 시켜야 할 tick 을 알려준다.
// 한 마디로 다음 알람 시간 설정!
// Fallback https://stackoverflow.com/questions/12468281/should-i-use-long-long-or-int64-t-for-portable-code
static int64_t next_tick_to_awake;
//static long long next_tick_to_awake;

// *************************ADDED LINE ENDS HERE************************* //

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
static uint64_t gdt[3] = {0, 0x00af9a000000ffff, 0x00cf92000000ffff};

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
void thread_init(void) {
    ASSERT(intr_get_level() == INTR_OFF);

    /* Reload the temporal gdt for the kernel
     * This gdt does not include the user context.
     * The kernel will rebuild the gdt with user context, in gdt_init (). */
    struct desc_ptr gdt_ds = {
            .size = sizeof(gdt) - 1,
            .address = (uint64_t) gdt
    };
    lgdt(&gdt_ds);

    // ******************************LINE ADDED****************************** //
    // Project 1-1 : Alarm Clock - Busy Waiting -> Sleep-Awake
    // sleep_list 초기화
    list_init (&sleep_list);
    next_tick_to_awake = INT64_MAX;
    // *************************ADDED LINE ENDS HERE************************* //

    /* Init the globla thread context */
    lock_init(&tid_lock);
    list_init(&ready_list);
    list_init(&destruction_req);

    /* Set up a thread structure for the running thread. */
    initial_thread = running_thread ();
    init_thread(initial_thread, "main", PRI_DEFAULT);
    initial_thread->status = THREAD_RUNNING;
    /*printf("allocate_tid called in thread_init\n"); //Debugging Project 2 : User Programs - Argument Passing*/
    initial_thread->tid = allocate_tid();
}

// ******************************LINE ADDED****************************** //
// Project 1-1 : Alarm Clock - Busy Waiting -> Sleep-Awake
// 다음 Wake 해야 할 시간을 업데이트 해주는 함수
void update_next_tick_to_awake(int64_t ticks){
    // next_tick_to_awake 변수는 다음에 Wake 해야 할 시간까지 offset을 저장하는 변수이다.
    // 즉, 깨워야 할 스레드의 tick중 가장 작은 값을 갖도록 한다.
    next_tick_to_awake = (next_tick_to_awake > ticks) ? ticks : next_tick_to_awake;
    // Reference : MIN Function Explained
    // https://linuxhint.com/min-function-c/
    // ? 연산자 설명
    // condition ? return_value_if_true : value_if_false
}

// Project 1-1 : Alarm Clock - Busy Waiting -> Sleep-Awake
// next_tick_to_awake 반환 함수
int64_t get_next_tick_to_awake(void){
    return next_tick_to_awake;
}

 /* 스레드를 sleep_list 에 삽입하고 blocked 상태로 만들어 대기상태로 재우는 함수.
  * 단, 현재 스레드가 idle 스레드가 아닌 경우여야 한다.
  * 스레드의 상태를 BLOCKED로 바꾸고 깨어나야 할 ticks을 저장해준다(알람 맞춰주고 재우기!)
  * sleep_list 에 삽입 후, awake 함수를 위한 tick update
  * >>>>> 질문사항 */
//     Q1. 찾아본 대부분의 레퍼런스들이 스레드를 Blocked (AKA.Sleep) 상태로 만들기 전에
//         스레드가 idle 스레드가 아닌지 체크하는 조건분기를 걸어두는데
//         왜 idle 스레드는 재우지 않아야 하는지 궁금합니다.
//
//         >> A1. 조교 답변
//         Idle이 도입된 이유는 CPU가 작업을 하고 있는지 아닌지 구분하는 로직을 만드는 것보다,
//         "아무것도 하고 있지 않음"을 표현하는 스레드인 idle을 도입하는 것이 스레드를 더 간단하게 관리할 수 있기 때문입니다.
//         즉 Idle은 "CPU가 쉬고 있음"을 표현하는 스레드이므로, block된다는 것이 말이 되지 않을 것 같습니다.
//
//         >> A1. 백승현 코치님 답변
//         idle thread는 일종의 default thread인데, 그 외의 다른 스레드들이 다 blocked 상태일 때 돌아야하는 스레드가 있어야 하기 때문입니다.
//         OS는 계속해서 동작하고 있어야 할 필요가 있을 거라 idle thread 라도 ready인 상태로 있어야 한다고 생각합니다.
//         PintOS 에서는 idle thread의 경우 idle_tick++을 하고 있습니다. thread_print_stats에서 tick을 출력하고 있습니다.
//         OS가 작동한 시간 같은 걸 알려 주는 용도인 듯 합니다.
//
//     Q2. 스켈레톤 코드 @thread/threads.c 의 do_schedule() 함수의 Comment 도 그렇고
//         찾아본 레퍼런스 코드들에도 그렇고 스레드의 상태 변경에 개입하는 함수 구획안에서는
//         인터럽트를 disable 시켜 두는데 해당 부분 관련해서 정확한 이유와 더 자세한 부분을 알고 싶습니다.
//
//         >> A2. 조교 답변
//         스레드의 상태를 변경하는 동작은 중간에 interrupt로 인해 방해되지 않아야 합니다 (atomic하게 일어나야 합니다).
//         그렇지 않으면 race condition이 발생해 스레드가 예기치 않은 동작을 할 수 있습니다.
//
//         >> A2. 백승현 코치님 답변
//         atomic하게 돌아야 하는 로직은 interrupt를 disable하는데요. do_schedule 함수의 destruction_req 리스트는
//         schedule 함수에서도 사용되는데 인터럽트가 disable되지 않는다면
//         schedule함수로 인해 destruction_req의 무결성이 보장되지 않을 수 있을 거 같습니다.
//
//         + Q2-1. do_schedule() 함수에 Comment 에서 명시적으로 해당 함수 내부에서
//                 printf() 함수 사용을 권장하지 않고 있는데 이는 printf() 함수 호출시 자체적으로
//                 인터럽트 발생 여지가 있어서 사용하지 말라고 하는건지? 아니면 I/O 관련 함수라 전체 성능에
//                 병목을 주기 때문인지가 궁금합니다.
//
//                 >> A2-1. 조교 답변
//                    printf 함수는 내부적으로 출력과 관련된 semaphore를 조작하고 있기 때문에
//                    다시 do_schedule을 호출하여 무한루프를 일으킬 위험이 있습니다.

// Project 1-1 : Alarm Clock - Busy Waiting -> Sleep-Awake
void thread_sleep(int64_t ticks){
    struct thread *cur;
    enum intr_level old_level;

    // thread_sleep 함수 중에는 인터럽트를 비활성화 시킨다.
    // old_level 에 이전 인터럽트 레벨 저장
    old_level = intr_disable();

    // ******************************INTERRUPT DISABLED****************************** //
    cur = thread_current();
    // ASSERT FLAG : idle_thread는 재우면 안된다!
    ASSERT(cur != idle_thread);
    // 예외 처리!
    if (cur != idle_thread){
        update_next_tick_to_awake(cur -> wakeup_tick = ticks);
        list_push_back(&sleep_list, &cur->elem);
    }

    // 스레드 Blocked 상태로 변경
    /*thread_block();*/
    do_schedule(THREAD_BLOCKED);

    // 인터럽트 재 허용 : 이전 인터럽트 레벨 복구
    intr_set_level(old_level);
    // ******************************INTERRUPT RE-ENABLED****************************** //
}

// Project 1-1 : Alarm Clock - Busy Waiting -> Sleep-Awake
// sleep_list를 순회하며 sleep 중인 스레드 중 wakeup_tick 이 지난 스레드를 wake 하는 함수
void thread_awake(int64_t wakeup_tick){
    next_tick_to_awake = INT64_MAX;
    struct list_elem *e;
    e = list_begin(&sleep_list);
    while(e != list_end(&sleep_list)){
        struct thread * t = list_entry(e, struct thread, elem);

        // 해당 스레드의 tick이 깨워야 할 tick보다 작거나 같으면 깨워야 하므로,
        // sleep_list 에서 제거하고, 스레드 Block을 해제해준다.
        if(wakeup_tick >= t->wakeup_tick){
            e = list_remove(&t->elem);
            thread_unblock(t);
        }
        // 현재 tick이 깨워야 할 tick보다 크다면, 더 재워도 된다.
        // update_next_tick_to_awake 함수 호출 하여, next_tick_to_awake 변수 갱신해줌.
        else{
            e = list_next(e);
            update_next_tick_to_awake(t->wakeup_tick);
        }
    }
}

// Project 1-2.1 : Thread - RoundRobin Scheduling -> Priority Scheduling
// -> Test Case
// threads/priority-change
// threads/priority-fifo
// threads/priority-preempt
void test_max_priority(void){
    // Debugging Project 2 : User Programs - Argument Passing
    /*printf("test_max_priority function called in somewhere\n");*/
    // 대기열이 비었을때 예외처리
    // intr_context OR 연산 통해 예외처리 없을시 userprog에서 Kernel Panic
    // main(AKA.PintOS Main)@init.c -> thread_init -> allocate_tid
    // -> lock_release -> sema_up -> test_max_priority 순으로 호출
    if (list_empty(&ready_list) || intr_context()){
        return;
    }

    struct list_elem *e = list_begin(&ready_list);
    struct thread *curr = list_entry(e, struct thread, elem);

    // 새로 들어온 프로세스 우선순위가, 지금 돌고있는 프로세스 우선순위 보다 높다면
    if (curr->priority > thread_get_priority()) {
        thread_yield();
    }
    /*if (!list_empty (&ready_list) && thread_current ()->priority
            < list_entry (list_front (&ready_list), struct thread, elem)->priority){
        thread_yield ();
    }*/
}

// Project 1-2.1 : Thread - RoundRobin Scheduling -> Priority Scheduling
// Compare Priority, 매개변수로 받은 스레드 두개를 비교하여,
// 대상(target)스레드 우선순위가 더 높으면 TRUE(1)을 반환
// 비교(compare)스레드 우선순위가 더 높으면 FALSE(0)을 반환
bool cmp_priority(const struct list_elem *target, const struct list_elem *compare, void *aux UNUSED){
    struct thread *target_thread;
    struct thread *compare_thread;

    target_thread = list_entry(target, struct thread, elem);
    compare_thread = list_entry(compare, struct thread, elem);

    // target의 우선순위가 compare 보다 크면 TURE (1)
    // target의 우선순위가 compare 작거나 같으면 FALSE (0)
    return((target_thread->priority)>(compare_thread->priority)) ? true:false;
}
// *************************ADDED LINE ENDS HERE************************* //

/* Starts preemptive thread scheduling by enabling interrupts.
   Also creates the idle thread. */
void thread_start(void) {
    /* Create the idle thread. */
    struct semaphore idle_started;
    sema_init(&idle_started, 0);
    thread_create("idle", PRI_MIN, idle, &idle_started);

    /* Start preemptive thread scheduling. */
    intr_enable();

    /* Wait for the idle thread to initialize idle_thread. */
    sema_down(&idle_started);
}

/* Called by the timer interrupt handler at each timer tick.
   Thus, this function runs in an external interrupt context. */
void thread_tick(void) {
    struct thread *t = thread_current();

    /* Update statistics. */
    if (t == idle_thread)
        idle_ticks++;
#ifdef USERPROG
        else if (t->pml4 != NULL)
            user_ticks++;
#endif
    else
        kernel_ticks++;

    /* Enforce preemption. */
    if (++thread_ticks >= TIME_SLICE)
        intr_yield_on_return();
}

/* Prints thread statistics. */
void thread_print_stats(void) {
    printf("Thread: %lld idle ticks, %lld kernel ticks, %lld user ticks\n",
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
tid_t thread_create(const char *name, int priority, thread_func *function, void *aux) {
    struct thread *t; // 새로 생성된 스레드

    // ******************************LINE ADDED****************************** //
    // Project 1-2.1 : Thread - RoundRobin Scheduling -> Priority Scheduling
    struct thread *now_running = thread_current(); // 지금 돌고 있는 스레드
    // *************************ADDED LINE ENDS HERE************************* //

    tid_t tid;

    ASSERT(function != NULL);

    /* Allocate thread. */
    t = palloc_get_page(PAL_ZERO);
    if (t == NULL)
        return TID_ERROR;

    /* Initialize thread. */
    // Project 1-1 : Alarm Clock - Busy Waiting -> Sleep-Awake
    // 우선순위를 매개변수로 넘겨준다.
    // 스레드의 우선순위 범위는 0~63사이로 정의되며 기본값은 31이다.
    // Defined @ thread.h as PRI_MIN, PRI_DEFAULT, PRI_MAX
    init_thread(t, name, priority);
    tid = t->tid = allocate_tid();

    // ******************************LINE ADDED****************************** //
    // Project 2-2 : User Programs - System Call

    /* 1) 부모 프로세스 저장
     * 2) 프로그램이 로드되지 않음
     * 3) 프로세스가 종료되지 않음
     * 4) exit 세마포어 0으로 초기화 -> init_thread
     * 5) load 세마포어 0으로 초기화 -> init_thread
     * 6) 자식 리스트에 추가*/

    /*struct thread *parent;
    parent = thread_current();*/
    list_push_back(&now_running->child_list,&t->child_elem); // parent child 리스트에 생성한 child를 담는다

    /* project 2 : File Descriptor */
    /*
     * 1) fd 값 초기화(0,1은 표준 입력, 출력) -> fdtable배열로 받았으므로 바로 값 넣어주면된다.
     * 2) File Descriptor 테이블에 메모리 할당
    */

    t->fd_table = palloc_get_multiple(PAL_ZERO, FDT_PAGES);
    if(t->fd_table == NULL)
        return TID_ERROR;

    t->fd_table[0] = 1;
    t->fd_table[1] = 2;
    t->fd_idx = 2; // idx 2로 초기화

    //count 초기화
    t->stdin_count = 1;
    t->stdout_count = 1;

    // *************************ADDED LINE ENDS HERE************************* //

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
    thread_unblock(t);

    // ******************************LINE ADDED****************************** //
    // Project 1-2.1 : Thread - RoundRobin Scheduling -> Priority Scheduling
    // compare the priorities of the currently running thread and the newly inserted one.
    // Yield the CPU if the newly arriving thread has higher priority
    // TODO
    //      void thread_unblock(struct thread *t) - DONE
    //          -> When the thread is unblocked, it is inserted to ready_list in the priority order.
    //          -> HINT
    //               When unblocking a thread, use "list_insert_ordered" instead of "list_push_back"
    //               -> list_insert_ordered 함수 만들기
    //      void thread_yield(void) - DONE
    //          -> The current thread yields CPU and it is inserted to ready_list in priority order.
    //      void thread_set_priority(int new_priority) - DONE
    //          -> Set priority of the current thread.
    //          -> Reorder the ready_list
    // 생성된 스레드(t), 지금 돌고 있는 스레드(now_running)비교
    // 새로 생성된 스레드가 우선순위 더 높으면 if문 TRUE -> Yield
    if (cmp_priority(&t->elem, &now_running->elem, 0)) {
            thread_yield();
    }
    // *************************ADDED LINE ENDS HERE************************* //

    return tid;
}

/* Puts the current thread to sleep.  It will not be scheduled
   again until awoken by thread_unblock().

   This function must be called with interrupts turned off.  It
   is usually a better idea to use one of the synchronization
   primitives in synch.h. */
void thread_block(void) {
    ASSERT(!intr_context());
    ASSERT(intr_get_level() == INTR_OFF);
    thread_current()->status = THREAD_BLOCKED;
    schedule();
}

/* Transitions a blocked thread T to the ready-to-run state.
   This is an error if T is not blocked.  (Use thread_yield() to
   make the running thread ready.)

   This function does not preempt the running thread.  This can
   be important: if the caller had disabled interrupts itself,
   it may expect that it can atomically unblock a thread and
   update other data. */
void thread_unblock(struct thread *t) {
    enum intr_level old_level;

    ASSERT(is_thread (t));

    old_level = intr_disable();
    ASSERT(t->status == THREAD_BLOCKED);

    // ******************************LINE MODDED****************************** //
    // Project 1-2.1 : Thread - RoundRobin Scheduling -> Priority Scheduling
    // 우선순위 스케쥴링 구현위해 기존에 무조건 ready_list 맨 마지막에 넣는 list_push_back 함수 Block Comment 처리
    // 우선 순위 고려한 list_insert_ordered로 교체
    // Defined @ lib/kernel/list.c

    /*list_push_back(&ready_list, &t->elem);*/
    list_insert_ordered(&ready_list, &t->elem, &cmp_priority, 0);
    // *************************MODDED LINE ENDS HERE************************* //

    t->status = THREAD_READY;
    intr_set_level(old_level);
}

// ******************************LINE ADDED****************************** //
// Project 1-2.3 : Priority Inversion Problem - Priority Donation
// LOCK donation 과정중 donations 리스트에 들어가는 donation_elem 에 대한 compare 함수
bool cmp_donation_list_priority (const struct list_elem *a, const struct list_elem *b, void *aux UNUSED){
    return (list_entry(a, struct thread, donation_elem)->priority)
    > (list_entry(b, struct thread, donation_elem)->priority);
}

// Project 1-2.3 : Priority Inversion Problem - Priority Donation
// 우선순위 Donation 함수 (key holder to current thread)
void donate_priority(){
    int depth;
    struct thread *curr = thread_current();

    /* Based on gitbook PROJECT 1 : THREADS - Priority Scheduling
     * "Implement priority donation. You will need to account for all different situations
     * in which priority donation is required. Be sure to handle multiple donations,
     * in which multiple priorities are donated to a single thread.
     * You must also handle nested donation: if H is waiting on a lock that M holds and M is waiting on a lock
     * that L holds, then both M and L should be boosted to H's priority.
     * If necessary, you may impose a reasonable limit on depth of nested priority donation, such as 8 levels.*/

    for (depth = 0; depth < NESTING_DEPTH; depth++){
        /*if (!curr -> wait_on_lock){*/
        if (curr->wait_on_lock == NULL){
            break;
        }
        /*struct thread *holder = curr -> wait_on_lock -> holder;
        holder -> priority = curr -> priority;
        curr = holder;*/
       /* curr = curr->wait_on_lock->holder;
        curr -> priority = curr_priority;*/

        curr -> wait_on_lock -> holder -> priority = curr -> priority;
        curr = curr -> wait_on_lock -> holder;
    }
}

// Project 1-2.3 : Priority Inversion Problem - Priority Donation
void remove_with_lock(struct lock *lock){
    struct thread *curr = thread_current ();
    /*struct list_elem *e;*/
    struct list_elem *e = list_begin(&curr -> donations);

    for (e; e != list_end((&curr -> donations));){
        struct thread *curr = list_entry(e, struct thread, donation_elem);
        if (curr -> wait_on_lock == lock){
            e = list_remove(e);
        }else{
            e = list_next(e);
        }
    }
}

// LOCK이 해제될때 Donation 받았던 Priority에 대한 처리가 필요하다!! (계속 해당 우선순위를 들고 있으면 안됨) -> priority 갱신
// Project 1-2.3 : Priority Inversion Problem - Priority Donation
void refresh_priority(void){
    struct thread *curr = thread_current();
    curr -> priority = curr -> init_priority;

    if (!(list_empty(&curr -> donations))){
        list_sort(&curr -> donations, cmp_donation_list_priority, 0);

        struct thread *front = list_entry(list_front(&curr->donations), struct thread, donation_elem);
        if (front -> priority > curr ->priority){
            curr -> priority = front -> priority;
        }
    }
}
// *************************ADDED LINE ENDS HERE************************* //


/* Returns the name of the running thread. */
const char *thread_name(void) {
    return thread_current()->name;
}

/* Returns the running thread.
   This is running_thread() plus a couple of sanity checks.
   See the big comment at the top of thread.h for details. */
struct thread *thread_current(void) {
    struct thread *t = running_thread ();

    /* Make sure T is really a thread.
       If either of these assertions fire, then your thread may
       have overflowed its stack.  Each thread has less than 4 kB
       of stack, so a few big automatic arrays or moderate
       recursion can cause stack overflow. */
    ASSERT(is_thread (t));
    ASSERT(t->status == THREAD_RUNNING);

    return t;
}

/* Returns the running thread's tid. */
tid_t thread_tid(void) {
    return thread_current()->tid;
}

/* Deschedules the current thread and destroys it.  Never
   returns to the caller. */
void thread_exit(void) {
    ASSERT(!intr_context());

#ifdef USERPROG
    process_exit ();
#endif

    /* Just set our status to dying and schedule another process.
       We will be destroyed during the call to schedule_tail(). */
    intr_disable();
    do_schedule(THREAD_DYING);
    NOT_REACHED();
}

/* Yields the CPU.  The current thread is not put to sleep and
   may be scheduled again immediately at the scheduler's whim. */
void thread_yield(void) {
    struct thread *curr = thread_current();
    enum intr_level old_level;

    ASSERT(!intr_context());
    // ******************************INTERRUPT DISABLED****************************** //
    old_level = intr_disable();
    if (curr != idle_thread){ // Idle Condition Check

        // ******************************LINE MODDED****************************** //
        // Project 1-2.1 : Thread - RoundRobin Scheduling -> Priority Scheduling

        /*list_push_back(&ready_list, &curr->elem);*/
        list_insert_ordered(&ready_list, &curr->elem, &cmp_priority, 0);

        // *************************MDDED LINE ENDS HERE************************* //
    }
    do_schedule(THREAD_READY);
    intr_set_level(old_level);
    // ******************************INTERRUPT RE-ENABLED****************************** //
}

/* Sets the current thread's priority to NEW_PRIORITY. */
void thread_set_priority(int new_priority) {
// 만약 Priority Donation 구현 이후 이전에 PASS되었던 TEST CASE들이 FAIL난다면 높은 확률로
// thread_current()->priority = new_priority; 를 수정 안해줬을 가능성이 크다.

    thread_current()->init_priority = new_priority;

    // ******************************LINE ADDED****************************** //
    // Project 1-2.1 : Thread - RoundRobin Scheduling -> Priority Scheduling

    // Project 1-2.3 : Priority Inversion Problem - Priority Donation
    // ******************************LINE ADDED****************************** //
    refresh_priority();
    // *************************ADDED LINE ENDS HERE************************* //
    /*printf("test_max_priority function called in thread_set_priority\n");
    //Debugging Project 2 : User Programs - Argument Passing*/
    test_max_priority();

    // *************************ADDED LINE ENDS HERE************************* //
}

/* Returns the current thread's priority. */
int thread_get_priority(void) {
    return thread_current()->priority;
}

/* Sets the current thread's nice value to NICE. */
void thread_set_nice(int nice UNUSED) {
    /* TODO: Your implementation goes here */
}

/* Returns the current thread's nice value. */
int thread_get_nice(void) {
    /* TODO: Your implementation goes here */
    return 0;
}

/* Returns 100 times the system load average. */
int thread_get_load_avg(void) {
    /* TODO: Your implementation goes here */
    return 0;
}

/* Returns 100 times the current thread's recent_cpu value. */
int thread_get_recent_cpu(void) {
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
static void idle(void *idle_started_ UNUSED) {
    struct semaphore *idle_started = idle_started_;

    idle_thread = thread_current();
    sema_up(idle_started);

    for (;;) {
        /* Let someone else run. */
        intr_disable();
        thread_block();

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
static void kernel_thread(thread_func *function, void *aux) {
    ASSERT(function != NULL);

    intr_enable();       /* The scheduler runs with interrupts off. */
    function(aux);       /* Execute the thread function. */
    thread_exit();       /* If function() returns, kill the thread. */
}


/* Does basic initialization of T as a blocked thread named
   NAME. */
static void init_thread(struct thread *t, const char *name, int priority) {
    ASSERT(t != NULL);
    ASSERT(PRI_MIN <= priority && priority <= PRI_MAX);
    ASSERT(name != NULL);

    memset(t, 0, sizeof *t);
    t->status = THREAD_BLOCKED;
    strlcpy(t->name, name, sizeof t->name);
    t->tf.rsp = (uint64_t) t + PGSIZE - sizeof(void *);
    t->priority = priority;
    t->magic = THREAD_MAGIC;

    // ******************************LINE ADDED****************************** //
    // Project 1-2.3 : Priority Inversion Problem - Priority Donation
    // Thread Struct has been MODDED for Priority Donation. Therefore inirialization have to modded as well
    t -> init_priority = priority; // 처음 할당받은 Priority를 담아둔다
    list_init(&t->donations);
    t -> wait_on_lock = NULL; // 처음 스레드 init 되었을때는 아직 어떤 LOCK 을 대기할지 모름


    // ******************************LINE ADDED****************************** //
    // Project 2-2-1: User Programs - System Call - Basics
    list_init(&t->child_list);
    sema_init(&t->wait_sema, 0);
    sema_init(&t->fork_sema, 0);
    sema_init(&t->free_sema, 0);
    t->running = NULL;
    t->exit_status = 0; // 스레드 시작시 상태 플래그 0으로 초기화
    // *************************ADDED LINE ENDS HERE************************* //
}

/* Chooses and returns the next thread to be scheduled.  Should
   return a thread from the run queue, unless the run queue is
   empty.  (If the running thread can continue running, then it
   will be in the run queue.)  If the run queue is empty, return
   idle_thread. */
static struct thread *next_thread_to_run(void) {
    if (list_empty(&ready_list))
        return idle_thread;
    else
        return list_entry(list_pop_front(&ready_list), struct thread, elem);
}

/* Use iretq to launch the thread */
void do_iret(struct intr_frame *tf) {
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
static void thread_launch(struct thread *th) {
    uint64_t tf_cur = (uint64_t) & running_thread ()->tf;
    uint64_t tf = (uint64_t) & th->tf;
    ASSERT(intr_get_level() == INTR_OFF);

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
            : : "g"(tf_cur), "g" (tf) : "memory");
}

/* Schedules a new process. At entry, interrupts must be off.
 * 스케쥴은 그 자체로 새로운 프로세스 이므로 진입시 인터럽트가 비활성화 되어야 한다.
 * This function modify current thread's status to status and then
 * do_schedule 함수는 현재 스레드의 상태를 다른 상태로 바꾸어 주고
 * finds another thread to run and switches to it.
 * 해당 스레드와 바꾸어 실행할 다른 스레드를 찾아준다.
 * It's not safe to call printf() in the schedule().
 * 스케쥴 함수 안에서는 printf() 함수 호출은 권장되지 않음.
 * */
static void do_schedule(int status) {
    ASSERT(intr_get_level() == INTR_OFF);
    ASSERT(thread_current()->status == THREAD_RUNNING);
    while (!list_empty(&destruction_req)) {
        struct thread *victim = list_entry(list_pop_front(&destruction_req), struct thread, elem);
        palloc_free_page(victim);
    }
    thread_current()->status = status;
    schedule();
}

static void schedule(void) {
    struct thread *curr = running_thread ();
    struct thread *next = next_thread_to_run();

    ASSERT(intr_get_level() == INTR_OFF);
    ASSERT(curr->status != THREAD_RUNNING);
    ASSERT(is_thread (next));
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
           currently used bye the stack.
           The real destruction logic will be called at the beginning of the
           schedule(). */
        if (curr && curr->status == THREAD_DYING && curr != initial_thread) {
            ASSERT(curr != next);
            list_push_back(&destruction_req, &curr->elem);
        }

        /* Before switching the thread, we first save the information
         * of current running. */
        thread_launch(next);
    }
}

/* Returns a tid to use for a new thread. */
static tid_t allocate_tid(void) {
    static tid_t next_tid = 1;
    tid_t tid;

    lock_acquire(&tid_lock);
    tid = next_tid++;
    /*printf("lock_release called in allocated_tid\n"); //Debugging Project 2 : User Programs - Argument Passing*/
    lock_release(&tid_lock);

    return tid;
}
