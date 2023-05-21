/* anon.c: Implementation of page for non-disk image (a.k.a. anonymous page). */

#include "vm/vm.h"
#include "devices/disk.h"
#include "include/lib/kernel/bitmap.h"

/* 스왑디스크에서 사용 가능한 영역과, 사용된 영역을 관리 : 스왑 영역은 PGSIZE 단위로 관리 */
struct bitmap *swap_table;
int bitcnt; // ??
const size_t SECTORS_PER_PAGE = PGSIZE / DISK_SECTOR_SIZE;
/* 섹터 : 하드 드라이브의 최소 기억 단위 - 8섹터당 1페이지이다. 스왑영역(각 섹터들)을 페이지 사이즈 단위로 관리하기 위함 */

/* DO NOT MODIFY BELOW LINE */
static struct disk *swap_disk;
static bool anon_swap_in (struct page *page, void *kva);
static bool anon_swap_out (struct page *page);
static void anon_destroy (struct page *page);

/* DO NOT MODIFY this struct */
static const struct page_operations anon_ops = {
	.swap_in = anon_swap_in,
	.swap_out = anon_swap_out,
	.destroy = anon_destroy,
	.type = VM_ANON,
};

/* Initialize the data for anonymous pages 
- anonymous page를 위한 디스크 내 스왑 공간 생성

*/
void
vm_anon_init (void) {
	/* TODO: Set up the swap_disk. */
	swap_disk = disk_get(1, 1); // -> 인자로 1, 1 이 들어가면 swap 영역을 반환받음
	size_t swap_size = disk_size(swap_disk) / SECTORS_PER_PAGE;
	swap_table = bitmap_create(swap_size);
}

/* Initialize the file mapping */
bool
anon_initializer (struct page *page, enum vm_type type, void *kva) {
    // new anon page: uninit 필드를 0으로 초기화
    // struct uninit_page *uninit = &page->uninit;
    // memset(uninit, 0, sizeof(struct uninit_page));
	/* anon으로 바꿀때 여기서 0으로 세팅(VM_UNINIT)해줘서 문제 생긴거!! */
    // /* Set up the handler */
    page->operations = &anon_ops;
    struct anon_page *anon_page = &page->anon;
}

/* Swap in the page by read contents from the swap disk. */
static bool
anon_swap_in (struct page *page, void *kva) {
	struct anon_page *anon_page = &page->anon; // 페이지를 anon 유형으로 변경

	int page_no = anon_page->swap_index; 

	if (bitmap_test(swap_table, page_no) == false) {
		return false;
	}

	// 디스크의 값을 물리 프레임에 다시 적재한다
	for (int i = 0; i < SECTORS_PER_PAGE; ++i) {
		disk_read(swap_disk, page_no * SECTORS_PER_PAGE + i, kva + DISK_SECTOR_SIZE * i);
	}

	bitmap_set(swap_table, page_no, false);

	return true;
}

/* Swap out the page by writing contents to the swap disk. 
- anonymous 페이지를 디스크 내부의 swap 공간으로 내림
*/
static bool
anon_swap_out (struct page *page) {
	struct anon_page *anon_page = &page->anon;

	// 비트맵을 순회해 false(해당 swap slot이 비어있는) 비트를 찾음
	int page_no;
	if ((page_no = bitmap_scan(swap_table, 0, 1,false)) == BITMAP_ERROR) {
		return false;
	} 

	// 해당 섹터에 페이지 크기만큼 써줘야 하니, 필요한 섹터수만큼을 disk_write()를 통해서 입력해줌
	for (int i = 0; i < SECTORS_PER_PAGE; ++i) {
		disk_write(swap_disk, page_no * SECTORS_PER_PAGE + i ,page->va + DISK_SECTOR_SIZE * i);
	} 

	// write 작업이 끝나서 해당 스왑 공간에 페이지가 채워졌으니, bitmap_set()으로 slot이 찼다고 표시 
	bitmap_set(swap_table ,page_no, true);

	// pml4_clear_page()로 물리 프레임에 올라와있던 페이지를 지움 
	pml4_clear_page(thread_current()->pml4, page->va);

	// 페이지의 swap_index 값을 이 페이지가 저장된 swap slot의 번호로 저장
	anon_page->swap_index = page_no;

	return true;
}

/* Destroy the anonymous page. PAGE will be freed by the caller. */
static void
anon_destroy (struct page *page) {
	struct anon_page *anon_page = &page->anon;
}
