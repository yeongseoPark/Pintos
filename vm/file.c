/* file.c: Implementation of memory backed file object (mmaped object). */

#include "vm/vm.h"


static bool file_backed_swap_in (struct page *page, void *kva);
static bool file_backed_swap_out (struct page *page);
static void file_backed_destroy (struct page *page);

struct lock filesys_lock;

/* DO NOT MODIFY this struct */
static const struct page_operations file_ops = {
	.swap_in = file_backed_swap_in,
	.swap_out = file_backed_swap_out,
	.destroy = file_backed_destroy,
	.type = VM_FILE,
};

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
}

/* Swap out the page by writeback contents to the file. */
static bool
file_backed_swap_out (struct page *page) {
	struct file_page *file_page UNUSED = &page->file;
}

/* Destory the file backed page. PAGE will be freed by the caller. */
static void
file_backed_destroy (struct page *page) {
	struct file_page *file_page UNUSED = &page->file;
}

static bool lazy_load_segment_file (struct page *page, void *aux) {
	/* TODO: Load the segment from the file */
	/* TODO: This called when the first page fault occurs on address VA. */
	/* TODO: VA is available when calling this function. */
	struct info_aux *info_aux = (struct info_aux*)aux;

	struct file *file = info_aux->file;
	off_t ofs = info_aux->offset;
	uint32_t read_bytes = info_aux->read_bytes;
	uint32_t zero_bytes = info_aux->zero_bytes;

	/* 써야할 파일의 오프셋을 원하는 ofs값으로 옮기기 */
	file_seek (file, ofs);
	
	// 원하는 파일을 kpage(즉 물리주소)에 로드
	if (file_read(file, page->frame->kva, read_bytes) != (int) read_bytes) {
		palloc_free_page(page->frame->kva);

		return false;
	}
	else {
        // 파일을 읽어온 경우
        // 파일 쓰기 - 4kb중 파일을 쓰고 남는 부분은 0으로 채움
        memset (page->frame->kva + read_bytes, 0, zero_bytes);

        return true;
	}
}

/* Do the mmap 
fd로 열린 파일의 오프셋 바이트부터 시작해서 length 바이트만큼을 프로세스의 가상 주소공간인 addr에 매핑 
매핑된 가상주소 반환
*/
void *
do_mmap (void *addr, size_t length, int writable,
		struct file *file, off_t offset) {
	struct thread *cur_thread = thread_current();

	file = file_reopen(file); // file_reopen : 인자로 받은 파일과 동일한 inode에 대한 새 파일을 연다
	
	// 읽으려는 바이트
	size_t read_bytes = file_length(file) < length ? file_length(file) : length; // file의 길이가 length보다 크면, 읽고자 했던 length만큼만 읽는다

	// 0으로 채워줄 바이트 : 페이지에서 read_byte만큼을 채우고, 남은 부분이 있다면 0으로 채운다
	size_t zero_bytes = PGSIZE - (read_bytes % PGSIZE); 

	void* addr_middleman = addr;

	while (read_bytes > 0 || zero_bytes > 0) {
		
		struct info_aux *if_aux = (struct info_aux*)calloc(1, sizeof(struct info_aux));

		size_t copy_bytes      = read_bytes < PGSIZE ? read_bytes : PGSIZE;
		size_t page_zero_bytes = PGSIZE - read_bytes;

		if_aux->file 	   = file;
		if_aux->offset 	   = offset;
		if_aux->read_bytes = read_bytes;
		if_aux->zero_bytes = zero_bytes; 

		// 페이지 만들고 
		if (!vm_alloc_page_with_initializer(VM_FILE, addr_middleman, writable, lazy_load_segment_file, if_aux)) {
			free(addr);
			return false;
		}

		read_bytes -= copy_bytes;
		zero_bytes -= page_zero_bytes;
		offset 	   += copy_bytes;
		addr_middleman += PGSIZE;
	}

	return addr; // 성공적이면 파일이 매핑된 부분의 가상주소를 리턴
}

/* Do the munmap
- unmap을 할때 프로세스에 의해 쓰인 모든 페이지는 file에 다시 쓰인다 (zero로 채운 부분은 그렇지 않음)
 */
void
do_munmap (void *addr) {
	struct page *cur_page = spt_find_page(&thread_current()->spt, pg_round_up(addr));
	struct file *mapped_file = cur_page->file.mapped_file;
	off_t offset = cur_page->file.offset;
	cur_page->file.mapped_file = NULL; // 매핑 해제

	// mapped_file->pos = offset; // offset부터 변경된 값들을 복사해 넣을 것
	file_seek(mapped_file, offset);
	
	file_write(mapped_file->pos, cur_page->frame->kva, cur_page->file.read_bytes);

	spt_remove_page(&thread_current()->spt, cur_page);
}
