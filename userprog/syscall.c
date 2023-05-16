#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/loader.h"
#include "userprog/gdt.h"
#include "threads/flags.h"
#include "intrinsic.h"
// ******************************LINE ADDED****************************** //
#include "filesys/filesys.h"
#include "filesys/file.h"
#include "threads/vaddr.h"
#include "userprog/process.h"
#include "threads/palloc.h"

void syscall_entry (void); // syscall_entry.S를 실행하는데, 이때부터 시스템콜 코드
/* syscall_entry의 진행 
1. rbx, r12 값을 임시로 저장해두고, rsp 역시 rbx에 값을 옮겨둔다
2. 스레드를 생성할때 저장해두었던 커널 스택 공간을 가리키는 rsp값(tss에 저장돼 있음)을 꺼내서 유저의 rsp에 덮어쓴다.  -> 이때부터 커널 스택 공간으로 들어감
3. syscall_handler()의 첫번째인자(%rdi)를 만들기 위해 rsp의 intr_handler 구조체에 들어가는 순서대로 레지스터 값들을 push해줌
4. rsp 내용을 rdi로 옮겨준다 (movq %rsp %rdi)
5. syscall_handler 호출 */
void syscall_handler (struct intr_frame *);
struct page* check_address(void *addr);
void check_valid_buffer(void* buffer, unsigned size, void* rsp, bool to_write);

// Project 2-2-1: User Programs - System Call - Basics
void halt(void);
void exit(int status);
bool create (const char *file, unsigned initial_size);
bool remove(const char *file);
tid_t fork (const char *thread_name, struct intr_frame *f);
int exec(char *file_name);
int wait(tid_t pid);
int open(const char *file);
int filesize(int fd);
int read(int fd, void *buffer, unsigned size);
int write(int fd, void *buffer, unsigned size);
void seek(int fd, unsigned position);
unsigned tell(int fd);
void close(int fd);

// Project 2-2-2 : User Programs - System Call - File Descriptor
static struct file *find_file_by_fd(int fd);
int add_file_to_fdt(struct file *file);
void remove_file_from_fdt(int fd);

const int STDIN = 1;
const int STDOUT = 2;
// *************************ADDED LINE ENDS HERE************************* //


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

void syscall_init (void) {
    // MSR: Machine_specific Register - syscall은 MSR의 값을 읽어서 작동한다
	write_msr(MSR_STAR, ((uint64_t)SEL_UCSEG - 0x10) << 48  | ((uint64_t)SEL_KCSEG) << 32); // code segment와 stack segment를 로드할때쓴느 레지스터 
	write_msr(MSR_LSTAR, (uint64_t) syscall_entry); // MSR_LSTAR는 시스템 콜 핸들러 함수의 주소를 저장

	/* The interrupt service rountine should not serve any interrupts
	 * until the syscall_entry swaps the userland stack to the kernel
	 * mode stack. Therefore, we masked the FLAG_FL.
     * MSR_SYSCALL_MASK에 인터럽트 처리시 레지스터 상태를 저장
     * 시스템콜 호출시 유저모드 -> 커널 모드 전환 과정에서 스택이 변경되는데, 커널모드에서 실행되야 하는 중요한 작업이 수행전까지는
     * 인터럽트를 처리하지 않기 위해서, 인터럽트 서비스 루틴은 syscall_entry가 유저 모드 스택을 커널 모드 스택으로 교체 전까지
     * 어떠한 인터럽트도 처리하지 않게 FLAG_FL을 마스킹함
     *  */
	write_msr(MSR_SYSCALL_MASK, FLAG_IF | FLAG_TF | FLAG_DF | FLAG_IOPL | FLAG_AC | FLAG_NT);

    // ******************************LINE ADDED****************************** //
    // Project 2-2-2 : User Programs - System Call - File Descriptor
    /* 파일 사용 시 lock 초기화 */
    lock_init(&filesys_lock);
    // *************************ADDED LINE ENDS HERE************************* //
}

/* The main system call interface */
void syscall_handler (struct intr_frame *f UNUSED) {
	// TODO: Your implementation goes here.
    // ******************************LINE ADDED****************************** //
    // Project 2-2-1: User Programs - System Call - Basics
    // Defined as enum @ include/lib/syscall-nr.h
    int sys_number = f -> R.rax; // 유저 스택에 저장되어 있는 시스템 콜 번호(rax)를 sys_number 변수에 저장
    // switch 문 통해 해당되는 시스템 콜 호출
    // REFERENCE in include/lib/syscall-nr.h
    switch(sys_number){
        case SYS_HALT: // Halt the operating system.
            halt();
            break;
        case SYS_EXIT: // Terminate this process.
            exit(f -> R.rdi);
            break;
        case SYS_FORK: // Clone current process. (자식 프로세스 생성)
            f -> R.rax = fork(f -> R.rdi, f);
            break;
        case SYS_EXEC: // Switch current process.
            if(exec(f -> R.rdi) == -1){
                exit(-1);
            }
            break;
        case SYS_WAIT: // Wait for a child process to die. (자식 프로세스 종료 대기)
            f -> R.rax = wait(f -> R.rdi);
            break;
        case SYS_CREATE: // Create a file.
            f -> R.rax = create(f -> R.rdi, f -> R.rsi);
            break;
        case SYS_REMOVE: // Delete a file.
            f -> R.rax = remove(f -> R.rdi);
            break;
        case SYS_OPEN: // Open a file.
            f -> R.rax = open(f -> R.rdi);
            break;
        case SYS_FILESIZE: // Obtain a file's size.
            f -> R.rax = filesize(f -> R.rdi);
            break;
        case SYS_READ: // Read from a file.
            // check_valid_buffer(f->R.rsi, f->R.rdx, f->rsp, 1);
            f -> R.rax = read(f -> R.rdi, f -> R.rsi, f -> R.rdx);
            break;
        case SYS_WRITE: // Write to a file.
            // check_valid_buffer(f->R.rsi, f->R.rdx, f->rsp, 0);
            f -> R.rax = write(f -> R.rdi, f -> R.rsi, f -> R.rdx);
            break;
        case SYS_SEEK: // Change position in a file.
            seek(f -> R.rdi, f -> R.rsi);
            break;
        case SYS_TELL: // Report current position in a file.
            f -> R.rax = tell(f -> R.rdi);
            break;
        case SYS_CLOSE: // Close a file.
            close(f -> R.rdi);
            break;
        default:
            /*printf ("system call!\n");
            thread_exit ();*/
            exit(-1);
            break;
    }
    // *************************ADDED LINE ENDS HERE************************* //

    // default 분기에서 처리해줌
    /*printf ("system call!\n");
    thread_exit ();*/

}

// ******************************LINE ADDED****************************** //
// Project 2-2-1: User Programs - System Call - Basics
// 주소 체크 예외 처리 함수
struct page* check_address(void *addr) {
    struct thread *curr = thread_current();
    // 체크 조건
    // user virtual address 인지 (is_user_vaddr) = 커널 VM이 아닌지
    // 주소가 NULL 은 아닌지
    // 유저 주소 영역내를 가르키지만 아직 할당되지 않았는지 (pml4_get_page)
    if(!is_user_vaddr(addr) || addr == NULL){
		exit(-1);
	}

	#ifdef VM
	// pml4_get_page()를 호출하여 주소에 pml4로 매핑되어 있는 페이지가 있는지 확인
	if(pml4_get_page (&thread_current()->pml4, addr) == NULL)
		// 매핑되어 있는 페이지가 없다면 대기중인 보조 페이지 테이블이 있는지 확인
		if (spt_find_page(&thread_current()->spt, addr) == NULL)
			// 보조 페이지 테이블이 없다면 -1로 종료
			exit(-1);
	#endif
}

/* 안씀 */
void check_valid_buffer(void* buffer, unsigned size, void* rsp, bool to_write) {
    for (int i = 0; i < size; i++) {
        struct page* page = check_address(buffer + i);    // 인자로 받은 buffer부터 buffer + size까지의 크기가 한 페이지의 크기를 넘을수도 있음
        if(page == NULL)
            exit(-1);
        if(to_write == true && page->writable == false)
            exit(-1);
    }
}

// Project 2-2-1: User Programs - System Call - Basics
// SYS_HALT : Halt the operating system 함수 구현
// 호출 시 핀토스를 종료시키는 함수
void halt(void){
    power_off();
}

// SYS_EXIT : Terminate this process 함수 구현
// 돌고 있는 프로세스를 종료시킴
void exit(int status){
    struct thread *curr = thread_current();
    curr->exit_status = status;
    // TODO : 스레드 구조체 exit_status 플래그 변수 만들기 (종료 상태 0) -> DONE
    printf ("%s: exit(%d)\n", thread_name(), status); // process termination message
    thread_exit(); //@thread.c
}

// SYS_CREATE : Create a file 함수 구현. 성공시 TRUE 반환, 실패시 FALSE 반환
bool create (const char *file, unsigned initial_size) {
    check_address(file);
    if (filesys_create(file, initial_size)) {
        return true;
    } else {
        return false;
    }
    /*return filesys_create(file, initial_size);*/
}

// SYS_REMOVE : Delete a file 함수 구현. 성공시 TRUE 반환, 실패시 FALSE 반환
bool remove (const char *file) {
    check_address(file);
    return filesys_remove(file);
    /*if (filesys_remove(file)) {
        return true;
    } else {
        return false;
    }*/
}

// ******************************LINE ADDED****************************** //
// System Call : fork(), exec(), wait()

// SYS_FORK : Clone current process. (자식 프로세스 생성)
tid_t fork (const char *thread_name, struct intr_frame *f) {
    check_address(thread_name);
    return process_fork(thread_name, f);
}

// SYS_WAIT : Wait for a child process to die. (자식 프로세스 종료 대기)
int wait(tid_t pid){
    return process_wait(pid);
}

// SYS_EXEC : Switch current process.
tid_t exec(char *file_name){ // 현재 프로세스를 command line에서 지정된 인수를 전달하여 이름이 지정된 실행 파일로 변경
    check_address(file_name);
    int size = strlen(file_name) + 1;
    char *fn_copy = palloc_get_page(PAL_ZERO);
    if (fn_copy == NULL){
        exit(-1);
    }
    strlcpy (fn_copy, file_name, size);

    if (process_exec(fn_copy) == -1){
        return -1;
    }
    NOT_REACHED();
    return 0;
}
// *************************ADDED LINE ENDS HERE************************* //


// ******************************LINE ADDED****************************** //
// Project 2-2-2 : User Programs - System Call - File Descriptor
static struct file *find_file_by_fd(int fd) {
    if (fd < 0 || fd >= FDCOUNT_LIMIT){
        return NULL;
    }

    struct thread *curr = thread_current();

    return curr->fd_table[fd];
}

// 파일 구조체 File Descriptor 테이블에 추가
// 현재 스레드의 fd_idx(File Descriptor table index 끝값)
int add_file_to_fdt(struct file *file) {
    struct thread *curr = thread_current();
    struct file **fdt = curr->fd_table;

    // Find open spot from the front
    while (curr->fd_idx < FDCOUNT_LIMIT && fdt[curr->fd_idx])
    {
        curr->fd_idx++;
    }

    // error - fd table full
    if (curr->fd_idx >= FDCOUNT_LIMIT)
        return -1;

    fdt[curr->fd_idx] = file;
    return curr->fd_idx;
}

void remove_file_from_fdt(int fd) {
    struct thread *cur = thread_current();
    if (fd < 0 || fd >= FDCOUNT_LIMIT){
        return;
    }
    cur->fd_table[fd] = NULL;
}

/* open(file) -> filesys_open(file) -> file_open(inode) -> file open 함수 실행 -> filesys_open
 * 을 통해서 file open 함수에 inode를 넣고 실행하여 file을 반환받음
 * inode는 우리가 입력한 파일 이름을 컴퓨터가 알고 있는 파일이름으로 바꾸는 과정 - link file과 file system의 연결고리
 * file_obj = file이 되고, 이를 현재 스레드 파일 디스크립터 테이블에 추가하여 관리할 수 있게함*/
int open(const char *file){
    check_address(file);
    lock_acquire(&filesys_lock); 

    struct file *file_obj = filesys_open(file); // 파일을 열고

    if (file_obj == NULL){
        return -1;
    }

    int fd = add_file_to_fdt(file_obj); // 현재 쓰레드의 fdt에 파일을 더함

    /* if fd full?*/
    if(fd == -1){
        file_close(file_obj);
    }

    lock_release(&filesys_lock);
    return fd;
}

int filesize(int fd){
    struct file *file_obj = find_file_by_fd(fd);

    // 파일이 열려있지 않을 경우 예외 처리
    if (file_obj == NULL){
        return -1;
    }
    return file_length(file_obj); // inode의 data값에서 length를 뽑아냄
}

/*열린 파일의 데이터를 읽는 시스템 콜
- 파일에 동시 접근이 일어날 수 있으므로 Lock 사용
- 파일 디스크립터를 이용하여 파일 객체 검색
- 파일 디스크립터가 0일 경우 키보드에 입력을 버퍼에 저장 후, 버퍼의 저장한 크기를 리턴 (input_getc() 이용)
- 파일 디스크립터가 0이 아닐 경우 파일의 데이터를 크기만큼 저장 후 읽은 바이트 수를 리턴*/
int read(int fd, void *buffer, unsigned size){
    check_address(buffer);
    // check_address(buffer+size-1);
    int read_count; // 글자수 카운트 용(for문 사용하기 위해)

    struct file *file_obj = find_file_by_fd(fd);
    unsigned char *buf = buffer;

    if (file_obj == NULL){
        return -1;
    }

    if(file_obj == STDIN){ // STDIN - 키보드 입력을 버퍼에 넣음
        char key;
        for (int read_count = 0; read_count < size; read_count++){
            key = input_getc();
            *buf++ = key;
            if (key == '\0'){
                break;
            }
        }
    }else if (file_obj == STDOUT){ // STDOUT - 출력을 읽을순 없다
        return -1;
    } else {
        lock_acquire(&filesys_lock);
        read_count = file_read(file_obj, buffer, size); // 파일을 읽어 버퍼에 넣음
        lock_release(&filesys_lock);
    }

    return read_count;
}

/* 버퍼의 값을 파일에 작성 */
int write(int fd, void *buffer, unsigned size){
    check_address(buffer);
    int read_count;
    struct file *file_obj = find_file_by_fd(fd);

    if (file_obj == NULL){
        return -1;
    }

    if(file_obj == STDOUT){ // STDOUT
        putbuf(buffer, size); // fd값이 1일 때, 버퍼에 저장된 데이터를 화면에 출력(putbuf()이용)
        read_count = size;
    } else if (file_obj == STDIN){ // STDIN
        return -1;
    } else{
        lock_acquire(&filesys_lock);
        read_count = file_write(file_obj, buffer, size);
        lock_release(&filesys_lock);
    }
    return read_count;
}

void seek(int fd, unsigned position){
    struct file *file_obj = find_file_by_fd(fd);
    if (fd < 2){
        return;
    }
    file_seek(file_obj, position); // 파일에서의 위치를 position으로 변경
}

unsigned tell(int fd){
    struct file *file_obj = find_file_by_fd(fd);
    if (fd < 2){
        return;
    }
    return file_tell(file_obj); // 현재 pos를 찾음
}

void close(int fd){
    if (fd <= 1) return;
    struct file *file_obj = find_file_by_fd(fd);

    if (file_obj == NULL){
        return;
    }
    remove_file_from_fdt(fd);
}
// *************************ADDED LINE ENDS HERE************************* //

