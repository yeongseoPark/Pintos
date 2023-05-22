/* file.c: Implementation of memory backed file object (mmaped object). */

#include "vm/vm.h"
#include "threads/vaddr.h"
#include "userprog/process.h"
/* Project 3-3 추가 */
#include "threads/mmu.h"

static bool file_backed_swap_in (struct page *page, void *kva);
static bool file_backed_swap_out (struct page *page);
static void file_backed_destroy (struct page *page);

/* DO NOT MODIFY this struct */
static const struct page_operations file_ops = {
	.swap_in = file_backed_swap_in,
	.swap_out = file_backed_swap_out,
	.destroy = file_backed_destroy,
	.type = VM_FILE,
};


// ******************************LINE ADDED****************************** //
struct container {
    struct file *file;
    off_t offset;
    size_t page_read_bytes;
};
// *************************ADDED LINE ENDS HERE************************* //



/* The initializer of file vm */
void
vm_file_init (void) {
}

/* Initialize the file backed page */
bool
file_backed_initializer (struct page *page, enum vm_type type, void *kva) {
	/* Set up the handler */
	page->operations = &file_ops;

	struct file_page *file_page = &page->file;
}

/* Swap in the page by read contents from the file. */
static bool
file_backed_swap_in (struct page *page, void *kva) {
	struct file_page *file_page UNUSED = &page->file;
	struct thread *t = thread_current();

	struct container *container = (struct container *)page->uninit.aux;	

	file_seek(container->file, container->offset);
	off_t read_size = file_read(container->file, kva, container->page_read_bytes);

	if (read_size != container->page_read_bytes) return false;
	if (read_size < PGSIZE) {
		memset (kva + read_size, 0, PGSIZE - read_size);
	}
	return true;
}

/* Swap out the page by writeback contents to the file. */
static bool
file_backed_swap_out (struct page *page) {
	struct file_page *file_page UNUSED = &page->file;
	struct thread *t = thread_current();

	if (page == NULL) return false;

	struct container *container = (struct container *)page->uninit.aux;		// page의 uninit 구조체로부터 접근
	if (pml4_is_dirty(t->pml4, page->va)) {
		file_write_at(container->file, page->va, container->page_read_bytes, container->offset);
		pml4_set_dirty(t->pml4, page->va, 0);
	}

	// bitmap_set(swap_table, page_no, true);
	pml4_clear_page(t->pml4, page->va);
	page->frame = NULL;

	return true;
}

/* Destory the file backed page. PAGE will be freed by the caller. */
static void
file_backed_destroy (struct page *page) {
	struct file_page *file_page UNUSED = &page->file;
}

/* Do the mmap */
void *
do_mmap (void *addr, size_t length, int writable,
		struct file *file, off_t offset) {
		//obtain a separate and independent reference to the file for each of its mappings.

	// 얼마나 읽을 것인가: 주어진 파일 길이와 length 비교: 읽고자 하는 length와 file 사이즈 중 작은 것
	struct file *file_ = file_reopen(file);
	size_t read_bytes = length > file_length(file) ? file_length(file) : length;
	size_t zero_bytes = PGSIZE - (read_bytes % PGSIZE);
	off_t offset_ = offset ;

	// 시작 주소 : 페이지 확장시 리턴 주소값 변경 방지
	void *start_addr = addr;

	if (file_ == NULL || read_bytes == 0) return NULL;

	/* 실제 가상페이지 할당하는 부분 */
	while (read_bytes > 0 || zero_bytes > 0) {	// 둘 다 0이 될 때까지 반복.
		// 파일을 페이지 단위로 잘라 container에 저장 
		size_t page_read_bytes = read_bytes > PGSIZE ? PGSIZE : read_bytes;		// 마지막에만 read_bytes
		size_t page_zero_bytes = PGSIZE - page_read_bytes;		// 중간에는 0, 마지막에 padding

		struct container *container = (struct container *)malloc(sizeof(struct container));
		container->file = file_;
		container->offset = offset_;
		container->page_read_bytes = page_read_bytes;

		// 대기중인 오브젝트 생성 - 초기화되지 않은 주어진 타입의 페이지 생성. 나중에 UNINIT을 FILE-backed로. 
		if (!vm_alloc_page_with_initializer(VM_FILE, addr, writable, lazy_load_segment, container)) {
			return NULL;
		}
		// 다음 페이지로 이동
		read_bytes -= page_read_bytes;
		zero_bytes -= page_zero_bytes;	// 마지막엔 0
		addr += PGSIZE;
		offset_ += page_read_bytes;
	}
	// 매핑 시작 주소 반환
	return start_addr;
}

/*
static bool lazy_load_file (struct page *page, void *aux) {
	
	struct container *container = (struct container *)aux;
	// aux 정보 가져오기
	struct file *file = container->file;
	off_t offset = container->offset;
	size_t page_read_bytes = container->page_read_bytes;
	size_t page_zero_bytes = PGSIZE - page_read_bytes;

	// file을 지정된 offset부터 읽는다
	file_seek(file, offset);

	if (file_read(file, page->frame->kva, page_read_bytes) != (int)page_read_bytes) {
		palloc_free_page(page->frame->kva);
	}
	memset(page->frame->kva + page_read_bytes, 0, page_zero_bytes);

	pml4_set_dirty (thread_current()->pml4, page->va, 0);  // 처음 로딩. 변경된 적 없음.

	free(container);

	return true;
}
*/

/* Do the munmap */
/* TODO
	1. 가상주소를 PGSIZE만큼 연속적으로 이동하며 (해당 page가 spt에 존재할 때까지) 디스크 파일에 반영.
	2. disk 파일을 업데이트 하기 전, 해당 va의 PTE를 찾아 dirty bit가 1인 경우만 업데이트 함으로써 효율성 높임.
	3. 변경 여부와 관계없이 pml4 PTE에 not_present로 표시하기. 
*/
void
do_munmap (void *addr) {
	struct thread *t = thread_current();
	// traverse 
	while (true) {
		struct page *page = spt_find_page(&t->spt, addr);

		if (page == NULL)
			return;

		// 파일이 수정된 후 다시 수정될 수 있도록 aux를 받아옴
		// Q. container에 언제 넣어줬나?
		struct container *container = (struct container *)page->uninit.aux;

		// 수정된 페이지(dirty bit == 1)는 업데이트 해둔다. 이후 dirty bit 0으로.
		if (pml4_is_dirty(t->pml4, page->va)) {
			// 수정된 파일 다시 쓰기 
			// Writes SIZE bytes from BUFFER into FILE
			file_write_at(container->file, addr, container->page_read_bytes, container->offset);

			pml4_set_dirty(t->pml4, page->va, 0);			
		}

		// present bit를 0으로 ?
		// process exit()에서 cleanup()을 호출하여 삭제하므로 여기서 페이지를 삭제하면 TC 실패
		// pml4_clear_page(t->pml4, page->va);

		addr += PGSIZE;		
	}
}
