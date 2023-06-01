/* anon.c: Implementation of page for non-disk image (a.k.a. anonymous page). */

#include "vm/vm.h"
#include "devices/disk.h"
#include "vm/anon.h"
#include <bitmap.h>
         
/* DO NOT MODIFY BELOW LINE */
static struct disk *swap_disk;
static bool anon_swap_in (struct page *page, void *kva);
static bool anon_swap_out (struct page *page);
static void anon_destroy (struct page *page);

/* P3 추가 */
const int SECTORS_PER_PAGE = 8; // 4kB / 512 (DISK_SECTOR_SIZE)
struct bitmap *swap_table;	   // 0 - empty, 1 - filled

#define INVALID_SLOT_IDX SIZE_MAX

/* DO NOT MODIFY this struct */
static const struct page_operations anon_ops = {
	.swap_in = anon_swap_in,
	.swap_out = anon_swap_out,
	.destroy = anon_destroy,
	.type = VM_ANON,
};

/* Initialize the data for anonymous pages */
void
vm_anon_init (void) {
	/* TODO: Set up the swap_disk. */
	swap_disk = disk_get(1, 1);
	size_t bitcnt = disk_size(swap_disk)/SECTORS_PER_PAGE; // one bit for each swap slot in the disk
	swap_table = bitmap_create(bitcnt);
}

/* Initialize the file mapping
- VM_ANON을 초기화하는 함수 */
bool
anon_initializer (struct page *page, enum vm_type type, void *kva) {
	// new anon page: uninit 필드를 0으로 초기화
	// struct uninit_page *uninit = &page->uninit;
	// memset(uninit, 0, sizeof(struct uninit_page));  -> 계속 uninit 타입으로 만들어져서 안됨..

	/* Set up the handler */
	page->operations = &anon_ops;
	struct anon_page *anon_page = &page->anon;

	// anon_page->owner = thread_current();
	anon_page->swap_index = INVALID_SLOT_IDX;	// &page->anon->swap_index를 설정함.
	return true;
}

/* Swap in the page by read contents from the swap disk. */
static bool
anon_swap_in (struct page *page, void *kva) {
	struct anon_page *anon_page = &page->anon;
	int page_no = anon_page->swap_index;

	if (anon_page->swap_index == INVALID_SLOT_IDX) return false;
	if (bitmap_test(swap_table, page_no) == false) return false;

	for (int i = 0; i < SECTORS_PER_PAGE; ++i) {
		disk_read(swap_disk, page_no * SECTORS_PER_PAGE + i, kva + DISK_SECTOR_SIZE * i);
	}
	bitmap_set(swap_table, page_no, false);
	return true;
}

/* Swap out the page by writing contents to the swap disk. */
static bool
anon_swap_out (struct page *page) {
	struct anon_page *anon_page = &page->anon;

	int page_no = bitmap_scan(swap_table, 0, 1, false);
	if (page_no == BITMAP_ERROR) return NULL;

	for (int i = 0; i < SECTORS_PER_PAGE; ++i) {
		// Convert swap slot index to writing sector number
		disk_write(swap_disk, page_no * SECTORS_PER_PAGE + i, page->va + DISK_SECTOR_SIZE * i);
	}
	bitmap_set(swap_table, page_no, true);
	pml4_clear_page(&thread_current()->pml4, page->va, 0);

	anon_page->swap_index = page_no;
	return true;
}

/* Destroy the anonymous page. PAGE will be freed by the caller. */
static void
anon_destroy (struct page *page) {
	struct anon_page *anon_page = &page->anon;
}
