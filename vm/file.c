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

	page->file.zeroed_start = -1; // 일단은 -1로 초기화, 만약 zero가 됐다면 -1이 아니겠지

	page->file.mapped_file = NULL;

	page->file.offset = NULL; 

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

/* Do the mmap 
fd로 열린 파일의 오프셋 바이트부터 시작해서 length 바이트만큼을 프로세스의 가상 주소공간인 addr에 매핑 
매핑된 가상주소 반환
*/
void *
do_mmap (void *addr, size_t length, int writable,
		struct file *file, off_t offset) {
	file = file_reopen(file); // file_reopen : 인자로 받은 파일과 동일한 inode에 대한 새 파일을 연다

	struct page* cur_page = spt_find_page(&thread_current()->spt, pg_round_up(addr));
	cur_page->file.mapped_file = file;   // 시작점(addr)에 해당하는 페이지에 매핑된 파일을 기록함
	cur_page->file.offset	   = offset; // 매핑된 파일의 시작점

	// 읽으려는 바이트
	size_t read_bytes = file_length(file) < length ? file_length(file) : length; // file의 길이가 length보다 크면, 읽고자 했던 length만큼만 읽는다

	// 0으로 채워줄 바이트 : 페이지에서 read_byte만큼을 채우고, 남은 부분이 있다면 0으로 채운다
	size_t zero_bytes = PGSIZE - (read_bytes % PGSIZE); 

	/* addr에 파일의 컨텐츠 옮기기 */
	// 파일의 오프셋을 offset으로 바꿔야 함
	file_seek(file, offset);
	
	void* addr_middleman = addr;
	int read_count; // file_read의 반환값 받기 위해 존재

	// 읽으려는 바이트가 0 이 될때까지 addr_middleman 에 복사하기
	while (read_bytes > 0) {
		cur_page = spt_find_page(&thread_current()->spt, pg_round_up(addr_middleman));
		cur_page->file.mapped_file = file;

		// 최대 페이지 사이즈만큼 복사해줘야 함
		size_t copy_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
		read_bytes -= copy_bytes;

		// read syscall의 일부분
		lock_acquire(&filesys_lock);
        read_count = file_read(file, addr_middleman, copy_bytes); // 파일을 읽어 버퍼에 넣음
        lock_release(&filesys_lock);

		// 복사해준 길이만큼 주소를 밑으로 내려야 함 
		addr_middleman -= copy_bytes;
	}

	// 나중에 디스크에 다시 쓸때, zero_byte로 채운 페이지의 여분은 버려야 하기 때문에, 얘를 페이지 구조체에 기록
	// 근데 round up인지 down인지 모르겠음, 다른 메모리들도 스택처럼 위에서 밑으로 길러지나?
	cur_page = spt_find_page(&thread_current()->spt, pg_round_up(addr_middleman));

	cur_page->file.zeroed_start = addr_middleman; 
	// 지수님 말씀 : 얘는 lazy_load에서 제로로 채워준다???????

	// 0으로 채워주려는 바이트만큼 0으로 채우기
	memset(addr_middleman, 0, zero_bytes);
	
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

	// mapped_file->pos = offset; // offset부터 변경된 값들을 복사해 넣을 것
	file_seek(mapped_file, offset);

	void *addr_middleman = addr;

	/* 페이지의 값들을 파일에 돌려놓아야 함*/
	while (true) {
		cur_page = spt_find_page(&thread_current()->spt, pg_round_up(addr));
		cur_page->file.mapped_file = NULL; // 매핑 해제

		if (cur_page->file.zeroed_start != -1) {
			// zero의 시작점까지만 읽어줌
			// memcpy(mapped_file->pos, addr_middleman, addr_middleman - cur_page->file.zeroed_start);
			file_write(mapped_file, addr_middleman, addr_middleman - cur_page->file.zeroed_start);
			break;
		}
		
		file_write(mapped_file->pos, addr_middleman, PGSIZE);
		addr_middleman -= PGSIZE;
		offset -= PGSIZE;
		file_seek(mapped_file, offset);
	}
}
