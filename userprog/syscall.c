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
#include "vm/vm.h"
#include "filesys/directory.h"
#include <syscall.h>

void syscall_entry (void); // syscall_entry.S를 실행하는데, 이때부터 시스템콜 코드
/* syscall_entry의 진행 
1. rbx, r12 값을 임시로 저장해두고, rsp 역시 rbx에 값을 옮겨둔다
2. 스레드를 생성할때 저장해두었던 커널 스택 공간을 가리키는 rsp값(tss에 저장돼 있음)을 꺼내서 유저의 rsp에 덮어쓴다.  -> 이때부터 커널 스택 공간으로 들어감
3. syscall_handler()의 첫번째인자(%rdi)를 만들기 위해 rsp의 intr_handler 구조체에 들어가는 순서대로 레지스터 값들을 push해줌
4. rsp 내용을 rdi로 옮겨준다 (movq %rsp %rdi)
5. syscall_handler 호출 */
void syscall_handler (struct intr_frame *);
void check_address(void *addr);

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

/* Project 4 only. */
bool chdir (const char *dir);
bool mkdir (const char *dir);
bool readdir (int fd, char name[READDIR_MAX_LEN + 1]);
bool isdir (int fd);
int inumber (int fd);
int symlink (const char* target, const char* linkpath);


// Project 2-2-2 : User Programs - System Call - File Descriptor
static struct file *find_file_by_fd(int fd);
int add_file_to_fdt(struct file *file);
void remove_file_from_fdt(int fd);

// Project 3
static void* mmap (void *addr, size_t length, int writable, int fd, off_t offset);
static void munmap (void *addr);


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
            f -> R.rax = read(f -> R.rdi, f -> R.rsi, f -> R.rdx);
            break;
        case SYS_WRITE: // Write to a file.
            f -> R.rax = write(f -> R.rdi, f -> R.rsi, f -> R.rdx);
            break;
        case SYS_SEEK: // Change position in a file.
            /*seek(f -> R.rdi, f -> R.rdx); <<<<<<<<<<< 이새끼가 범인이에요*/
            seek(f -> R.rdi, f -> R.rsi);
            break;
        case SYS_TELL: // Report current position in a file.
            f -> R.rax = tell(f -> R.rdi);
            break;
        case SYS_CLOSE: // Close a file.
            close(f -> R.rdi);
            break;
        case SYS_MMAP:
            f -> R.rax = (uint64_t) mmap((void*) f->R.rdi, (size_t) f->R.rsi, (int) f->R.rdx, (int) f->R.r10, (off_t) f->R.r8);
            break;
            // void *mmap (void *addr, size_t length, int writable, int fd, off_t offset);
        case SYS_MUNMAP:
            munmap ((void*) f->R.rdi);
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
void check_address(void *addr) {
    struct thread *curr = thread_current();
    // 체크 조건
    // user virtual address 인지 (is_user_vaddr) = 커널 VM이 아닌지
    // 주소가 NULL 은 아닌지
    // 유저 주소 영역내를 가르키지만 아직 할당되지 않았는지 (pml4_get_page)
    if (!is_user_vaddr(addr) || addr == NULL || pml4_get_page(curr->pml4, addr) == NULL){
            exit(-1);
    }
    #ifdef VM
    if (pml4_get_page (&thread_current()->pml4, addr) == NULL) {    // pml4로 매핑되어 있는 페이지가 있는지 확인
        if (spt_find_page(&thread_current()->spt, addr) == NULL)    // 매핑되어 있는 페이지가 없다면 대기중인 보조 페이지 테이블이 있는지 확인
            exit(-1);                  // 보조 페이지 테이블이 없다면 -1로 종료
    }
    #endif
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
    // check_address(thread_name);
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
 * file_ = file이 되고, 이를 현재 스레드 파일 디스크립터 테이블에 추가하여 관리할 수 있게함*/
int open(const char *file){
    check_address(file);

    if (file == NULL) return -1;

    lock_acquire(&filesys_lock); 

    struct file *file_ = filesys_open(file); // 파일을 열고

    if (file_ == NULL) return -1;

    int fd = add_file_to_fdt(file_); // 현재 쓰레드의 fdt에 파일을 더함

    /* if fd full?*/
    if(fd == -1){
        file_close(file_);
    }

    lock_release(&filesys_lock);
    return fd;
}

int filesize(int fd){
    struct file *file_ = find_file_by_fd(fd);

    // 파일이 열려있지 않을 경우 예외 처리
    if (file_ == NULL){
        return -1;
    }
    return file_length(file_); // inode의 data값에서 length를 뽑아냄
}

/*열린 파일의 데이터를 읽는 시스템 콜
- 파일에 동시 접근이 일어날 수 있으므로 Lock 사용
- 파일 디스크립터를 이용하여 파일 객체 검색
- 파일 디스크립터가 0일 경우 키보드에 입력을 버퍼에 저장 후, 버퍼의 저장한 크기를 리턴 (input_getc() 이용)
- 파일 디스크립터가 0이 아닐 경우 파일의 데이터를 크기만큼 저장 후 읽은 바이트 수를 리턴*/
int read(int fd, void *buffer, unsigned size){
    check_address(buffer);
    check_address(buffer+size-1);
    int read_count; // 글자수 카운트 용(for문 사용하기 위해)

    struct file *file_ = find_file_by_fd(fd);
    unsigned char *buf = buffer;

    if (file_ == NULL){
        return -1;
    }

    if(file_ == STDIN){ // STDIN - 키보드 입력을 버퍼에 넣음
        char key;
        for (int read_count = 0; read_count < size; read_count++){
            key = input_getc();
            *buf++ = key;
            if (key == '\0'){
                break;
            }
        }
    }else if (file_ == STDOUT){ // STDOUT - 출력을 읽을순 없다
        return -1;
    } else {
        lock_acquire(&filesys_lock);
        read_count = file_read(file_, buffer, size); // 파일을 읽어 버퍼에 넣음
        lock_release(&filesys_lock);
    }

    return read_count;
}

/* 버퍼의 값을 파일에 작성 */
int write(int fd, void *buffer, unsigned size){
    check_address(buffer);
    int read_count;
    struct file *file_ = find_file_by_fd(fd);

    if (file_ == NULL){
        return -1;
    }

    if(file_ == STDOUT){ // STDOUT
        putbuf(buffer, size); // fd값이 1일 때, 버퍼에 저장된 데이터를 화면에 출력(putbuf()이용)
        read_count = size;
    } else if (file_ == STDIN){ // STDIN
        return -1;
    } else{
        lock_acquire(&filesys_lock);
        read_count = file_write(file_, buffer, size);
        lock_release(&filesys_lock);
    }
    return read_count;
}

void seek(int fd, unsigned position){
    struct file *file_ = find_file_by_fd(fd);
    if (fd < 2){
        return;
    }
    file_seek(file_, position); // 파일에서의 위치를 position으로 변경
}

unsigned tell(int fd){
    struct file *file_ = find_file_by_fd(fd);
    if (fd < 2){
        return;
    }
    return file_tell(file_); // 현재 pos를 찾음
}

void close(int fd){
    if (fd <= 1) return;
    struct file *file_ = find_file_by_fd(fd);

    if (file_ == NULL){
        return;
    }
    remove_file_from_fdt(fd);
}
// *************************ADDED LINE ENDS HERE************************* //



// ******************************LINE ADDED****************************** //
/* Project 3-3 : Virtual Memory - Memory Mapped files
 * 매핑하려는 주소 & 읽으려는 파일이 유효한가?
 * 읽을 파일의 offset이 왜 align 해야하지? 
 */
static void*
mmap (void *addr, size_t length, int writable, int fd, off_t offset) {

    struct file *file = find_file_by_fd(fd);        // 앞에서 open 시스템 콜로 fd 리턴한 것을 받아서
    struct thread *t = thread_current();

    // addr is not page-aligned
    if ((uint64_t)addr % PGSIZE != 0) return NULL;

    // overlaps existing set of mapped pages
    for (uint64_t i = (uint64_t)addr; i < (uint64_t)addr + length; i += PGSIZE) {
        if (spt_find_page(&t->spt, (void*)i) != NULL) return NULL;
    }

    // addr == 0 -> fail, 핀토스는 virtual page 0를 'not mapped'로 간주
    if (addr == 0) return NULL;

    // length is zero
    if (length == 0 || file == NULL) return NULL;

    // fd = 0, 1(console input/output)
    if (fd == 0 || fd == 1) return NULL;

    return do_mmap(addr, length, writable, file, offset);
}


static void 
munmap (void *addr) {
    do_munmap(addr);
}


// *************************ADDED LINE ENDS HERE************************* //



// ************************* P4-2 추가 ************************* //
bool chdir(const char *name)        // name: new dir path
{
    struct thread *curr = thread_current();

    char *copy_name = (char *)malloc(strlen(name) + 1);  // 메모리 할당 
    if (copy_name == NULL) return false;

    strlcpy(copy_name, name, strlen(name) + 1);     // 문자열 복사
    if (strlen(copy_name) == 0) return false;

    struct dir *dir;

    if (copy_name[0] == '/')        // '/' -> 절대 경로
        dir = dir_open_root();      // inode_open(root), root = #define ROOT_DIR_CLUSTER 1    /* Cluster for the root directory */
    else
        dir = dir_reopen(curr->curr_dir); // 상대경로: 현재 스레드의 dir를 연다- dir * 리턴. 

    /* 새로운 path명을 경로로 파싱해, 해당 dir을 inode로 지정, dir_open(inode) */
    char *token, *save_ptr;
    token = strtok_r(copy_name, "/", &save_ptr);    // 구분 기호로 '/'가 포함된 strtok_r을 사용하여 토큰화. 경로가 개별 디렉토리 또는 파일 이름으로 분할

    struct inode *inode = NULL; // 디렉토리 또는 파일의 inode를 저장하는 데 사용

    while (token != NULL) {    
        if (!dir_lookup(dir, token, &inode))  // @@@@@@ token: dir에서 파일명(token)에 해당하는 inode를 찾아 &inode에 저장. On success, sets *INODE to an inode for the file / &inode 역참조로 이미 값 할당했음
            goto fail;  
        if (!inode_is_dir(inode))
            goto fail;

        dir_close(dir); // 현재 디렉토리(dir)는 dir_close를 사용하여 닫고
        dir = dir_open(inode); // @@@@@@ dir_lookp에서 얻은 inode는 dir_open을 사용하여 새 디렉토리를 여는 데 사용
    
        token = strtok_r(NULL, "/", &save_ptr); // path 끝까지- dir인 한 계속 폴더를 열어 'dir'에 저장
    }

    dir_close(curr->curr_dir);  // 모든 토큰 처리를 완료하면 curr_dir를 닫고
    curr->curr_dir = dir;       // @@@@@@ 새 디렉토리(dir)가 curr->curr_dir에 할당
    free(copy_name);

    return true;

fail:
    dir_close(dir);
    if (inode)
        inode_close(inode); // 연 건 다 닫고 false 처리

    return false;
}




bool mkdir (const char *dir)
{
    char *copy_dir = (char *)malloc(strlen(dir) + 1);
    if (copy_dir == NULL) return false;

    strlcpy(copy_dir, dir, strlen(dir) + 1); // strlcpy (*dest, *src, size)

    lock_acquire(&filesys_lock);
    bool succ = filesys_create_dir(copy_dir);
    lock_release(&filesys_lock);

    free(copy_dir);

    return succ;
}