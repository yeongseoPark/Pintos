#include "userprog/process.h"
#include <debug.h>
#include <inttypes.h>
#include <round.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "userprog/gdt.h"
#include "userprog/tss.h"
#include "filesys/directory.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "threads/flags.h"
#include "threads/init.h"
#include "threads/interrupt.h"
#include "threads/palloc.h"
#include "threads/thread.h"
#include "threads/mmu.h"
#include "threads/vaddr.h"
#include "intrinsic.h"
#ifdef VM
#include "vm/vm.h"
#endif

// ******************************LINE ADDED****************************** //
// Project 2-1 : User Programs - Argument Passing
#define EOL sizeof("") // Size of Sentinel
#define SAU 8 // Stack Pointer Alignment Unit = 8 byte
// *************************ADDED LINE ENDS HERE************************* //

static void process_cleanup (void);
static bool load (const char *file_name, struct intr_frame *if_);
static void initd (void *f_name);
static void __do_fork (void *);

// ******************************LINE ADDED****************************** //
// Project 2-1 : User Programs - Argument Passing
void argument_stack(char **argv, int argc, struct intr_frame *if_);
// *************************ADDED LINE ENDS HERE************************* //

/* General process initializer for initd and other process. */
static void process_init (void) {
	struct thread *current = thread_current (); // 1프로세스 = 1스레드 이니까 Assert(is thread)만으로도 프로세스가 온전한지 확인할 수 있다
}

/* Starts the first userland program, called "initd", loaded from FILE_NAME.
 * The new thread may be scheduled (and may even exit)
 * before process_create_initd() returns. Returns the initd's
 * thread id, or TID_ERROR if the thread cannot be created.
 * Notice that THIS SHOULD BE CALLED ONCE. */
tid_t process_create_initd (const char *file_name) {
	char *fn_copy;
	tid_t tid;
    // ******************************LINE ADDED****************************** //
    // Project 2-2 : User Programs - System Call
    char *save_ptr;
    // *************************ADDED LINE ENDS HERE************************* //

	/* Make a copy of FILE_NAME.
	 * Otherwise there's a race between the caller and load(). */
	fn_copy = palloc_get_page (0);
	if (fn_copy == NULL)
		return TID_ERROR;
	strlcpy (fn_copy, file_name, PGSIZE);

	/* Create a new thread to execute FILE_NAME. */
    // ******************************LINE MODDED****************************** //
    // Project 2-2-2 : User Programs - System Call - File Descriptor
    file_name = strtok_r (file_name, " ", &save_ptr);
    tid = thread_create (file_name, PRI_DEFAULT, initd, fn_copy);
    // *************************MODDED LINE ENDS HERE************************* //

	if (tid == TID_ERROR)
		palloc_free_page (fn_copy);
	return tid;
}

/* A thread function that launches first user process.
	첫번째 유저 프로세스를 실행하는 함수
 */
/* start_process */
static void initd (void *f_name) {
#ifdef VM
	supplemental_page_table_init (&thread_current ()->spt);
#endif

/* 준코(05/12) */
/* Initializing the set of vm_entries. ex) hash table */
	
/* Initialize interrupt frame and load executable */

	memset(&if_, 0, sizeof if_);
/* 준코 끝 */	
	process_init (); // 제대로된 스레드인지를 확인하는 것 만으로도 제대로 된 프로세스인지를 알 수 있음

	if (process_exec (f_name) < 0)
		PANIC("Fail to launch initd\n");
	NOT_REACHED ();
}

/* Clones the current process as `name`. Returns the new process's thread id, or  
 * TID_ERROR if the thread cannot be created.
 child process 안에서 return value = 0
 RBX, RSP, RBP, R12 ~ R15 를 제외한 나머지는 clone해줄 필요 X
 child는 부모와 duplicated 자원을 가짐(fd, 가상메모리 공간 등)
 부모는 자식이 완전히 clone됐다는 것을 알기 전까지 fork에서 리턴하면 안됨
 -> 자식이 자원을 복제하는데 실패했다면, fork는 TID_ERROR 반환
 pml4_for_each로 전체 유저 메모리 공간 복제
  */
tid_t process_fork (const char *name, struct intr_frame *if_ UNUSED) {
	/* Clone current thread to new thread.*/
    // ******************************LINE ADDED****************************** //
    // Project 2-2 : User Programs - System Call

    struct thread *curr = thread_current();
    memcpy(&curr->parent_if, if_, sizeof(struct intr_frame)); // 인자로 받은 유저스택(if_)를 parent_if에 붙여넣기
	/* parent_if : tf의 rsp는 커널 스택을 가리키고 있다(tss->rsp0)
	   그래서 userland context를 가지고 있지 않은데, fork()를 할때 user stack 정보를 가져와야만 한다.
	   그래야 fork() 이후부터 자식 스레드를 실행할 수 있다
	   따라서 user_stack 정보를 담는 인터럽트 프레임을 만들어줘야 하는데, 이것이 parent_if고, 커널 스택 안에서 만들어진다.
	   - tf는 커널 스레드에 할당되는 페이지 가장 아래에 있는 struct thread 구조체의 멤버	
	 */

	/* tf의 rsp가 커널 스택을 가리키고 있는 이유, tf의 rsp는 원래 user stack의 끝(최하단)을 가리키고 있음(user context)
	근데 fork()를 실행하기 위해 syscall -> syscall entry를 거치는데, 이때 syscall_entry에서 rsp값을 rbx로 옮겨준다
	그리고 rsp에는 커널 스택 공간을 가리키는 tss->rsp0을 넣기에, rsp는 커널 스택 공간을 보고있다.
	따라서 tf의 rsp는 현재 userland context를 보고 있지 않은것.
	*/

    tid_t pid = thread_create(name, PRI_DEFAULT, __do_fork, curr); 

    if (pid == TID_ERROR){
        return TID_ERROR;
    }

    struct thread *child = get_child(pid);

    sema_down(&child->fork_sema); // 자식 프로세스 로드 완료될때까지 기다림 (__do_fork에서 FDT까지 복사를 완료하면 up해줌)

    if (child->exit_status == -1)
        return TID_ERROR;

    return pid;
    // *************************ADDED LINE ENDS HERE************************* //
}

#ifndef VM
/* Duplicate the parent's address space by passing this function to the
 * pml4_for_each. This is only for the project 2. */
static bool duplicate_pte (uint64_t *pte, void *va, void *aux) {
	struct thread *current = thread_current ();
	struct thread *parent = (struct thread *) aux;
	void *parent_page;
	void *newpage;
	bool writable;

	/* 1. TODO: If the parent_page is kernel page, then return immediately.
		커널 영역인지 확인
	 */
    // ******************************LINE ADDED****************************** //
    // Project 2-2 : User Programs - System Call
    if (is_kernel_vaddr(va)){ 
        return true;
    }
    // *************************ADDED LINE ENDS HERE************************* //

	/* 2. Resolve VA from the parent's page map level 4. 
		부모 스레드 내 멤버인 pml4를 이용해서 parent_page를 얻어냄
	*/
	parent_page = pml4_get_page (parent->pml4, va);
    // ******************************LINE ADDED****************************** //
    // Project 2-2 : User Programs - System Call
    if (parent_page == NULL){
        return false;
    }
    // *************************ADDED LINE ENDS HERE************************* //

	/* 3. TODO: Allocate new PAL_USER page for the child and set result to
	 *    TODO: NEWPAGE.
	 child를 위한 페이지 newpage를 PAL_USER로 할당받음
	  */
    newpage = palloc_get_page (PAL_USER | PAL_ZERO);
    if (newpage == NULL){
        return false;
    }

	/* 4. TODO: Duplicate parent's page to the new page and
	 *    TODO: check whether parent's page is writable or not (set WRITABLE
	 *    TODO: according to the result). 
	 * parent_page를 newpage에 복사 
	 * */
    memcpy(newpage, parent_page, PGSIZE); // parent_page는 가상주소이고, 이것을 newpage에 복사 _ SIZE는 4KB(할당해준 공간도 4KB)
    writable = is_writable(pte); // pte가 읽고/쓰기가 가능한지 확인

	/* 5. Add new page to child's page table at address VA with WRITABLE
	 *    permission.
	 child의 pml에 newpage를 설정함
	  */
	if (!pml4_set_page (current->pml4, va, newpage, writable)) {
		/* 6. TODO: if fail to insert page, do error handling. */
        // ******************************LINE ADDED****************************** //
        // Project 2-2 : User Programs - System Call
        return false; // 에러 발생시 에러핸들링
        // *************************ADDED LINE ENDS HERE************************* //
	}
	return true;
}
#endif

/* A thread function that copies parent's execution context.
 * Hint) parent->tf does not hold the userland context of the process.
 *       That is, you are required to pass second argument of process_fork to
 *       this function. */
static void __do_fork (void *aux) {
	struct intr_frame if_;
	struct thread *parent = (struct thread *) aux;
	struct thread *current = thread_current (); // 밑의 주석은 틀렸고, do_fork는 스레드가 running 상태로 들어가면 실행되는 함수이기 때문에, 당연히 curr는 복제되는 프로세스가 맞다
	 // 얘는 복제되는 스레드임? 부모스레드임? -> rsp를 기준으로 current를 잡기때문에
	// thread_create->init_thread에서 rsp를 새로 생성되는 스레드의 끝으로 잡는다. 따라서 복제되는(새로 생성되는)스레드가 맞음
	/* TODO: somehow pass the parent_if. (i.e. process_fork()'s if_) */
	struct intr_frame *parent_if;
	bool success = true;

	/* 1. Read the cpu context to local stack. */
    // ******************************LINE ADDED****************************** //
    parent_if = &parent->parent_if; // user stack의 정보를 parent_if에 저장

	memcpy (&if_, parent_if, sizeof (struct intr_frame)); // if_에 parent_if를 복사
    if_.R.rax = 0; // Child Process return 0 -> 반환값을 rax에 저장하는데, child process는 0을 반환
    // *************************ADDED LINE ENDS HERE************************* //

	/* 2. Duplicate PT */
	current->pml4 = pml4_create();
	if (current->pml4 == NULL)
		goto error;

	process_activate (current);
#ifdef VM
	supplemental_page_table_init (&current->spt);
	if (!supplemental_page_table_copy (&current->spt, &parent->spt))
		goto error;
#else
	if (!pml4_for_each (parent->pml4, duplicate_pte, parent)) // duplicate_pte를 parent->pml4의 모든 페이지에 적용
	// 부모의 페이지들을 복사
		goto error;
#endif

	/* TODO: Your code goes here.
	 * TODO: Hint) To duplicate the file object, use `file_duplicate`
	 * TODO:       in include/filesys/file.h. Note that parent should not return
	 * TODO:       from the fork() until this function successfully duplicates
	 * TODO:       the resources of parent.*/

    // ******************************LINE ADDED****************************** //
    // Project 2-2-2 : User Programs - System Call - File Descriptor
    if (parent->fd_idx >= FDCOUNT_LIMIT)
        goto error;

	/* caution! FDT는 각 프로세스마다 들고있는 식별자 테이블이고, 
		열려있는 파일은 모든 프로세스들이 공유하는 한개의 파일 테이블인 File Table로 표시됨
		File Table에서 각 프로세스별로 파일을 오픈할때 refcnt(참조횟수)를 1증가시키고, 파일을 닫으면 refcnt를 1감소시킴.
		모든 프로세스에서 동일한 파일을 다 닫으면 refcnt = 0 

		이때, 자식프로세스의 FDT는 부모프로세스의 FDT와 동일하게 해줘야 한다.
	*/

	/* 이 둘은 특정 파일 객체를 나타내는 것이 아니라, 그저 표준 입출력 값이기 때문에 바로 매칭해주면 된다 */
    current->fd_table[0] = parent->fd_table[0]; // stdin
    current->fd_table[1] = parent->fd_table[1]; // stdout

    for (int i = 2 ; i < FDCOUNT_LIMIT ;i++ ){
        struct file *f = parent->fd_table[i];
        if (f == NULL){
            continue;
        }
        current->fd_table[i] = file_duplicate(f); // file_duplicate가 제공됨
    }


    current->fd_idx = parent->fd_idx;

    // ------------------여기서 부모를 베껴오는 과정이 끝. if child loaded successfully, wake up parent in process_fork
    sema_up(&current->fork_sema);

	/* Finally, switch to the newly created process. do_iret으로, 인터럽트프레임의 값을 CPU에 올림 */
	if (success)
		do_iret (&if_);
error:
    // ******************************LINE MODDED****************************** //
    // Project 2-2 : User Programs - System Call
    current->exit_status = TID_ERROR;
    sema_up(&current->fork_sema);
    exit(TID_ERROR);
    /*thread_exit ();*/
    // *************************MODDED LINE ENDS HERE************************* //

}

/* Switch the current execution context to the f_name.
 * Returns -1 on fail. */
int process_exec (void *f_name) {
	char *file_name = f_name; // 매개변수 void* 로 넘겨받은 f_name을 char* 로 변환 처리
	// caller와 load함수간의 race condition을 방지

	bool success;

    // ******************************LINE ADDED****************************** //
    // Project 2-1 : User Programs - Argument Passing
    // strtok_r 함수 이용, 공백 기준으로 명령어 parsing 구현
    char *arg_list[128];
    char *token, *save_ptr;
    int token_count = 0;

    token = strtok_r(file_name, " ", &save_ptr);
    arg_list[token_count] = token;

    while(token!=NULL){
        token = strtok_r(NULL, " ", &save_ptr);
        token_count++;
        arg_list[token_count] = token;
    }

	/* We cannot use the intr_frame in the thread structure.
	 * This is because when current thread rescheduled,
	 * it stores the execution information to the member. -> 이걸 멤버에 저장해서 직접적으로 쓸수없다? */
	struct intr_frame _if;
	_if.ds = _if.es = _if.ss = SEL_UDSEG; // 커널스레드가 아니라 유저프로세스이기에 KDSEG가 아니라 UDSEG로 설정
	_if.cs = SEL_UCSEG;
	_if.eflags = FLAG_IF | FLAG_MBS; // 인터럽트 가능 , 부동소수점

	/* We first kill the current context */
	process_cleanup ();

    // ******************************LINE MODDED****************************** //
	/* And then load the binary */
	/* ELF : 사용자 프로세스를 실행하는데 사용.
	   핀토스의 사용자 프로세스는 ELF파일로부터 읽힌 코드와 데이터를 메모리에 로드하여 실행됨
	   load()가 하는일 : 페이지테이블 만들고, 파일을 열고, ELF실행파일이 올바른지 확인, ELF의 프로그램 헤더 테이블 읽어서 메모리에 로드, 
	 */

	/* VM : initialize the set of vm_entries */

	success = load (file_name, &_if); 
    /*memset(&_if, 0, sizeof _if);
    success = load(file_name_copy, &_if);*/
    // *************************ADDED LINE ENDS HERE************************* //

	/* If load failed, quit. */
	if (!success) {
        palloc_free_page (file_name);
        return -1;
    }

    // ******************************LINE ADDED****************************** //
    // Project 2-1 : User Programs - Argument Passing

    argument_stack(arg_list, token_count, &_if); // 인자들도 유저 스택에 쌓아준다

    // hex dump for Debugging
    // !!! CAUTION !!! hex_dump 는 매개변수를 pos, buffer, size, boolean 로 받는다. Defined in stdio.c
    // Dumps the SIZE bytes in BUF to the console as hex bytes arranged 16 per line.
    // Numeric offsets are also included, starting at OFS for the first byte in BUF.
    // If ASCII is true then the corresponding ASCII characters are also rendered alongside.

    /*hex_dump(_if.rsp, _if.rsp, KERN_BASE - _if.rsp, true);*/

    // KERN_BASE , USER_STACK 어떤걸 기준으로 hex_dump 할 것인지에 따라 page_fault 오류 야기시킴
    // 0000000047480000  Page fault at 0x47480000: not present error reading page in kernel context. 의 원인.
    // Gitbook Project 2 : USER PROGRAMS - Introduction - Virtual Memory Layout~Accessing User Memory를 참조하자.
    /*hex_dump(_if.rsp, _if.rsp, KERN_BASE - _if.rsp, true);*/

    // hex_dump(_if.rsp, _if.rsp, USER_STACK - _if.rsp, true);
    // *************************ADDED LINE ENDS HERE************************* //

	/* Start switched process. */
	do_iret (&_if); // 사용자 프로세스로 CPU가 진짜 넘어감
	// exec()에서 여태껏 만들어준 _if 구조체 안의 값으로 레지스터 값을 저장

	NOT_REACHED ();
}


/* Waits for thread TID to die and returns its exit status.  If
 * it was terminated by the kernel (i.e. killed due to an
 * exception), returns -1.  If TID is invalid or if it was not a
 * child of the calling process, or if process_wait() has already
 * been successfully called for the given TID, returns -1
 * immediately, without waiting.
 *
 * This function will be implemented in problem 2-2.  For now, it
 * does nothing. */
int process_wait (tid_t child_tid UNUSED) {
    // ******************************LINE ADDED****************************** //
    // Project 2-1 : User Programs - Argument Passing
    /* XXX: Hint) The pintos exit if process_wait (initd), we recommend you
     * XXX:       to add infinite loop here before
     * XXX:       implementing the process_wait. */
    /*while(true) {
        // (TEMPORARY FOR DEBUGGING) Infinite Loop to fake child wait
    }
    return -1;*/
    /*printf("Waiting Child Process...\n");*/
    // *************************ADDED LINE ENDS HERE************************* //

    // ******************************LINE ADDED****************************** //
    // Project 2-2 : User Programs - System Call
    struct thread *child = get_child(child_tid);
    if (child == NULL){
        return -1;
    }
    sema_down(&child->wait_sema); 
	// 자식이 부모를 재움? -> 결국 현재 돌고 있는 스레드는 부모니까,
	// 부모는 자다가 process_exit에서 자식이 wait_sema를 up해주면 일어남.
    int exit_status = child->exit_status;
    list_remove(&child->child_elem);
    sema_up(&child->free_sema); 
	// free_sema를 up시켜줘서, 부모가 자신을 정리할동안 기다리던 자식이, 스스로를 정리할 수 있게 해줌

    return exit_status;
    // *************************ADDED LINE ENDS HERE************************* //
}

/* Exit the process. This function is called by thread_exit (). */
void process_exit (void) {
	struct thread *curr = thread_current ();
	uint32_t *pd; /* 준코(05/12) */
	/* TODO: Your code goes here.
	 * TODO: Implement process termination message (see
	 * TODO: project2/process_termination.html).
	 * TODO: We recommend you to implement process resource cleanup here. */

    // Close all Opened file by for loop
    for (int i=0; i<FDCOUNT_LIMIT; i++)
    {
        close(i);
    }
    palloc_free_multiple(curr->fd_table, FDT_PAGES);
    file_close(curr->running);

	/* 준코(05/12) */
	/* vm_entry delete 함수 추가해야 함 */
	pd = curr->pagedir;

    sema_up(&curr->wait_sema); // 부모를 깨운다
    sema_down(&curr->free_sema); 
	/*	부모가 process_wait의 해당코드를 실행해서 자식을 자식 리스트에서 지우는동안, 잠깐 기다리기 위해서 free sema를 down시켜서 현재 스레드는 잔다
	int exit_status = child->exit_status;
    list_remove(&child->child_elem);
    sema_up(&child->free_sema); // 여기서 자식은 blocked에서 벗어나 스케줄링 될 기회를 얻음
	만약 스케줄링이 되면, 바로 밑의 process_cleanup으로 자기 자신을 정리하고 끝내면 됨
	*/
    process_cleanup ();
}

/* Free the current process's resources. */
static void process_cleanup (void) {
	struct thread *curr = thread_current ();

#ifdef VM
	supplemental_page_table_kill (&curr->spt); // 카이스트 강의에서 vm_entries라고 부르는 이 친구 를 삭제
#endif

	uint64_t *pml4;
	/* Destroy the current process's page directory and switch back
	 * to the kernel-only page directory. */
	pml4 = curr->pml4;
	if (pml4 != NULL) {
		/* Correct ordering here is crucial.  We must set
		 * cur->pagedir to NULL before switching page directories,
		 * so that a timer interrupt can't switch back to the
		 * process page directory.  We must activate the base page
		 * directory before destroying the process's page
		 * directory, or our active page directory will be one
		 * that's been freed (and cleared). */
		curr->pml4 = NULL;
		pml4_activate (NULL);
		pml4_destroy (pml4);
	}
}

/* Sets up the CPU for running user code in the nest thread.
 * This function is called on every context switch. */
void process_activate (struct thread *next) {
	/* Activate thread's page tables. */
	pml4_activate (next->pml4);

	/* Set thread's kernel stack for use in processing interrupts. */
	tss_update (next);
}

/* We load ELF binaries.  The following definitions are taken
 * from the ELF specification, [ELF1], more-or-less verbatim.  */

/* ELF types.  See [ELF1] 1-2. */
#define EI_NIDENT 16

#define PT_NULL    0            /* Ignore. */
#define PT_LOAD    1            /* Loadable segment. */
#define PT_DYNAMIC 2            /* Dynamic linking info. */
#define PT_INTERP  3            /* Name of dynamic loader. */
#define PT_NOTE    4            /* Auxiliary info. */
#define PT_SHLIB   5            /* Reserved. */
#define PT_PHDR    6            /* Program header table. */
#define PT_STACK   0x6474e551   /* Stack segment. */

#define PF_X 1          /* Executable. */
#define PF_W 2          /* Writable. */
#define PF_R 4          /* Readable. */

/* Executable header.  See [ELF1] 1-4 to 1-8.
 * This appears at the very beginning of an ELF binary. */
struct ELF64_hdr {
	unsigned char e_ident[EI_NIDENT];
	uint16_t e_type;
	uint16_t e_machine;
	uint32_t e_version;
	uint64_t e_entry;
	uint64_t e_phoff;
	uint64_t e_shoff;
	uint32_t e_flags;
	uint16_t e_ehsize;
	uint16_t e_phentsize;
	uint16_t e_phnum;
	uint16_t e_shentsize;
	uint16_t e_shnum;
	uint16_t e_shstrndx;
};

struct ELF64_PHDR {
	uint32_t p_type;
	uint32_t p_flags;
	uint64_t p_offset;
	uint64_t p_vaddr;
	uint64_t p_paddr;
	uint64_t p_filesz;
	uint64_t p_memsz;
	uint64_t p_align;
};

/* Abbreviations */
#define ELF ELF64_hdr
#define Phdr ELF64_PHDR

static bool setup_stack (struct intr_frame *if_);
static bool validate_segment (const struct Phdr *, struct file *);
static bool load_segment (struct file *file, off_t ofs, uint8_t *upage,
		uint32_t read_bytes, uint32_t zero_bytes,
		bool writable);

// ******************************LINE ADDED****************************** //
// Project 2-1 : User Programs - Argument Passing
void argument_stack(char **argv, int argc, struct intr_frame *if_){
    char *arg_address[128];

    // 스택의 확장 방향은 거꾸로!! for문 순회시 역방향으로(--)!!

    // 1. 스택 공간 확보 및 복사
    for(int i = argc - 1; i >= 0; i--){
        int argv_with_senti_len = strlen(argv[i]) + EOL; // EOL(Sentinel) 포함한 길이를 변수에 저장
        if_->rsp -= (argv_with_senti_len); // 받아온 길이 만큼 스택 크기를 늘려줌
        memcpy(if_->rsp, argv[i], argv_with_senti_len); // 늘려준 스택 공간에 해당 인자를 복사
        arg_address[i] = if_->rsp; // arg_address에 인자를 복사한 시작 주소값을 저장해준다
    }

    // 2. Stack Pointer의 word-alignment 단위는 8byte 단위이다. 따라서 이를 맞추기 위해 0 넣어줌.
    while(if_->rsp % SAU != 0){
        if_->rsp--;
        memset(if_->rsp, 0, sizeof(uint8_t));
    }

    // 3. word-align 이후 ~argv[0]의 주소를 넣어준다
    for(int j = argc; j >= 0; j--){
        if_->rsp -= SAU;
        if(j == argc){
            memset(if_->rsp, 0, sizeof(char **));
        } else{
            memcpy(if_->rsp, &arg_address[j], sizeof(char **));
        }
    }

    // 4. Fake return address
    if_->rsp -= sizeof(void *);
    memset(if_->rsp, 0, sizeof(void *));

    // 5. Set rdi, rsi (rdi : 문자열 목적지 주소, rsi : 문자열 출발지 주소)
    if_->R.rdi = argc;
    if_->R.rsi = if_->rsp + SAU;
}

/* 준코(05/12) : ppt 보고 만듬 */
/* page fault 다룰때 호출 */
/* page fault 발생하면, 물리 메모리 할당한다. */
/* 디스크에서 물리 메모리로 파일 load한다.(load_file()) */
/* 물리 메모리로 로드 후 관련된 PTE 업데이트한다.(install_page()) */
bool handle_mm_fault(struct vm_entry *vme)
{
	/* 물리메모리 할당 */
	/* load_file(void* akddr, struct vm_entry *vme) */
	/* install_page(void *upage, void *kpage, bool writable) */
}
// *************************ADDED LINE ENDS HERE************************* //

/* Loads an ELF executable from FILE_NAME into the current thread.
 * Stores the executable's entry point into *RIP
 * and its initial stack pointer into *RSP.
 * Returns true if successful, false otherwise. */
static bool load (const char *file_name, struct intr_frame *if_) {
	struct thread *t = thread_current ();
	struct ELF ehdr; // ELF는 프로그램이 실행될때 메모리에 올라가야 할 각 부분들을 미리 관리하다가, 실행하게 되면 해당부분(code ,data, bss)를 메모리에 올리게 된다
	struct file *file = NULL;
	off_t file_ofs;
	bool success = false;
	int i;

	/* Allocate and activate page directory. */
	t->pml4 = pml4_create (); // 페이지테이블의 최상위레벨 pml4
	if (t->pml4 == NULL)
		goto done;
	process_activate (thread_current ()); // main에서 쭉 진행된 경우, 이때 thread_current는 main스레드

	/* Open executable file. */
	file = filesys_open (file_name);
	if (file == NULL) {
		printf ("load: %s: open failed\n", file_name);
		goto done;
	}

    // ******************************LINE ADDED****************************** //
    // Project 2-2 : User Programs - System Call
    /* 실행 중인 스레드 t의 running을 실행할 파일로 초기화*/
    t->running = file;

    /* 현재 오픈한 파일에 다른내용 쓰지 못하게 함 */
    file_deny_write(file);
    // *************************ADDED LINE ENDS HERE************************* //

	/* Read and verify executable header. -> ELF파일을 올바르게 읽을 수 있는지 확인 */
	if (file_read (file, &ehdr, sizeof ehdr) != sizeof ehdr // file_read로 ELF파일의 헤더를 읽어옴
			|| memcmp (ehdr.e_ident, "\177ELF\2\1\1", 7) // e_ident: ELF파일의 식별자, "\177ELF\2\1\1" 는 ELF파일을 나타내는 매직넘버
			|| ehdr.e_type != 2
			|| ehdr.e_machine != 0x3E // amd64
			|| ehdr.e_version != 1
			|| ehdr.e_phentsize != sizeof (struct Phdr)
			|| ehdr.e_phnum > 1024) { // 위의 4개는 ELF 파일의 정보를 나타냄
		printf ("load: %s: error loading executable\n", file_name);
		goto done;
	}

	/* Read program headers. - ELF파일의 프로그램 헤더 테이블을 읽어서 메모리에 로드 */
	// https://malgun-gothic.tistory.com/110 - 사진
	// ELF 파일 = ELF헤더 + 프로그램 헤더 테이블(시스템에 어떻게 프로세스 이미지 만들지) + 섹션(링킹을 위한 object파일의 정보 보유) 헤더 테이블 - 모든 섹션은 테이블에 하나의 엔트리 가짐
	file_ofs = ehdr.e_phoff; // ELF의 프로그램 헤더테이블 오프셋
	for (i = 0; i < ehdr.e_phnum; i++) { // ELF파일 내의 Program 헤더 엔트리의 수, 엔트리 안에는 해당 세그먼트의 정보가 담겨있음
		struct Phdr phdr;

		if (file_ofs < 0 || file_ofs > file_length (file))
			goto done;
		file_seek (file, file_ofs);

		if (file_read (file, &phdr, sizeof phdr) != sizeof phdr)
			goto done;
		file_ofs += sizeof phdr;
		switch (phdr.p_type) { // 세그먼트 타입에 따라 다른동작
			case PT_NULL:
			case PT_NOTE:
			case PT_PHDR:
			case PT_STACK:
			default:
				/* Ignore this segment. */
				break;
			case PT_DYNAMIC:
			case PT_INTERP:
			case PT_SHLIB:
				goto done;
			case PT_LOAD:
				if (validate_segment (&phdr, file)) { // 해당 세그먼트의 정보가 유효한지 검증
					bool writable = (phdr.p_flags & PF_W) != 0;
					uint64_t file_page = phdr.p_offset & ~PGMASK;
					uint64_t mem_page = phdr.p_vaddr & ~PGMASK;
					uint64_t page_offset = phdr.p_vaddr & PGMASK;
					uint32_t read_bytes, zero_bytes;
					if (phdr.p_filesz > 0) {
						/* Normal segment.
						 * Read initial part from disk and zero the rest. */
						read_bytes = page_offset + phdr.p_filesz;
						zero_bytes = (ROUND_UP (page_offset + phdr.p_memsz, PGSIZE)
								- read_bytes);
					} else {
						/* Entirely zero.
						 * Don't read anything from disk. */
						read_bytes = 0;
						zero_bytes = ROUND_UP (page_offset + phdr.p_memsz, PGSIZE);
					}
					// mem_page에 file_page의 read_byte부터 zero_byte만큼 파일을 로드 = 세그먼트를 메모리에 로드
					if (!load_segment (file, file_page, (void *) mem_page,
								read_bytes, zero_bytes, writable))
						goto done;
				}
				else
					goto done;
				break;
		}
	}

	/* Set up stack. */
	if (!setup_stack (if_)) // 유저 스택에 사용할 페이지를 할당하고, 그 페이지를 메모리에 매핑(유저 스택에 공간을 할당)
		goto done;

	/* Start address. */
	if_->rip = ehdr.e_entry; // 인터럽트 프레임의 PC를 ELF헤더의 엔트리로 설정

	/* TODO: Your code goes here.
	 * TODO: Implement argument passing (see project2/argument_passing.html). */

	success = true;

done:
	/* We arrive here whether the load is successful or not. */
    // ******************************LINE MODDED****************************** //
	/*file_close (file);*/
    // *************************MODDED LINE ENDS HERE************************* //
	return success;
}

// ******************************LINE ADDED****************************** //
// Project 2 : User Programs - System Call - fork()
struct thread *get_child(int pid){

    struct thread *curr = thread_current();
    struct list *child_list = &curr->child_list;
    struct list_elem *e;
    if (list_empty(child_list)) return NULL;
    for (e = list_begin (child_list); e != list_end (child_list); e = list_next (e)){
        struct thread *t = list_entry(e, struct thread, child_elem);
        if (t->tid == pid){
            return t; // 자식 스레드 반환
        }
    }
    return NULL;
}
// *************************ADDED LINE ENDS HERE************************* //


/* Checks whether PHDR describes a valid, loadable segment in
 * FILE and returns true if so, false otherwise. */
static bool validate_segment (const struct Phdr *phdr, struct file *file) {
	/* p_offset and p_vaddr must have the same page offset. */
	if ((phdr->p_offset & PGMASK) != (phdr->p_vaddr & PGMASK))
		return false;

	/* p_offset must point within FILE. */
	if (phdr->p_offset > (uint64_t) file_length (file))
		return false;

	/* p_memsz must be at least as big as p_filesz. */
	if (phdr->p_memsz < phdr->p_filesz)
		return false;

	/* The segment must not be empty. */
	if (phdr->p_memsz == 0)
		return false;

	/* The virtual memory region must both start and end within the
	   user address space range. */
	if (!is_user_vaddr ((void *) phdr->p_vaddr))
		return false;
	if (!is_user_vaddr ((void *) (phdr->p_vaddr + phdr->p_memsz)))
		return false;

	/* The region cannot "wrap around" across the kernel virtual
	   address space. */
	if (phdr->p_vaddr + phdr->p_memsz < phdr->p_vaddr)
		return false;

	/* Disallow mapping page 0.
	   Not only is it a bad idea to map page 0, but if we allowed
	   it then user code that passed a null pointer to system calls
	   could quite likely panic the kernel by way of null pointer
	   assertions in memcpy(), etc. */
	if (phdr->p_vaddr < PGSIZE)
		return false;

	/* It's okay. */
	return true;
}

#ifndef VM
/* Codes of this block will be ONLY USED DURING project 2.
 * If you want to implement the function for whole project 2, implement it
 * outside of #ifndef macro. */

/* load() helpers. */
static bool install_page (void *upage, void *kpage, bool writable);

/* Loads a segment starting at offset OFS in FILE at address
 * UPAGE.  In total, READ_BYTES + ZERO_BYTES bytes of virtual
 * memory are initialized, as follows:
 *
 * - READ_BYTES bytes at UPAGE must be read from FILE
 * starting at offset OFS.
 *
 * - ZERO_BYTES bytes at UPAGE + READ_BYTES must be zeroed.
 *
 * The pages initialized by this function must be writable by the
 * user process if WRITABLE is true, read-only otherwise.
 *
 * Return true if successful, false if a memory allocation error
 * or disk read error occurs. */

/* 준코(05/12) */
/* 가상 주소 공간과 관련된 구조체 초기화하는 함수 만들기 */
/* binary file을 가상 주소 공간으로 load하는 구문 삭제 */
/* 다음 3가지를 추가하기 1.vm_entry 구조체 할당 2.필드값 초기화 3.해쉬테이블에 삽입*/
static bool load_segment (struct file *file, off_t ofs, uint8_t *upage,
		uint32_t read_bytes, uint32_t zero_bytes, bool writable) {
	ASSERT ((read_bytes + zero_bytes) % PGSIZE == 0);
	ASSERT (pg_ofs (upage) == 0);
	ASSERT (ofs % PGSIZE == 0);

	

	// 여긴 지워야 하는듯,,?
	file_seek (file, ofs);
	while (read_bytes > 0 || zero_bytes > 0) {
		/* Do calculate how to fill this page.
		 * We will read PAGE_READ_BYTES bytes from FILE
		 * and zero the final PAGE_ZERO_BYTES bytes. */
		size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
		size_t page_zero_bytes = PGSIZE - page_read_bytes;

			/* create vm_entry(use malloc)*
	/* setting vm_entry members, offset and size of file to read when virtual page is required, zero byte to pad at the end/

		/* 준코(05/12) : ppt에서 삭제하래서 주석처리 */
		/* Get a page of memory. */
		// uint8_t *kpage = palloc_get_page (PAL_USER);
		// if (kpage == NULL)
		// 	return false;

		// /* Load this page. */
		// if (file_read (file, kpage, page_read_bytes) != (int) page_read_bytes) {
		// 	palloc_free_page (kpage);
		// 	return false;
		// }
		// memset (kpage + page_read_bytes, 0, page_zero_bytes);

		// /* Add the page to the process's address space. */
		// if (!install_page (upage, kpage, writable)) {
		// 	printf("fail\n");
		// 	palloc_free_page (kpage);
		// 	return false;
		// }
		/* 준코(05/12) : 추가부분*/

		/* vm_entry 생성 (malloc 사용) */
		/* vm_entry 원소, offset, 가상 페이지가 요구할 때 읽을 파일의 크기 */
		/* insert_vme() 사용하여 vm_entry를 hash table에 넣기 */

		/* 준코 끝 */

		/* Advance. */
		read_bytes -= page_read_bytes;
		zero_bytes -= page_zero_bytes;
		upage += PGSIZE;
	}
	return true;
}

/* Create a minimal stack by mapping a zeroed page at the USER_STACK */
static bool setup_stack (struct intr_frame *if_) {
	uint8_t *kpage;
	bool success = false;

	kpage = palloc_get_page (PAL_USER | PAL_ZERO);
	if (kpage != NULL) {
		success = install_page (((uint8_t *) USER_STACK) - PGSIZE, kpage, true); // 할당된 페이지를 유저 스택페이지에 매핑(유저 스택의 가장 위쪽주소에)
		if (success)
			if_->rsp = USER_STACK; // 스택이 줄어들었기에(바로 위에서 install해서), 인터럽트 프레임의 rsp를 업데이트 해줌
		else
			palloc_free_page (kpage);
	}

	/* 준코(05/12) */
	/* vm_entry 생성 */
	/* vm_entry 원소 설치 */
	/* insert_vme()를 사용해 vm_entry를 해쉬 테이블에 추가 */
	return success;
}

/* Adds a mapping from user virtual address UPAGE to kernel
 * virtual address KPAGE to the page table.
 * If WRITABLE is true, the user process may modify the page;
 * otherwise, it is read-only.
 * UPAGE must not already be mapped.
 * KPAGE should probably be a page obtained from the user pool
 * with palloc_get_page().
 * Returns true on success, false if UPAGE is already mapped or
 * if memory allocation fails. */
static bool install_page (void *upage, void *kpage, bool writable) {
	struct thread *t = thread_current ();

	/* Verify that there's not already a page at that virtual
	 * address, then map our page there. */
	return (pml4_get_page (t->pml4, upage) == NULL
			&& pml4_set_page (t->pml4, upage, kpage, writable));
}

/* ----------------- if VM defined ------------------*/
#else 
/* From here, codes will be used after project 3.
 * If you want to implement the function for only project 2, implement it on the
 * upper block. */

static bool lazy_load_segment (struct page *page, void *aux) {
	/* TODO: Load the segment from the file */
	/* TODO: This called when the first page fault occurs on address VA. */
	/* TODO: VA is available when calling this function. */
}

/* Loads a segment starting at offset OFS in FILE at address
 * UPAGE.  In total, READ_BYTES + ZERO_BYTES bytes of virtual
 * memory are initialized, as follows:
 *
 * - READ_BYTES bytes at UPAGE must be read from FILE
 * starting at offset OFS.
 *
 * - ZERO_BYTES bytes at UPAGE + READ_BYTES must be zeroed.
 *
 * The pages initialized by this function must be writable by the
 * user process if WRITABLE is true, read-only otherwise.
 *
 * Return true if successful, false if a memory allocation error
 * or disk read error occurs. */
static bool load_segment (struct file *file, off_t ofs, uint8_t *upage,
		uint32_t read_bytes, uint32_t zero_bytes, bool writable) {
	ASSERT ((read_bytes + zero_bytes) % PGSIZE == 0);
	ASSERT (pg_ofs (upage) == 0);
	ASSERT (ofs % PGSIZE == 0);

	while (read_bytes > 0 || zero_bytes > 0) {
		/* Do calculate how to fill this page.
		 * We will read PAGE_READ_BYTES bytes from FILE
		 * and zero the final PAGE_ZERO_BYTES bytes. */
		size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
		size_t page_zero_bytes = PGSIZE - page_read_bytes;

		/* TODO: Set up aux to pass information to the lazy_load_segment. */
		void *aux = NULL;
		if (!vm_alloc_page_with_initializer (VM_ANON, upage,
					writable, lazy_load_segment, aux))
			return false;

		/* Advance. */
		read_bytes -= page_read_bytes; 
		zero_bytes -= page_zero_bytes;
		upage += PGSIZE;
	}
	return true;
}

/* Create a PAGE of stack at the USER_STACK. Return true on success. */
static bool setup_stack (struct intr_frame *if_) {
	bool success = false;
	void *stack_bottom = (void *) (((uint8_t *) USER_STACK) - PGSIZE);

	/* TODO: Map the stack on stack_bottom and claim the page immediately.
	 * TODO: If success, set the rsp accordingly.
	 * TODO: You should mark the page is stack. */
	/* TODO: Your code goes here */

	return success;
}
#endif /* VM */
