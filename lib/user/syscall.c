#include <syscall.h>
#include <stdint.h>
#include "../syscall-nr.h"

// http://wiki.kldp.org/KoreanDoc/html/EmbeddedKernel-KLDP/app3.basic.html => inline asm 설명
__attribute__((always_inline))
static __inline int64_t syscall (uint64_t num_, uint64_t a1_, uint64_t a2_,
		uint64_t a3_, uint64_t a4_, uint64_t a5_, uint64_t a6_) {
	int64_t ret;
	// 각 레지스터에 인자값 저장
	register uint64_t *num asm ("rax") = (uint64_t *) num_;
	register uint64_t *a1 asm ("rdi") = (uint64_t *) a1_;
	register uint64_t *a2 asm ("rsi") = (uint64_t *) a2_;
	register uint64_t *a3 asm ("rdx") = (uint64_t *) a3_;
	register uint64_t *a4 asm ("r10") = (uint64_t *) a4_;
	register uint64_t *a5 asm ("r8") = (uint64_t *) a5_;
	register uint64_t *a6 asm ("r9") = (uint64_t *) a6_;

	// __ams: 인라인 어셈블리, __volatile : 컴파일러의 최적화 방지
	__asm __volatile(
		/* 어셈블리어들: %0, %1 등을 사용해서 input, output 오퍼랜드를 나타냄.
		 output부터 시작해 input에 나열된 변수의 순서대로 %0, %1 ... 순으로 번호가 매겨짐 */
		 /* input의 파라미터들을 rax, rdi.... 등 레지스터들에 옮김 */
			"mov %1, %%rax\n" 
			"mov %2, %%rdi\n"
			"mov %3, %%rsi\n"
			"mov %4, %%rdx\n"
			"mov %5, %%r10\n"
			"mov %6, %%r8\n"
			"mov %7, %%r9\n"
			"syscall\n" // 시스템콜 호출 -> syscall-entry.s 로 진입아니고, syscall자체를 호출하는건데, 커널단에서 시스템콜을 다루는건 syscall_handler
			/* output */
			: "=a" (ret) // 결과값을 출력하는 변수가 ret, 이를 a라는 오퍼랜드에 넣는데, = 을 사용해서 쓰기 전용 오퍼랜드임을 나타냄
			/* input : 인라인 어셈블리에 넘겨주는 파라미터를 적는다 */
			: "g" (num), "g" (a1), "g" (a2), "g" (a3), "g" (a4), "g" (a5), "g" (a6) // g는 일반레지스터, 메모리 혹은, immediate 정수 중 아무것이나 나타내는 오퍼랜드
			/* clobber : output, input에 명시돼 있지는 않지만, asms를 실행해서 값이 변하는 것을 적어준다 */
			: "cc", "memory"); // memory clobber는 GCC가 메모리가 임의적으로 asm블럭에 의해 읽히거나 쓰일것이라고 가정하게 함. 따라서 컴파일러가 reorder하는 것을 막음
			// cc 클로버는 모든 asm()안에 implicit하게 들어있음, 얘는 flag와 condition 코드를 컴파일러가 미리 고려하게 함
	return ret;
}

/* Invokes syscall NUMBER, passing no arguments, and returns the
   return value as an `int'. */
#define syscall0(NUMBER) ( \
		syscall(((uint64_t) NUMBER), 0, 0, 0, 0, 0, 0))

/* Invokes syscall NUMBER, passing argument ARG0, and returns the
   return value as an `int'. */
#define syscall1(NUMBER, ARG0) ( \
		syscall(((uint64_t) NUMBER), \
			((uint64_t) ARG0), 0, 0, 0, 0, 0))
/* Invokes syscall NUMBER, passing arguments ARG0 and ARG1, and
   returns the return value as an `int'. */
#define syscall2(NUMBER, ARG0, ARG1) ( \
		syscall(((uint64_t) NUMBER), \
			((uint64_t) ARG0), \
			((uint64_t) ARG1), \
			0, 0, 0, 0))

#define syscall3(NUMBER, ARG0, ARG1, ARG2) ( \
		syscall(((uint64_t) NUMBER), \
			((uint64_t) ARG0), \
			((uint64_t) ARG1), \
			((uint64_t) ARG2), 0, 0, 0))

#define syscall4(NUMBER, ARG0, ARG1, ARG2, ARG3) ( \
		syscall(((uint64_t *) NUMBER), \
			((uint64_t) ARG0), \
			((uint64_t) ARG1), \
			((uint64_t) ARG2), \
			((uint64_t) ARG3), 0, 0))

#define syscall5(NUMBER, ARG0, ARG1, ARG2, ARG3, ARG4) ( \
		syscall(((uint64_t) NUMBER), \
			((uint64_t) ARG0), \
			((uint64_t) ARG1), \
			((uint64_t) ARG2), \
			((uint64_t) ARG3), \
			((uint64_t) ARG4), \
			0))
void halt (void) {
	syscall0 (SYS_HALT);
	NOT_REACHED ();
}

void exit (int status) {
	syscall1 (SYS_EXIT, status);
	NOT_REACHED ();
}

pid_t fork (const char *thread_name){
	return (pid_t) syscall1 (SYS_FORK, thread_name);
}

int exec (const char *file) {
	return (pid_t) syscall1 (SYS_EXEC, file);
}

int wait (pid_t pid) {
	return syscall1 (SYS_WAIT, pid);
}

bool create (const char *file, unsigned initial_size) {
	return syscall2 (SYS_CREATE, file, initial_size);
}

bool remove (const char *file) {
	return syscall1 (SYS_REMOVE, file);
}

int open (const char *file) {
	return syscall1 (SYS_OPEN, file);
}

int filesize (int fd) {
	return syscall1 (SYS_FILESIZE, fd);
}

int read (int fd, void *buffer, unsigned size) {
	return syscall3 (SYS_READ, fd, buffer, size);
}

int write (int fd, const void *buffer, unsigned size) {
	return syscall3 (SYS_WRITE, fd, buffer, size);
}

void seek (int fd, unsigned position) {
	syscall2 (SYS_SEEK, fd, position);
}

unsigned tell (int fd) {
	return syscall1 (SYS_TELL, fd);
}

void close (int fd) {
	syscall1 (SYS_CLOSE, fd);
}

int dup2 (int oldfd, int newfd){
	return syscall2 (SYS_DUP2, oldfd, newfd);
}

void *mmap (void *addr, size_t length, int writable, int fd, off_t offset) {
	return (void *) syscall5 (SYS_MMAP, addr, length, writable, fd, offset);
}

void munmap (void *addr) {
	syscall1 (SYS_MUNMAP, addr);
}

bool chdir (const char *dir) {
	return syscall1 (SYS_CHDIR, dir);
}

bool mkdir (const char *dir) {
	return syscall1 (SYS_MKDIR, dir);
}

bool readdir (int fd, char name[READDIR_MAX_LEN + 1]) {
	return syscall2 (SYS_READDIR, fd, name);
}

bool isdir (int fd) {
	return syscall1 (SYS_ISDIR, fd);
}

int inumber (int fd) {
	return syscall1 (SYS_INUMBER, fd);
}

int symlink (const char* target, const char* linkpath) {
	return syscall2 (SYS_SYMLINK, target, linkpath);
}

int mount (const char *path, int chan_no, int dev_no) {
	return syscall3 (SYS_MOUNT, path, chan_no, dev_no);
}

int umount (const char *path) {
	return syscall1 (SYS_UMOUNT, path);
}
