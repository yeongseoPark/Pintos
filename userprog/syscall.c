#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/loader.h"
#include "userprog/gdt.h"
#include "threads/flags.h"
#include "intrinsic.h"

void syscall_entry (void);
void syscall_handler (struct intr_frame *);

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

void
syscall_init (void) {
	write_msr(MSR_STAR, ((uint64_t)SEL_UCSEG - 0x10) << 48  |
			((uint64_t)SEL_KCSEG) << 32);
	write_msr(MSR_LSTAR, (uint64_t) syscall_entry);

	/* The interrupt service rountine should not serve any interrupts
	 * until the syscall_entry swaps the userland stack to the kernel
	 * mode stack. Therefore, we masked the FLAG_FL. */
	write_msr(MSR_SYSCALL_MASK,
			FLAG_IF | FLAG_TF | FLAG_DF | FLAG_IOPL | FLAG_AC | FLAG_NT);
}

/* The main system call interface */
void
syscall_handler (struct intr_frame *f UNUSED) {
	// TODO: Your implementation goes here.
	printf ("system call!\n");
	thread_exit ();
}

/*----------- project2 : user memory access --------------*/
// 주소값이 유저 영역에서 사용하는 주소값인지 확인, 유저 영역을 벗어난 영역인경우 프로세스를 종료(exit(-1))
// 잘못된 메모리에 대한 접근을 막음
/* 
	1. Null pointer이거나
	2. KERN_BASE 보다 큰 값 : kernel VM을 가리킬때
	3. 유저 영역을 가리키지만, 아직 할당되지 않음
*/
void check_address(void *addr) {
	if (&addr > KERN_BASE) { // 유저 어드레스는 KERN_BASE 밑이여야 함
		// free(addr);
		struct thread *cur = thread_current();
		if (addr == NULL || is_kernel_vaddr(addr) || pml4_get_page(cur->pml4 ,addr) == NULL) { // pml4 는 매핑을 관리하는 페이지테이블 
			exit(1);
		}
	}	
}